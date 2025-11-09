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
    size_t length, midCh, loopStackLevel;
    const uint8_t *datau;
    const char *data;
    int loopsNum;
    DuratedNote *note;
    LoopStackEntry *loopEntryP;
    LoopState *loop;

    if(m_deviceMask != Device_ANY && (m_deviceMask & m_trackDevices[track]) == 0)
        return; // Ignore this track completely

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

    if(m_interface->onEvent && evt.type < 0x100 && evt.subtype < 0x100)
    {
        // Only standard MIDI events will be reported, built-in events (>=0x100) will remain private

        if(evt.data_block.size > 0)
            m_interface->onEvent(m_interface->onEvent_userData,
                                 evt.type, evt.subtype, evt.channel,
                                 getData(evt.data_block), evt.data_block.size);
        else
            m_interface->onEvent(m_interface->onEvent_userData,
                                 evt.type, evt.subtype, evt.channel,
                                 evt.data_loc, evt.data_loc_size);
    }

    if(evt.type == MidiEvent::T_SYSCOMSNGSEL || evt.type == MidiEvent::T_SYSCOMSPOSPTR)
        return;

    midCh = evt.channel;

    if(evt.type < 0x10)
    {
        if(m_interface->rt_currentDevice)
            midCh += m_interface->rt_currentDevice(m_interface->rtUserData, track);

        status = evt.type;
    }

    switch(evt.type)
    {
    case MidiEvent::T_SYSEX:
    case MidiEvent::T_SYSEX2: // Handle SysEx
        if(m_interface->rt_systemExclusive)
            m_interface->rt_systemExclusive(m_interface->rtUserData, getData(evt.data_block), evt.data_block.size);
        return;

    case MidiEvent::T_SPECIAL:
    {
        length = evt.data_block.size > 0 ? evt.data_block.size : static_cast<size_t>(evt.data_loc_size);
        datau = evt.data_block.size > 0 ? getData(evt.data_block) : evt.data_loc;
        data = (length ? reinterpret_cast<const char *>(datau) : "\0\0\0\0\0\0\0\0");

        if(m_interface->rt_metaEvent && evt.subtype < 0x100) // Meta event hook
            m_interface->rt_metaEvent(m_interface->rtUserData, evt.subtype, reinterpret_cast<const uint8_t*>(data), length);

        switch(evt.subtype)
        {
        case MidiEvent::ST_ENDTRACK:
            status = -1;
            return;
        case MidiEvent::ST_TEMPOCHANGE:
            m_tempo = m_invDeltaTicks * fraction<uint64_t>(readBEint(datau, length));
            return;

        case MidiEvent::ST_DEVICESWITCH:
            if(m_interface->onDebugMessage)
                m_interface->onDebugMessage(m_interface->onDebugMessage_userData, "Switching another device: %.*s", length, data);
            if(m_interface->rt_deviceSwitch)
                m_interface->rt_deviceSwitch(m_interface->rtUserData, track, data, length);
            return;

        case MidiEvent::ST_MARKER:
        case MidiEvent::ST_TEXT:
        case MidiEvent::ST_LYRICS:
        case MidiEvent::ST_TIMESIGNATURE:
        case MidiEvent::ST_SMPTEOFFSET:
            // Do nothing! :-P
            return;


        // NON-STANDARD EVENTS (>= 0x100)


        case MidiEvent::ST_CALLBACK_TRIGGER:
#if 0 /* Print all callback triggers events */
            if(m_interface->onDebugMessage)
                m_interface->onDebugMessage(m_interface->onDebugMessage_userData, "Callback Trigger: %02X", evt.data[0]);
#endif
            if(m_triggerHandler)
                m_triggerHandler(m_triggerUserData, static_cast<unsigned>(data[0]), track);
            return;

        case MidiEvent::ST_RAWOPL:
            if(m_interface->rt_rawOPL)
                m_interface->rt_rawOPL(m_interface->rtUserData, datau[0], datau[1]);
            return;


        case MidiEvent::ST_SONG_BEGIN_HOOK:
            if(m_interface->onSongStart)
                m_interface->onSongStart(m_interface->onSongStart_userData);
            return;

        case MidiEvent::ST_LOOPSTART:
            if(m_loopEnabled && !m_loop.invalidLoop)
                m_loop.caughtStart = true;
            return;

        case MidiEvent::ST_LOOPEND:
            if(m_loopEnabled && !m_loop.invalidLoop)
                m_loop.caughtEnd = true;
            return;

        case MidiEvent::ST_LOOPSTACK_BEGIN:
            if(m_loopEnabled && !m_loop.invalidLoop)
            {
                if(m_loop.skipStackStart)
                {
                    m_loop.skipStackStart = false;
                    return;
                }

                loopsNum = datau[0];
                loopStackLevel = static_cast<size_t>(m_loop.stackLevel + 1);

                while(loopStackLevel >= m_loop.stackDepth && m_loop.stackDepth < LoopState::stackDepthMax - 1)
                {
                    loopEntryP = &m_loop.stack[m_loop.stackDepth++];
                    loopEntryP->loops = loopsNum;
                    loopEntryP->infinity = (loopsNum == 0);
                    loopEntryP->start = 0;
                    loopEntryP->end = 0;
                }

                loopEntryP = &m_loop.stack[loopStackLevel];
                loopEntryP->loops = static_cast<int>(loopsNum);
                loopEntryP->infinity = (loopsNum == 0);
                m_loop.caughtStackStart = true;
            }
            return;

        case MidiEvent::ST_LOOPSTACK_END:
            if(m_loopEnabled && !m_loop.invalidLoop)
                m_loop.caughtStackEnd = true;
            return;

        case MidiEvent::ST_LOOPSTACK_BREAK:
            if(m_loopEnabled && !m_loop.invalidLoop)
                m_loop.caughtStackBreak = true;
            return;

        case MidiEvent::ST_TRACK_LOOPSTACK_BEGIN:
            loop = &m_trackLoop[track];
            if(m_loopEnabled && !loop->invalidLoop)
            {
                if(loop->skipStackStart)
                {
                    loop->skipStackStart = false;
                    return;
                }

                loopsNum = datau[0];
                loopStackLevel = static_cast<size_t>(loop->stackLevel + 1);

                while(loopStackLevel >= loop->stackDepth && loop->stackDepth < LoopState::stackDepthMax - 1)
                {
                    loopEntryP = &loop->stack[loop->stackDepth++];
                    loopEntryP->loops = loopsNum;
                    loopEntryP->infinity = (loopsNum == 0);
                    loopEntryP->start = 0;
                    loopEntryP->end = 0;
                }

                loopEntryP = &loop->stack[loopStackLevel];
                loopEntryP->loops = static_cast<int>(loopsNum);
                loopEntryP->infinity = (loopsNum == 0);
                loop->caughtStackStart = true;
            }
            return;

        case MidiEvent::ST_TRACK_LOOPSTACK_END:
            loop = &m_trackLoop[track];
            if(m_loopEnabled && !loop->invalidLoop)
                loop->caughtStackEnd = true;
            return;

        case MidiEvent::ST_TRACK_LOOPSTACK_BREAK:
            loop = &m_trackLoop[track];
            if(m_loopEnabled && !loop->invalidLoop)
                loop->caughtStackBreak = true;
            return;
        }

        return;
    }

    case MidiEvent::T_NOTEOFF: // Note off
        if(evt.channel < 16 && m_channelDisable[evt.channel])
            return; // Disabled channel

        if(m_interface->rt_noteOff)
            m_interface->rt_noteOff(m_interface->rtUserData, static_cast<uint8_t>(midCh), evt.data_loc[0]);

        if(m_interface->rt_noteOffVel)
            m_interface->rt_noteOffVel(m_interface->rtUserData, static_cast<uint8_t>(midCh), evt.data_loc[0], evt.data_loc[1]);

        return;

    case MidiEvent::T_NOTEON:  // Note on
        if(evt.channel < 16 && m_channelDisable[evt.channel])
            return; // Disabled channel
        m_interface->rt_noteOn(m_interface->rtUserData, static_cast<uint8_t>(midCh), evt.data_loc[0], evt.data_loc[1]);
        return;

    case MidiEvent::T_NOTEON_DURATED: // Note on with duration
        if(duratedNoteAlloc(track, &note)) // Do call true Note ON only when note OFF is successfully added into the list!
        {
            note->channel = evt.channel;
            note->note = evt.data_loc[0];
            note->velocity = evt.data_loc[1];
            note->ttl = readBEint(evt.data_loc + 2, 3);
            m_interface->rt_noteOn(m_interface->rtUserData, static_cast<uint8_t>(midCh), evt.data_loc[0], evt.data_loc[1]);
        }
        return;

    case MidiEvent::T_NOTETOUCH: // Note touch
        m_interface->rt_noteAfterTouch(m_interface->rtUserData, static_cast<uint8_t>(midCh), evt.data_loc[0], evt.data_loc[1]);
        return;

    case MidiEvent::T_CTRLCHANGE: // Controller change
        m_interface->rt_controllerChange(m_interface->rtUserData, static_cast<uint8_t>(midCh), evt.data_loc[0], evt.data_loc[1]);
        return;

    case MidiEvent::T_PATCHCHANGE: // Patch change
        m_interface->rt_patchChange(m_interface->rtUserData, static_cast<uint8_t>(midCh), evt.data_loc[0]);
        return;

    case MidiEvent::T_CHANAFTTOUCH: // Channel after-touch
        m_interface->rt_channelAfterTouch(m_interface->rtUserData, static_cast<uint8_t>(midCh), evt.data_loc[0]);
        return;

    case MidiEvent::T_WHEEL: // Wheel/pitch bend
        m_interface->rt_pitchBend(m_interface->rtUserData, static_cast<uint8_t>(midCh), evt.data_loc[1], evt.data_loc[0]);
        return;

    default:
        return;
    }
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
        LoopState &trackLoop = m_trackLoop[tk];
        unsigned caughLocalLoopStackStart = 0;
        unsigned caughLocalLoopStackEnds = 0;
        unsigned caughLocalLoopStackBreaks = 0;

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

                if(trackLoop.caughtStackStart)
                {
                    caughLocalLoopStackStart++;
                    trackLoop.caughtStackStart = false;
                }

                if(trackLoop.caughtStackBreak)
                {
                    caughLocalLoopStackBreaks++;
                    trackLoop.caughtStackBreak = false;
                }

                if(trackLoop.isStackEnd())
                {
                    if(trackLoop.caughtStackEnd)
                    {
                        trackLoop.caughtStackEnd = false;
                        caughLocalLoopStackEnds++;
                    }
                    break; // Stop event handling on catching loopEnd event!
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

            // Process local loop
            if(caughLocalLoopStackStart > 0)
            {
                while(caughLocalLoopStackStart > 0)
                {
                    trackLoop.stackUp();
                    LoopStackEntry &s = trackLoop.getCurStack();
                    s.startPosition.track.clear();
                    s.startPosition.track.push_back(rowBeginPosition.track[tk]);
                    s.startPosition.absTimePosition = rowBeginPosition.absTimePosition;
                    s.startPosition.wait = rowBeginPosition.wait;
                    s.startPosition.began = rowBeginPosition.began;
                    caughLocalLoopStackStart--;
                }
                return true;
            }

            if(caughLocalLoopStackBreaks > 0)
            {
                while(caughLocalLoopStackBreaks > 0)
                {
                    LoopStackEntry &s = trackLoop.getCurStack();
                    s.loops = 0;
                    s.infinity = false;
                    // Quit the loop
                    trackLoop.stackDown();
                    caughLocalLoopStackBreaks--;
                }
            }

            if(caughLocalLoopStackEnds > 0)
            {
                while(caughLocalLoopStackEnds > 0)
                {
                    LoopStackEntry &s = trackLoop.getCurStack();
                    if(s.infinity)
                    {
                        m_currentPosition.track[tk] = s.startPosition.track[0];
                        trackLoop.skipStackStart = true;
                        return true;
                    }
                    else
                    if(s.loops >= 0)
                    {
                        s.loops--;
                        if(s.loops > 0)
                        {
                            m_currentPosition.track[tk] = s.startPosition.track[0];
                            trackLoop.skipStackStart = true;
                            return true;
                        }
                        else
                        {
                            // Quit the loop
                            trackLoop.stackDown();
                        }
                    }
                    else
                    {
                        // Quit the loop
                        trackLoop.stackDown();
                    }
                    caughLocalLoopStackEnds--;
                }
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

                // Do all notes off on loop
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

                    // Do all notes off on loop
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

        // Do all notes off on loop
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
