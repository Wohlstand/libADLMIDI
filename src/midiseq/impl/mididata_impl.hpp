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
#ifndef BW_MIDISEQ_MIDIDATA_IMPL_HPP
#define BW_MIDISEQ_MIDIDATA_IMPL_HPP

#include <cstring>

#include "../midi_sequencer.hpp"


void BW_MidiSequencer::buildSmfSetupReset(size_t trackCount)
{
    m_tracksCount = trackCount;
    m_fullSongTimeLength = 0.0;
    m_loopStartTime = -1.0;
    m_loopEndTime = -1.0;
    m_loopFormat = Loop_Default;
    m_trackDisable.clear();
    m_trackSolo = ~(size_t)0;
    m_musTitle.size = 0;
    m_musTitle.offset = 0;
    m_musCopyright.size = 0;
    m_musCopyright.offset = 0;

    m_currentPosition.began = false;
    m_currentPosition.absTimePosition = 0.0;
    m_currentPosition.wait = 0.0;

    m_musTrackTitles.clear();
    m_musMarkers.clear();
    m_dataBank.clear();
    m_eventBank.clear();
    m_trackData.clear();
    m_trackDuratedNotes.clear();

    m_loop.reset();
    m_loop.invalidLoop = false;
    m_time.reset();

    m_currentPosition.track.clear();

    buildSmfResizeTracks(m_tracksCount);

    std::memset(m_trackDuratedNotes.data(), 0, sizeof(DuratedNotesCache) * trackCount);
    std::memset(m_channelDisable, 0, sizeof(m_channelDisable));
}

void BW_MidiSequencer::buildSmfResizeTracks(size_t tracksCount)
{
    m_tracksCount = tracksCount;
    m_trackData.resize(m_tracksCount, MidiTrackQueue());
    m_currentPosition.track.resize(m_tracksCount);
    m_trackDuratedNotes.resize(m_tracksCount);
    m_trackDisable.resize(m_tracksCount);
}


void BW_MidiSequencer::initTracksBegin(size_t track)
{
    if(m_trackData[track].size() > 0)
    {
        MidiTrackQueue::iterator pos = m_trackData[track].begin();
        m_currentPosition.track[track].pos = pos;
        // Some events doesn't begin at zero!
        m_currentPosition.track[track].delay = pos->absPos;
        m_currentPosition.track[track].lastHandledEvent = 0;
    }
    else
    {
        m_currentPosition.track[track].delay = 0;
        m_currentPosition.track[track].lastHandledEvent = -1;
    }
}

void BW_MidiSequencer::installLoop(BW_MidiSequencer::LoopPointParseState &loopState)
{
    if(loopState.gotLoopStart && !loopState.gotLoopEnd)
    {
        loopState.gotLoopEnd = true;
        loopState.loopEndTicks = loopState.ticksSongLength;
    }

    // loopStart must be located before loopEnd!
    if(loopState.loopStartTicks >= loopState.loopEndTicks)
    {
        m_loop.invalidLoop = true;
        if(m_interface->onDebugMessage && (loopState.gotLoopStart || loopState.gotLoopEnd))
        {
            m_interface->onDebugMessage(
                m_interface->onDebugMessage_userData,
                "== Invalid loop detected! [loopEnd is going before loopStart] =="
            );
        }
    }
}


void BW_MidiSequencer::analyseLoopEvent(LoopPointParseState &loopState, const MidiEvent &event, uint64_t abs_position)
{
    if(!m_loop.invalidLoop && (event.subtype == MidiEvent::ST_LOOPSTART))
    {
        /*
         * loopStart is invalid when:
         * - starts together with loopEnd
         * - appears more than one time in same MIDI file
         */
        if(loopState.gotLoopStart || loopState.gotLoopEventsInThisRow)
            m_loop.invalidLoop = true;
        else
        {
            loopState.gotLoopStart = true;
            loopState.loopStartTicks = abs_position;
        }
        // In this row we got loop event, register this!
        loopState.gotLoopEventsInThisRow = true;
    }
    else if(!m_loop.invalidLoop && (event.subtype == MidiEvent::ST_LOOPEND))
    {
        /*
         * loopEnd is invalid when:
         * - starts before loopStart
         * - starts together with loopStart
         * - appars more than one time in same MIDI file
         */
        if(loopState.gotLoopEnd || loopState.gotLoopEventsInThisRow)
        {
            m_loop.invalidLoop = true;
            if(m_interface->onDebugMessage)
            {
                m_interface->onDebugMessage(
                    m_interface->onDebugMessage_userData,
                    "== Invalid loop detected! %s %s ==",
                    (loopState.gotLoopEnd ? "[Caught more than 1 loopEnd!]" : ""),
                    (loopState.gotLoopEventsInThisRow ? "[loopEnd in same row as loopStart!]" : "")
                );
            }
        }
        else
        {
            loopState.gotLoopEnd = true;
            loopState.loopEndTicks = abs_position;
        }
        // In this row we got loop event, register this!
        loopState.gotLoopEventsInThisRow = true;
    }
    else if(!m_loop.invalidLoop && (event.subtype == MidiEvent::ST_LOOPSTACK_BEGIN))
    {
        if(!loopState.gotStackLoopStart)
        {
            if(!loopState.gotLoopStart)
                loopState.loopStartTicks = abs_position;
            loopState.gotStackLoopStart = true;
        }

        m_loop.stackUp();
        if(m_loop.stackLevel >= static_cast<int>(m_loop.stack.size()))
        {
            LoopStackEntry e;
            e.loops = event.data_loc[0];
            e.infinity = (event.data_loc[0] == 0);
            e.start = abs_position;
            e.end = abs_position;
            m_loop.stack.push_back(e);
        }
    }
    else if(!m_loop.invalidLoop &&
        ((event.subtype == MidiEvent::ST_LOOPSTACK_END) ||
         (event.subtype == MidiEvent::ST_LOOPSTACK_BREAK))
    )
    {
        if(m_loop.stackLevel <= -1)
        {
            m_loop.invalidLoop = true; // Caught loop end without of loop start!
            if(m_interface->onDebugMessage)
            {
                m_interface->onDebugMessage(
                    m_interface->onDebugMessage_userData,
                    "== Invalid loop detected! [Caught loop end without of loop start] =="
                );
            }
        }
        else
        {
            if(loopState.loopEndTicks < abs_position)
                loopState.loopEndTicks = abs_position;
            m_loop.getCurStack().end = abs_position;
            m_loop.stackDown();
        }
    }
}


void BW_MidiSequencer::buildTimeLine(const std::vector<TempoEvent> &tempos,
                                          uint64_t loopStartTicks,
                                          uint64_t loopEndTicks)
{
    const size_t    trackCount = m_trackData.size();
    /********************************************************************************/
    // Calculate time basing on collected tempo events
    /********************************************************************************/
    for(size_t tk = 0; tk < trackCount; ++tk)
    {
        fraction<uint64_t> currentTempo = m_tempo;
        double  time = 0.0;
        // uint64_t abs_position = 0;
        size_t tempo_change_index = 0;
        MidiTrackQueue &track = m_trackData[tk];
        if(track.empty())
            continue;//Empty track is useless!

#ifdef DEBUG_TIME_CALCULATION
        std::fprintf(stdout, "\n============Track %" PRIuPTR "=============\n", tk);
        std::fflush(stdout);
#endif

        MidiTrackRow fakePos, *posPrev = &(*(track.begin()));//First element
        if(posPrev->absPos > 0) // If doesn't begins with zero!
        {
            fakePos.absPos = 0;
            fakePos.delay = posPrev->absPos;
            posPrev = &fakePos;
        }

        for(MidiTrackQueue::iterator it = track.begin(); it != track.end(); it++)
        {
#ifdef DEBUG_TIME_CALCULATION
            bool tempoChanged = false;
#endif
            MidiTrackRow &pos = *it;
            if((posPrev != &pos) && // Skip first event
               (!tempos.empty()) && // Only when in-track tempo events are available
               (tempo_change_index < tempos.size())
              )
            {
                // If tempo event is going between of current and previous event
                if(tempos[tempo_change_index].absPosition <= pos.absPos)
                {
                    // Stop points: begin point and tempo change points are before end point
                    std::vector<TempoChangePoint> points;
                    fraction<uint64_t> t;
                    TempoChangePoint firstPoint = {posPrev->absPos, currentTempo};
                    points.push_back(firstPoint);

                    // Collect tempo change points between previous and current events
                    do
                    {
                        TempoChangePoint tempoMarker;
                        const TempoEvent &tempoPoint = tempos[tempo_change_index];
                        tempoMarker.absPos = tempoPoint.absPosition;
                        tempoMarker.tempo = m_invDeltaTicks * fraction<uint64_t>(tempoPoint.tempo);
                        points.push_back(tempoMarker);
                        tempo_change_index++;
                    }
                    while((tempo_change_index < tempos.size()) &&
                          (tempos[tempo_change_index].absPosition <= pos.absPos));

                    // Re-calculate time delay of previous event
                    time -= posPrev->timeDelay;
                    posPrev->timeDelay = 0.0;

                    for(size_t i = 0, j = 1; j < points.size(); i++, j++)
                    {
                        /* If one or more tempo events are appears between of two events,
                         * calculate delays between each tempo point, begin and end */
                        uint64_t midDelay = 0;
                        // Delay between points
                        midDelay  = points[j].absPos - points[i].absPos;
                        // Time delay between points
                        t = midDelay * currentTempo;
                        posPrev->timeDelay += t.value();

                        // Apply next tempo
                        currentTempo = points[j].tempo;
#ifdef DEBUG_TIME_CALCULATION
                        tempoChanged = true;
#endif
                    }
                    // Then calculate time between last tempo change point and end point
                    TempoChangePoint tailTempo = points.back();
                    uint64_t postDelay = pos.absPos - tailTempo.absPos;
                    t = postDelay * currentTempo;
                    posPrev->timeDelay += t.value();

                    // Store Common time delay
                    posPrev->time = time;
                    time += posPrev->timeDelay;
                }
            }

            fraction<uint64_t> t = pos.delay * currentTempo;
            pos.timeDelay = t.value();
            pos.time = time;
            time += pos.timeDelay;

            // Capture markers after time value calculation
            for(size_t i = pos.events_begin; i < pos.events_end; ++i)
            {
                MidiEvent &e = m_eventBank[i];
                if((e.type == MidiEvent::T_SPECIAL) && (e.subtype == MidiEvent::ST_MARKER))
                {
                    MIDI_MarkerEntry marker;
                    marker.label = e.data_block;
                    marker.pos_ticks = pos.absPos;
                    marker.pos_time = pos.time;
                    m_musMarkers.push_back(marker);
                }
            }

            // Capture loop points time positions
            if(!m_loop.invalidLoop)
            {
                // Set loop points times
                if(loopStartTicks == pos.absPos)
                    m_loopStartTime = pos.time;
                else if(loopEndTicks == pos.absPos)
                    m_loopEndTime = pos.time;
            }

#ifdef DEBUG_TIME_CALCULATION
            std::fprintf(stdout, "= %10" PRId64 " = %10f%s\n", pos.absPos, pos.time, tempoChanged ? " <----TEMPO CHANGED" : "");
            std::fflush(stdout);
#endif

            // abs_position += pos.delay;
            posPrev = &pos;
        }

        if(time > m_fullSongTimeLength)
            m_fullSongTimeLength = time;
    }

    m_fullSongTimeLength += m_postSongWaitDelay;
    // Set begin of the music
    m_trackBeginPosition = m_currentPosition;
    // Initial loop position will begin at begin of track until passing of the loop point
    m_loopBeginPosition  = m_currentPosition;
    // Set lowest level of the loop stack
    m_loop.stackLevel = -1;

    // Set the count of loops
    m_loop.loopsCount = m_loopCount;
    m_loop.loopsLeft = m_loopCount;

    /********************************************************************************/
    // Find and set proper loop points
    /********************************************************************************/
    if(!m_loop.invalidLoop && !m_currentPosition.track.empty())
    {
        unsigned caughLoopStart = 0;
        bool scanDone = false;
        const size_t  trackCount = m_currentPosition.track.size();
        Position rowPosition(m_currentPosition);

        while(!scanDone)
        {
            const Position      rowBeginPosition(rowPosition);

            for(size_t tk = 0; tk < trackCount; ++tk)
            {
                Position::TrackInfo &track = rowPosition.track[tk];
                if((track.lastHandledEvent >= 0) && (track.delay <= 0))
                {
                    // Check is an end of track has been reached
                    if(track.pos == m_trackData[tk].end())
                    {
                        track.lastHandledEvent = -1;
                        continue;
                    }

                    for(size_t i = track.pos->events_begin; i < track.pos->events_end; ++i)
                    {
                        const MidiEvent &evt = m_eventBank[i];
                        if(evt.type == MidiEvent::T_SPECIAL && evt.subtype == MidiEvent::ST_LOOPSTART)
                        {
                            caughLoopStart++;
                            scanDone = true;
                            break;
                        }
                    }

                    if(track.lastHandledEvent >= 0)
                    {
                        track.delay += track.pos->delay;
                        track.pos++;
                    }
                }
            }

            // Find a shortest delay from all track
            uint64_t shortestDelay = 0;
            bool     shortestDelayNotFound = true;

            for(size_t tk = 0; tk < trackCount; ++tk)
            {
                Position::TrackInfo &track = rowPosition.track[tk];
                if((track.lastHandledEvent >= 0) && (shortestDelayNotFound || track.delay < shortestDelay))
                {
                    shortestDelay = track.delay;
                    shortestDelayNotFound = false;
                }
            }

            // Schedule the next playevent to be processed after that delay
            for(size_t tk = 0; tk < trackCount; ++tk)
                rowPosition.track[tk].delay -= shortestDelay;

            if(caughLoopStart > 0)
            {
                m_loopBeginPosition = rowBeginPosition;
                m_loopBeginPosition.absTimePosition = m_loopStartTime;
                scanDone = true;
            }

            if(shortestDelayNotFound)
                break;
        }
    }

    /********************************************************************************/
    // Resolve "hell of all times" of too short drum notes:
    // move too short percussion note-offs far far away as possible
    /********************************************************************************/
// OLD DEAD CODE NO LONGER NEEDED
#if 0 // Use this to record WAVEs for comparison before/after implementing of this
    if(m_format == Format_MIDI) // Percussion fix is needed for MIDI only, not for IMF/RSXX or CMF
    {
        //! Minimal real time in seconds
#define DRUM_NOTE_MIN_TIME  0.03
        //! Minimal ticks count
#define DRUM_NOTE_MIN_TICKS 15
        struct NoteState
        {
            double       delay;
            uint64_t     delayTicks;
            bool         isOn;
            char         ___pad[7];
        } drNotes[255];
        size_t banks[16];

        for(size_t tk = 0; tk < trackCount; ++tk)
        {
            std::memset(drNotes, 0, sizeof(drNotes));
            std::memset(banks, 0, sizeof(banks));
            MidiTrackQueue &track = m_trackData[tk];
            if(track.empty())
                continue; // Empty track is useless!

            for(MidiTrackQueue::iterator it = track.begin(); it != track.end(); it++)
            {
                MidiTrackRow &pos = *it;

                for(ssize_t e = 0; e < (ssize_t)pos.events.size(); e++)
                {
                    MidiEvent *et = &pos.events[(size_t)e];

                    /* Set MSB/LSB bank */
                    if(et->type == MidiEvent::T_CTRLCHANGE)
                    {
                        uint8_t ctrlno = et->data[0];
                        uint8_t value =  et->data[1];
                        switch(ctrlno)
                        {
                        case 0: // Set bank msb (GM bank)
                            banks[et->channel] = (value << 8) | (banks[et->channel] & 0x00FF);
                            break;
                        case 32: // Set bank lsb (XG bank)
                            banks[et->channel] = (banks[et->channel] & 0xFF00) | (value & 0x00FF);
                            break;
                        default:
                            break;
                        }
                        continue;
                    }

                    bool percussion = (et->channel == 9) ||
                                      banks[et->channel] == 0x7E00 || // XG SFX1/SFX2 channel (16128 signed decimal)
                                      banks[et->channel] == 0x7F00;   // XG Percussion channel (16256 signed decimal)
                    if(!percussion)
                        continue;

                    if(et->type == MidiEvent::T_NOTEON)
                    {
                        uint8_t     note = et->data[0] & 0x7F;
                        NoteState   &ns = drNotes[note];
                        ns.isOn = true;
                        ns.delay = 0.0;
                        ns.delayTicks = 0;
                    }
                    else if(et->type == MidiEvent::T_NOTEOFF)
                    {
                        uint8_t note = et->data[0] & 0x7F;
                        NoteState &ns = drNotes[note];
                        if(ns.isOn)
                        {
                            ns.isOn = false;
                            if(ns.delayTicks < DRUM_NOTE_MIN_TICKS || ns.delay < DRUM_NOTE_MIN_TIME) // If note is too short
                            {
                                //Move it into next event position if that possible
                                for(MidiTrackQueue::iterator itNext = it;
                                    itNext != track.end();
                                    itNext++)
                                {
                                    MidiTrackRow &posN = *itNext;
                                    if(ns.delayTicks > DRUM_NOTE_MIN_TICKS && ns.delay > DRUM_NOTE_MIN_TIME)
                                    {
                                        // Put note-off into begin of next event list
                                        posN.events.insert(posN.events.begin(), pos.events[(size_t)e]);
                                        // Renive this event from a current row
                                        pos.events.erase(pos.events.begin() + (int)e);
                                        e--;
                                        break;
                                    }
                                    ns.delay += posN.timeDelay;
                                    ns.delayTicks += posN.delay;
                                }
                            }
                            ns.delay = 0.0;
                            ns.delayTicks = 0;
                        }
                    }
                }

                //Append time delays to sustaining notes
                for(size_t no = 0; no < 128; no++)
                {
                    NoteState &ns = drNotes[no];
                    if(ns.isOn)
                    {
                        ns.delay        += pos.timeDelay;
                        ns.delayTicks   += pos.delay;
                    }
                }
            }
        }
#undef DRUM_NOTE_MIN_TIME
#undef DRUM_NOTE_MIN_TICKS
    }
#endif
}

#endif /* BW_MIDISEQ_READ_SMF_IMPL_HPP */
