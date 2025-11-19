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
#ifdef BWMIDI_DEBUG_TIME_CALCULATION
#   include <inttypes.h>
#   if !defined(__PRIPTR_PREFIX)
#       if __SIZEOF_POINTER__ == 8
#           define __PRIPTR_PREFIX	"l"
#       else
#           define __PRIPTR_PREFIX
#       endif
#   endif
#   ifndef PRId64
#       define PRId64 "lu"
#   endif
#   ifndef PRIuPTR
#       define PRIuPTR __PRIPTR_PREFIX "u"
#   endif
#endif

#include "../midi_sequencer.hpp"


void BW_MidiSequencer::buildSmfSetupReset(size_t trackCount)
{
    m_stateRestoreSetup = TRACK_RESTORE_DEFAULT;
    m_tracksCount = trackCount;
    m_fullSongTimeLength = 0.0;
    m_loopStartTime = -1.0;
    m_loopEndTime = -1.0;
    m_loopFormat = Loop_Default;
    m_trackSolo = ~(size_t)0;
    m_musTitle.size = 0;
    m_musTitle.offset = 0;
    m_musCopyright.size = 0;
    m_musCopyright.offset = 0;

    m_currentPosition.clear();
    m_trackBeginPosition.clear();
    m_loopBeginPosition.clear();

    m_musTrackTitles.clear();
    m_musMarkers.clear();
    m_dataBank.clear();
    m_eventBank.clear();
    m_branches.clear();

    m_trackData.clear();
    m_trackState.clear();

    m_loop.reset();
    m_loop.invalidLoop = false;
    m_time.reset();

    buildSmfResizeTracks(m_tracksCount);

    std::memset(m_channelDisable, 0, sizeof(m_channelDisable));
}

void BW_MidiSequencer::buildSmfResizeTracks(size_t tracksCount)
{
    m_tracksCount = tracksCount;
    m_trackData.resize(m_tracksCount, MidiTrackQueue());
    m_trackState.resize(m_tracksCount, MidiTrackState());
    m_trackBeginPosition.track.resize(m_tracksCount);
}


void BW_MidiSequencer::initTracksBegin(size_t track)
{
    if(m_trackData[track].size() > 0)
    {
        MidiTrackQueue::iterator pos = m_trackData[track].begin();
        m_trackBeginPosition.track[track].pos = pos;
        // Some events doesn't begin at zero!
        m_trackBeginPosition.track[track].delay = pos->absPos;
        m_trackBeginPosition.track[track].lastHandledEvent = 0;
        std::memcpy(&m_trackBeginPosition.track[track].state, &m_trackState[track].state, sizeof(TrackStateSaved));
    }
    else
    {
        m_trackBeginPosition.track[track].delay = 0;
        m_trackBeginPosition.track[track].lastHandledEvent = -1;
    }
}


void BW_MidiSequencer::buildTimeLine(const std::vector<TempoEvent> &tempos,
                                     uint64_t loopStartTicks,
                                     uint64_t loopEndTicks)
{
    TempoChangePoint firstPoint, tempoMarker, *tailTempo;
    MidiTrackRow fakePos, *posPrev;
    Position rowPosition, rowBeginPosition;
    MIDI_MarkerEntry marker;
    Tempo_t currentTempo;
    Tempo_t t;

    uint64_t shortestDelay = 0, midDelay = 0, postDelay = 0;
    size_t tempo_change_index, tk, i, j;
    unsigned caughLoopStart = 0;
    bool shortestDelayNotFound = true, scanDone = false;
    double time = 0.0;

    std::vector<TempoChangePoint> points;


    /********************************************************************************/
    // Calculate time basing on collected tempo events
    /********************************************************************************/
    for(tk = 0; tk < m_tracksCount; ++tk)
    {
        time = 0.0;
        currentTempo = m_tempo;
        // uint64_t abs_position = 0;
        tempo_change_index = 0;

        MidiTrackQueue &track = m_trackData[tk];

        if(track.empty())
            continue;//Empty track is useless!

#ifdef BWMIDI_DEBUG_TIME_CALCULATION
        std::fprintf(stdout, "\n============Track %u=============\n", (unsigned)tk);
        std::fflush(stdout);
#endif

        posPrev = &(*(track.begin()));//First element

        // If doesn't begins with zero, add a fake one!
        if(posPrev->absPos > 0)
        {
            fakePos.absPos = 0;
            fakePos.delay = posPrev->absPos;
            posPrev = &fakePos;
        }

        for(MidiTrackQueue::iterator it = track.begin(); it != track.end(); it++)
        {
#ifdef BWMIDI_DEBUG_TIME_CALCULATION
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
                    points.clear();

                    firstPoint.absPos = posPrev->absPos;
                    firstPoint.tempo = currentTempo;
                    points.push_back(firstPoint);

                    // Collect tempo change points between previous and current events
                    do
                    {
                        const TempoEvent &tempoPoint = tempos[tempo_change_index];
                        tempoMarker.absPos = tempoPoint.absPosition;
                        tempo_mul(&tempoMarker.tempo, &m_invDeltaTicks, tempoPoint.tempo);
                        points.push_back(tempoMarker);
                        tempo_change_index++;
                    }
                    while((tempo_change_index < tempos.size()) &&
                          (tempos[tempo_change_index].absPosition <= pos.absPos));

                    // Re-calculate time delay of previous event
                    time -= posPrev->timeDelay;
                    posPrev->timeDelay = 0.0;

                    for(i = 0, j = 1; j < points.size(); i++, j++)
                    {
                        /* If one or more tempo events are appears between of two events,
                         * calculate delays between each tempo point, begin and end */

                        // Delay between points
                        midDelay  = points[j].absPos - points[i].absPos;
                        // Time delay between points
                        tempo_mul(&t, &currentTempo, midDelay);
                        posPrev->timeDelay += tempo_get(&t);

                        // Apply next tempo
                        currentTempo = points[j].tempo;
#ifdef BWMIDI_DEBUG_TIME_CALCULATION
                        tempoChanged = true;
#endif
                    }

                    // Then calculate time between last tempo change point and end point
                    tailTempo = &points.back();
                    postDelay = pos.absPos - tailTempo->absPos;
                    tempo_mul(&t, &currentTempo, postDelay);
                    posPrev->timeDelay += tempo_get(&t);

                    // Store Common time delay
                    posPrev->time = time;
                    time += posPrev->timeDelay;
                }
            }

            tempo_mul(&t, &currentTempo, pos.delay);
            pos.timeDelay = tempo_get(&t);
            pos.time = time;
            time += pos.timeDelay;

            // Capture markers after time value calculation
            for(i = pos.events_begin; i < pos.events_end; ++i)
            {
                MidiEvent &e = m_eventBank[i];
                if((e.type == MidiEvent::T_SPECIAL) && (e.subtype == MidiEvent::ST_MARKER))
                {
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

#ifdef BWMIDI_DEBUG_TIME_CALCULATION
            std::fprintf(stdout, "= %10" PRId64 " = %10f%s\n", (unsigned long)pos.absPos, pos.time, tempoChanged ? " <----TEMPO CHANGED" : "");
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
    m_currentPosition = m_trackBeginPosition;
    // Initial loop position will begin at begin of track until passing of the loop point
    m_loopBeginPosition = m_trackBeginPosition;
    // Set lowest level of the loop stack
    m_loop.stackLevel = -1;

    // Set the count of loops
    m_loop.loopsCount = m_loopCount;
    m_loop.loopsLeft = m_loopCount;

    /********************************************************************************/
    // Find and set proper loop points
    /********************************************************************************/
    if(!m_loop.invalidLoop)
    {
        caughLoopStart = 0;
        scanDone = false;
        rowPosition = m_trackBeginPosition;

        while(!scanDone)
        {
            rowBeginPosition = rowPosition;

            for(tk = 0; tk < m_tracksCount; ++tk)
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

                    for(i = track.pos->events_begin; i < track.pos->events_end; ++i)
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
            shortestDelay = 0;
            shortestDelayNotFound = true;

            for(tk = 0; tk < m_tracksCount; ++tk)
            {
                Position::TrackInfo &track = rowPosition.track[tk];
                if((track.lastHandledEvent >= 0) && (shortestDelayNotFound || track.delay < shortestDelay))
                {
                    shortestDelay = track.delay;
                    shortestDelayNotFound = false;
                }
            }

            // Schedule the next playevent to be processed after that delay
            for(tk = 0; tk < m_tracksCount; ++tk)
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
}

#endif /* BW_MIDISEQ_READ_SMF_IMPL_HPP */
