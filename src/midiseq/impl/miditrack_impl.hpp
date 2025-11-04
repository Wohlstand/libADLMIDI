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
#ifndef BW_MIDISEQ_MIDITRACK_IMPL_HPP
#define BW_MIDISEQ_MIDITRACK_IMPL_HPP

#include <cstring>

#include "../midi_sequencer.hpp"

/**********************************************************************************
 *                             MidiTrackRow                                       *
 **********************************************************************************/


BW_MidiSequencer::MidiTrackRow::MidiTrackRow() :
    time(0.0),
    delay(0),
    absPos(0),
    timeDelay(0.0),
    events_begin(0),
    events_end(0)
{}


void BW_MidiSequencer::MidiTrackRow::clear()
{
    time = 0.0;
    delay = 0;
    absPos = 0;
    timeDelay = 0.0;
    events_begin = 0;
    events_end = 0;
}

int BW_MidiSequencer::MidiTrackRow::typePriority(const BW_MidiSequencer::MidiEvent &evt)
{
    switch(evt.type)
    {
    case MidiEvent::T_SYSEX:
    case MidiEvent::T_SYSEX2:
        return 0;

    case MidiEvent::T_NOTEOFF:
        return 1;

    case MidiEvent::T_SPECIAL:
    case MidiEvent::T_SYSCOMSPOSPTR:
    case MidiEvent::T_SYSCOMSNGSEL:
        switch(evt.subtype)
        {
        case MidiEvent::ST_MARKER:
        case MidiEvent::ST_DEVICESWITCH:
        case MidiEvent::ST_SONG_BEGIN_HOOK:
        case MidiEvent::ST_LOOPSTART:
        case MidiEvent::ST_LOOPEND:
        case MidiEvent::ST_LOOPSTACK_BEGIN:
        case MidiEvent::ST_LOOPSTACK_END:
        case MidiEvent::ST_LOOPSTACK_BREAK:
            return 2;

        case MidiEvent::ST_ENDTRACK:
            return 20;

        default:
            return 10;
        }

    case MidiEvent::T_NOTETOUCH:
    case MidiEvent::T_CTRLCHANGE:
    case MidiEvent::T_PATCHCHANGE:
    case MidiEvent::T_CHANAFTTOUCH:
    case MidiEvent::T_WHEEL:
        return 3;

    case MidiEvent::T_NOTEON:
    case MidiEvent::T_NOTEON_DURATED:
        return 4;

    default:
        return 10;
    }
}

void BW_MidiSequencer::MidiTrackRow::sortEvents(std::vector<MidiEvent> &eventsBank, bool *noteStates)
{
    //if(events.size() > 0)
    size_t arr_size = 0;
    MidiEvent *arr = NULL, *arr_end = NULL;

    if(events_begin != events_end && events_begin < events_end)
    {
        MidiEvent piv;
        MidiEvent *i, *j;

        arr = eventsBank.data() + events_begin;
        arr_end = eventsBank.data() + events_end;
        arr_size = events_end - events_begin;

        for(i = arr + 1; i < arr_end; ++i)
        {
            std::memcpy(&piv, i, sizeof(MidiEvent));
            int piv_pri = typePriority(piv);

            for(j = i; j > arr && piv_pri < typePriority(*(j - 1)); --j)
                std::memcpy(j, j - 1, sizeof(MidiEvent));

            std::memcpy(j, &piv, sizeof(MidiEvent));
        }
    }

    /*
     * If Note-Off and it's Note-On is on the same row - move this damned note off down!
     */
    if(noteStates && arr && arr_size > 0)
    {
        size_t max_size = arr_size;

        if(arr_size > 1)
        {
            for(size_t i = arr_size - 1; ; --i)
            {
                const MidiEvent &e = arr[i];

                if(e.type == MidiEvent::T_NOTEOFF)
                    break;

                if(e.type != MidiEvent::T_NOTEON)
                {
                    if(i == 0)
                        break;
                    continue;
                }

                const size_t note_i = (static_cast<size_t>(e.channel) << 7) | (e.data_loc[0] & 0x7F);

                //Check, was previously note is on or off
                bool wasOn = noteStates[note_i];

                // Detect zero-length notes are following previously pressed note
                int noteOffsOnSameNote = 0;

                for(size_t j = 0; j < max_size; )
                {
                    const MidiEvent &o = arr[j];
                    if(o.type == MidiEvent::T_NOTEON)
                        break;

                    if(o.type != MidiEvent::T_NOTEOFF)
                    {
                        ++j;
                        continue;
                    }

                    const size_t note_j = (static_cast<size_t>(o.channel) << 7) | (o.data_loc[0] & 0x7F);

                    // If note was off, and note-off on same row with note-on - move it down!
                    if(note_j == note_i)
                    {
                        // If note is already off OR more than one note-off on same row and same note
                        if(!wasOn || (noteOffsOnSameNote != 0))
                        {
                            MidiEvent tmp;

                            if(j < arr_size - 1)
                            {
                                MidiEvent *dst = arr + j;
                                MidiEvent *src = dst + 1;

                                // Move this event to end of the list
                                std::memcpy(&tmp, &o, sizeof(MidiEvent));

                                for(size_t k = j + 1; k < arr_size; ++k)
                                    std::memcpy(dst++, src++, sizeof(MidiEvent));

                                std::memcpy(&arr[arr_size - 1], &tmp, sizeof(MidiEvent));

                                --max_size;

                                // Caused offset of i (j is always smaller than i!)
                                if(j < i)
                                    --i;

                            }
                            else
                                ++j;

                            continue;
                        }
                        else
                        {
                            // When same row has many note-offs on same row
                            // that means a zero-length note follows previous note
                            // it must be shuted down
                            ++noteOffsOnSameNote;
                        }
                    }

                    ++j;
                } // Sub-Loop through note-offs

                if(i == 0)
                    break;
            } // loop through note-ons
        } // arr_size > 1

        // Apply note states according to event types
        for(size_t i = 0; i < arr_size ; ++i)
        {
            const MidiEvent &e = arr[i];

            if(e.type == MidiEvent::T_NOTEON)
                noteStates[(static_cast<size_t>(e.channel) << 7) | (e.data_loc[0] & 0x7F)] = true;
            else if(e.type == MidiEvent::T_NOTEOFF)
                noteStates[(static_cast<size_t>(e.channel) << 7) | (e.data_loc[0] & 0x7F)] = false;
        }
    }
}

#endif /* BW_MIDISEQ_READ_SMF_IMPL_HPP */
