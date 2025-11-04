/*
 * BW_Midi_Sequencer - MIDI Sequencer for C++
 *
 * Copyright (c) 2015-2025 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once
#ifndef BW_MIDISEQ_PROCESS_IMPL_HPP
#define BW_MIDISEQ_PROCESS_IMPL_HPP

#include <cstring>

#include "../midi_sequencer.hpp"
#include "common.hpp"



void BW_MidiSequencer::handleEvent(size_t track, const BW_MidiSequencer::MidiEvent &evt, int32_t &status)
{
    if(track == 0 && m_smfFormat < 2 && evt.type == MidiEvent::T_SPECIAL &&
       (evt.subtype == MidiEvent::ST_TEMPOCHANGE || evt.subtype == MidiEvent::ST_TIMESIGNATURE))
    {
        /* never reject track 0 timing events on SMF format != 2 */
    }
    else
    {
        if(m_trackSolo != ~static_cast<size_t>(0) && track != m_trackSolo)
            return;

        if(m_trackDisable[track])
            return;
    }

    if(m_interface->onEvent)
    {
        if(evt.data_block.size > 0)
            m_interface->onEvent(m_interface->onEvent_userData,
                                 evt.type, evt.subtype, evt.channel,
                                 getData(evt.data_block), evt.data_block.size);
        else
            m_interface->onEvent(m_interface->onEvent_userData,
                                 evt.type, evt.subtype, evt.channel,
                                 evt.data_loc, evt.data_loc_size);
    }

    if(evt.type == MidiEvent::T_SYSEX || evt.type == MidiEvent::T_SYSEX2) // Handle SysEx
    {
        m_interface->rt_systemExclusive(m_interface->rtUserData, getData(evt.data_block), evt.data_block.size);
        return;
    }

    if(evt.type == MidiEvent::T_SPECIAL)
    {
        // Special event FF
        uint_fast16_t  evtype = evt.subtype;
        size_t length = evt.data_block.size > 0 ? evt.data_block.size : static_cast<size_t>(evt.data_loc_size);
        const uint8_t *datau = evt.data_block.size > 0 ? getData(evt.data_block) : evt.data_loc;
        const char *data(length ? reinterpret_cast<const char *>(datau) : "\0\0\0\0\0\0\0\0");

        if(m_interface->rt_metaEvent) // Meta event hook
            m_interface->rt_metaEvent(m_interface->rtUserData, evtype, reinterpret_cast<const uint8_t*>(data), length);

        if(evtype == MidiEvent::ST_ENDTRACK) // End Of Track
        {
            status = -1;
            return;
        }

        if(evtype == MidiEvent::ST_TEMPOCHANGE) // Tempo change
        {
            m_tempo = m_invDeltaTicks * fraction<uint64_t>(readBEint(datau, length));
            return;
        }

        if(evtype == MidiEvent::ST_MARKER) // Meta event
        {
            // Do nothing! :-P
            return;
        }

        if(evtype == MidiEvent::ST_DEVICESWITCH)
        {
            if(m_interface->onDebugMessage)
                m_interface->onDebugMessage(m_interface->onDebugMessage_userData, "Switching another device: %.*s", length, data);
            if(m_interface->rt_deviceSwitch)
                m_interface->rt_deviceSwitch(m_interface->rtUserData, track, data, length);
            return;
        }

        // Turn on Loop handling when loop is enabled
        if(m_loopEnabled && !m_loop.invalidLoop)
        {
            if(evtype == MidiEvent::ST_LOOPSTART) // Special non-spec MIDI loop Start point
            {
                m_loop.caughtStart = true;
                return;
            }

            if(evtype == MidiEvent::ST_LOOPEND) // Special non-spec MIDI loop End point
            {
                m_loop.caughtEnd = true;
                return;
            }

            if(evtype == MidiEvent::ST_LOOPSTACK_BEGIN)
            {
                if(m_loop.skipStackStart)
                {
                    m_loop.skipStackStart = false;
                    return;
                }

                char x = data[0];
                size_t slevel = static_cast<size_t>(m_loop.stackLevel + 1);
                while(slevel >= m_loop.stack.size())
                {
                    LoopStackEntry e;
                    e.loops = x;
                    e.infinity = (x == 0);
                    e.start = 0;
                    e.end = 0;
                    m_loop.stack.push_back(e);
                }

                LoopStackEntry &s = m_loop.stack[slevel];
                s.loops = static_cast<int>(x);
                s.infinity = (x == 0);
                m_loop.caughtStackStart = true;
                return;
            }

            if(evtype == MidiEvent::ST_LOOPSTACK_END)
            {
                m_loop.caughtStackEnd = true;
                return;
            }

            if(evtype == MidiEvent::ST_LOOPSTACK_BREAK)
            {
                m_loop.caughtStackBreak = true;
                return;
            }
        }

        if(evtype == MidiEvent::ST_CALLBACK_TRIGGER)
        {
#if 0 /* Print all callback triggers events */
            if(m_interface->onDebugMessage)
                m_interface->onDebugMessage(m_interface->onDebugMessage_userData, "Callback Trigger: %02X", evt.data[0]);
#endif
            if(m_triggerHandler)
                m_triggerHandler(m_triggerUserData, static_cast<unsigned>(data[0]), track);
            return;
        }

        if(evtype == MidiEvent::ST_RAWOPL) // Special non-spec ADLMIDI special for IMF playback: Direct poke to AdLib
        {
            if(m_interface->rt_rawOPL)
                m_interface->rt_rawOPL(m_interface->rtUserData, datau[0], datau[1]);
            return;
        }

        if(evtype == MidiEvent::ST_SONG_BEGIN_HOOK)
        {
            if(m_interface->onSongStart)
                m_interface->onSongStart(m_interface->onSongStart_userData);
            return;
        }

        return;
    }

    if(evt.type == MidiEvent::T_SYSCOMSNGSEL ||
       evt.type == MidiEvent::T_SYSCOMSPOSPTR)
        return;

    size_t midCh = evt.channel;
    if(m_interface->rt_currentDevice)
        midCh += m_interface->rt_currentDevice(m_interface->rtUserData, track);

    status = evt.type;

    switch(evt.type)
    {
    case MidiEvent::T_NOTEOFF: // Note off
    {
        if(midCh < 16 && m_channelDisable[midCh])
            break; // Disabled channel
        uint8_t note = evt.data_loc[0];
        uint8_t vol = evt.data_loc[1];
        if(m_interface->rt_noteOff)
            m_interface->rt_noteOff(m_interface->rtUserData, static_cast<uint8_t>(midCh), note);
        if(m_interface->rt_noteOffVel)
            m_interface->rt_noteOffVel(m_interface->rtUserData, static_cast<uint8_t>(midCh), note, vol);
        break;
    }

    case MidiEvent::T_NOTEON: // Note on
    {
        if(midCh < 16 && m_channelDisable[midCh])
            break; // Disabled channel
        uint8_t note = evt.data_loc[0];
        uint8_t vol  = evt.data_loc[1];
        m_interface->rt_noteOn(m_interface->rtUserData, static_cast<uint8_t>(midCh), note, vol);
        break;
    }

    case MidiEvent::T_NOTEON_DURATED: // Note on with duration
    {
        if(midCh < 16 && m_channelDisable[midCh])
            break; // Disabled channel
        DuratedNote note;
        note.channel = evt.channel;
        note.note = evt.data_loc[0];
        note.velocity = evt.data_loc[1];
        note.ttl = readBEint(evt.data_loc + 2, 3);
        if(duratedNoteInsert(track, &note)) // Do call true Note ON only when note OFF is successfully added into the list!
            m_interface->rt_noteOn(m_interface->rtUserData, static_cast<uint8_t>(midCh), note.note, note.velocity);
        break;
    }

    case MidiEvent::T_NOTETOUCH: // Note touch
    {
        uint8_t note = evt.data_loc[0];
        uint8_t vol =  evt.data_loc[1];
        m_interface->rt_noteAfterTouch(m_interface->rtUserData, static_cast<uint8_t>(midCh), note, vol);
        break;
    }

    case MidiEvent::T_CTRLCHANGE: // Controller change
    {
        uint8_t ctrlno = evt.data_loc[0];
        uint8_t value =  evt.data_loc[1];
        m_interface->rt_controllerChange(m_interface->rtUserData, static_cast<uint8_t>(midCh), ctrlno, value);
        break;
    }

    case MidiEvent::T_PATCHCHANGE: // Patch change
    {
        m_interface->rt_patchChange(m_interface->rtUserData, static_cast<uint8_t>(midCh), evt.data_loc[0]);
        break;
    }

    case MidiEvent::T_CHANAFTTOUCH: // Channel after-touch
    {
        uint8_t chanat = evt.data_loc[0];
        m_interface->rt_channelAfterTouch(m_interface->rtUserData, static_cast<uint8_t>(midCh), chanat);
        break;
    }

    case MidiEvent::T_WHEEL: // Wheel/pitch bend
    {
        uint8_t a = evt.data_loc[0];
        uint8_t b = evt.data_loc[1];
        m_interface->rt_pitchBend(m_interface->rtUserData, static_cast<uint8_t>(midCh), b, a);
        break;
    }

    default:
        break;
    }//switch
}

void BW_MidiSequencer::processDuratedNotes(size_t track, int32_t &status)
{
    DuratedNotesCache &cache = m_trackDuratedNotes[track];

    if(cache.notes_count == 0)
        return; // Nothing to do!

    for(size_t i = 0; i < cache.notes_count; )
    {
        if(cache.notes[i].ttl <= 0)
        {
            DuratedNote *n = &cache.notes[i];

            if(m_interface->rt_noteOff)
                m_interface->rt_noteOff(m_interface->rtUserData, n->channel, n->note);

            if(m_interface->rt_noteOffVel)
                m_interface->rt_noteOffVel(m_interface->rtUserData, n->channel, n->note, n->velocity);

            status = MidiEvent::T_NOTEOFF;

            duratedNotePop(track, i);
        }
        else
            ++i;
    }
}

bool BW_MidiSequencer::processEvents(bool isSeek)
{
    if(m_currentPosition.track.size() == 0)
        m_atEnd = true; // No MIDI track data to play

    if(m_atEnd)
        return false;   // No more events in the queue

    m_loop.caughtEnd = false;
    const size_t        trackCount = m_currentPosition.track.size();
    const Position      rowBeginPosition(m_currentPosition);
    bool     doLoopJump = false;
    unsigned caughLoopStart = 0;
    unsigned caughLoopStackStart = 0;
    unsigned caughLoopStackEnds = 0;
    double   caughLoopStackEndsTime = 0.0;
    unsigned caughLoopStackBreaks = 0;

#ifdef DEBUG_TIME_CALCULATION
    double maxTime = 0.0;
#endif

    for(size_t tk = 0; tk < trackCount; ++tk)
    {
        Position::TrackInfo &track = m_currentPosition.track[tk];

        // Process note-OFFs
        processDuratedNotes(tk, track.lastHandledEvent);

        if((track.lastHandledEvent >= 0) && (track.delay <= 0))
        {
            // Check is an end of track has been reached
            if(track.pos == m_trackData[tk].end())
            {
                track.lastHandledEvent = -1;
                break;
            }

            // Handle event
            for(size_t i = track.pos->events_begin; i < track.pos->events_end; ++i)
            {
                const MidiEvent &evt = m_eventBank[i];
#ifdef ENABLE_BEGIN_SILENCE_SKIPPING
                if(!m_currentPosition.began && (evt.type == MidiEvent::T_NOTEON))
                    m_currentPosition.began = true;
#endif
                if(isSeek && (evt.type == MidiEvent::T_NOTEON || evt.type == MidiEvent::T_NOTEON_DURATED))
                    continue;

                handleEvent(tk, evt, track.lastHandledEvent);

                if(m_loop.caughtStart)
                {
                    if(m_interface->onloopStart) // Loop Start hook
                        m_interface->onloopStart(m_interface->onloopStart_userData);

                    caughLoopStart++;
                    m_loop.caughtStart = false;
                }

                if(m_loop.caughtStackStart)
                {
                    if(m_interface->onloopStart && (m_loopStartTime >= track.pos->time)) // Loop Start hook
                        m_interface->onloopStart(m_interface->onloopStart_userData);

                    caughLoopStackStart++;
                    m_loop.caughtStackStart = false;
                }

                if(m_loop.caughtStackBreak)
                {
                    caughLoopStackBreaks++;
                    m_loop.caughtStackBreak = false;
                }

                if(m_loop.caughtEnd || m_loop.isStackEnd())
                {
                    if(m_loop.caughtStackEnd)
                    {
                        m_loop.caughtStackEnd = false;
                        caughLoopStackEnds++;
                        caughLoopStackEndsTime = track.pos->time;
                    }
                    doLoopJump = true;
                    break; // Stop event handling on catching loopEnd event!
                }
            }

#ifdef DEBUG_TIME_CALCULATION
            if(maxTime < track.pos->time)
                maxTime = track.pos->time;
#endif
            // Read next event time (unless the track just ended)
            if(track.lastHandledEvent >= 0)
            {
                track.delay += track.pos->delay;
                track.pos++;
            }

            if(doLoopJump)
                break;
        }
    }

#ifdef DEBUG_TIME_CALCULATION
    std::fprintf(stdout, "                              \r");
    std::fprintf(stdout, "Time: %10f; Audio: %10f\r", maxTime, m_currentPosition.absTimePosition);
    std::fflush(stdout);
#endif

    // Find a shortest delay from all track
    uint64_t shortestDelay = 0;
    bool     shortestDelayNotFound = true;

    for(size_t tk = 0; tk < trackCount; ++tk)
    {
        Position::TrackInfo &track = m_currentPosition.track[tk];
        DuratedNotesCache &timedNotes = m_trackDuratedNotes[tk];

        // Normal events
        if((track.lastHandledEvent >= 0) && (shortestDelayNotFound || track.delay < shortestDelay))
        {
            shortestDelay = track.delay;
            shortestDelayNotFound = false;
        }

        // Note events with duration
        for(size_t i = 0; i < timedNotes.notes_count; ++i)
        {
            DuratedNote &n = timedNotes.notes[i];
            if(n.ttl <= 0)
            {
                shortestDelay = 0; // Just zero!
                shortestDelayNotFound = false;
            }
            else if(shortestDelayNotFound || static_cast<uint64_t>(n.ttl) < shortestDelay)
            {
                shortestDelay = n.ttl; // Extra tick
                shortestDelayNotFound = false;
            }
        }
    }

    // Schedule the next playevent to be processed after that delay
    for(size_t tk = 0; tk < trackCount; ++tk)
    {
        m_currentPosition.track[tk].delay -= shortestDelay;
        duratedNoteTick(tk, shortestDelay);
    }

    fraction<uint64_t> t = shortestDelay * m_tempo;

#ifdef ENABLE_BEGIN_SILENCE_SKIPPING
    if(m_currentPosition.began)
#endif
        m_currentPosition.wait += t.value();

    if(caughLoopStart > 0 && m_loopBeginPosition.absTimePosition <= 0.0)
        m_loopBeginPosition = rowBeginPosition;

    if(caughLoopStackStart > 0)
    {
        while(caughLoopStackStart > 0)
        {
            m_loop.stackUp();
            LoopStackEntry &s = m_loop.getCurStack();
            s.startPosition = rowBeginPosition;
            caughLoopStackStart--;
        }
        return true;
    }

    if(caughLoopStackBreaks > 0)
    {
        while(caughLoopStackBreaks > 0)
        {
            LoopStackEntry &s = m_loop.getCurStack();
            s.loops = 0;
            s.infinity = false;
            // Quit the loop
            m_loop.stackDown();
            caughLoopStackBreaks--;
        }
    }

    if(caughLoopStackEnds > 0)
    {
        while(caughLoopStackEnds > 0)
        {
            LoopStackEntry &s = m_loop.getCurStack();
            if(s.infinity)
            {
                if(m_interface->onloopEnd && (m_loopEndTime >= caughLoopStackEndsTime)) // Loop End hook
                {
                    m_interface->onloopEnd(m_interface->onloopEnd_userData);
                    if(m_loopHooksOnly) // Stop song on reaching loop end
                    {
                        m_atEnd = true; // Don't handle events anymore
                        m_currentPosition.wait += m_postSongWaitDelay; // One second delay until stop playing
                    }
                }

                m_currentPosition = s.startPosition;
                m_loop.skipStackStart = true;

                for(uint8_t i = 0; i < 16; i++)
                    m_interface->rt_controllerChange(m_interface->rtUserData, i, 123, 0);

                return true;
            }
            else
            if(s.loops >= 0)
            {
                s.loops--;
                if(s.loops > 0)
                {
                    m_currentPosition = s.startPosition;
                    m_loop.skipStackStart = true;

                    for(uint8_t i = 0; i < 16; i++)
                        m_interface->rt_controllerChange(m_interface->rtUserData, i, 123, 0);

                    return true;
                }
                else
                {
                    // Quit the loop
                    m_loop.stackDown();
                }
            }
            else
            {
                // Quit the loop
                m_loop.stackDown();
            }
            caughLoopStackEnds--;
        }

        return true;
    }

    if(shortestDelayNotFound || m_loop.caughtEnd)
    {
        if(m_interface->onloopEnd) // Loop End hook
            m_interface->onloopEnd(m_interface->onloopEnd_userData);

        for(uint8_t i = 0; i < 16; i++)
            m_interface->rt_controllerChange(m_interface->rtUserData, i, 123, 0);

        // Loop if song end or loop end point has reached
        m_loop.caughtEnd         = false;

        if(!m_loopEnabled || (shortestDelayNotFound && m_loop.loopsCount >= 0 && m_loop.loopsLeft < 1) || m_loopHooksOnly)
        {
            m_atEnd = true; // Don't handle events anymore
            m_currentPosition.wait += m_postSongWaitDelay; // One second delay until stop playing
            return true; // We have caugh end here!
        }

        if(m_loop.temporaryBroken)
        {
            m_currentPosition = m_trackBeginPosition;
            m_loop.temporaryBroken = false;
        }
        else if(m_loop.loopsCount < 0 || m_loop.loopsLeft >= 1)
        {
            m_currentPosition = m_loopBeginPosition;
            if(m_loop.loopsCount >= 1)
                m_loop.loopsLeft--;
        }
    }

    return true; // Has events in queue
}

#endif /* BW_MIDISEQ_PROCESS_IMPL_HPP */
