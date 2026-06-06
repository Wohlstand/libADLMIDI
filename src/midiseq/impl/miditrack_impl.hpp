/*
 * BW_Midi_Sequencer - MIDI Sequencer for C++
 *
 * Copyright (c) 2015-2026 Vitaly Novichkov <admin@wohlnet.ru>
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
 *                                 Position                                       *
 **********************************************************************************/

void BW_MidiSequencer::Position::tracks_resize(size_t size)
{
    if(track_size != size)
    {
        if(size == 0)
        {
            clear();
            return;
        }

        size_t rawSize = size * sizeof(TrackInfo);
        if(track)
        {
#if defined(__DJGPP__)
            dpmi_allocator_impl::dpmi_unlock_memory(track, rawSize);
#endif
            track = (TrackInfo*)std::realloc(track, rawSize);
        }
        else
            track = (TrackInfo*)std::malloc(rawSize);

#if defined(__DJGPP__)
        dpmi_allocator_impl::dpmi_lock_memory(track, rawSize);
#endif
        for(size_t i = track_size; i < size; ++i)
            tracks_init_one(track[i]);

        track_size = size;
    }
}

void BW_MidiSequencer::Position::tracks_reset()
{
    for(size_t i = 0; i < track_size; ++i)
        tracks_init_one(track[i]);
}

void BW_MidiSequencer::Position::tracks_init_one(TrackInfo &t)
{
    t.delay = 0;
    t.lastHandledEvent = 0;
    std::memset(&t.state, 0, sizeof(t.state));
    std::memset(t.state.cc_values, 0xFF, sizeof(t.state.cc_values));
    std::memset(t.state.reserve_note_att, 0xFF, sizeof(t.state.reserve_note_att));
    t.state.reserve_patch = 0xFF;
    t.state.reserve_wheel[0] = 0xFF;
    t.state.reserve_wheel[1] = 0xFF;
    t.state.reserve_channel_att = 0xFF;
}

BW_MidiSequencer::Position::Position():
    wait(0.0),
    absTimePosition(0.0),
    absTickPosition(0),
    began(false),
    track(NULL),
    track_size(0)
{}

BW_MidiSequencer::Position::~Position()
{
    clear();
}

BW_MidiSequencer::Position::Position(const Position &o):
    wait(o.wait),
    absTimePosition(o.absTimePosition),
    absTickPosition(o.absTickPosition),
    began(o.began),
    track(NULL),
    track_size(0)
{
    tracks_resize(o.track_size);

    if(track && o.track)
        std::memcpy(track, o.track, o.track_size * sizeof(TrackInfo));
}

BW_MidiSequencer::Position &BW_MidiSequencer::Position::operator=(const Position &o)
{
    wait = o.wait;
    absTimePosition = o.absTimePosition;
    absTickPosition = o.absTickPosition;
    began = o.began;

    tracks_resize(o.track_size);
    if(track && o.track)
        std::memcpy(track, o.track, o.track_size * sizeof(TrackInfo));

    return *this;
}

void BW_MidiSequencer::Position::clear()
{
    wait = 0.0;
    began = false;
    absTimePosition = 0.0;
    absTickPosition = 0;

    if(track)
    {
#if defined(__DJGPP__)
        dpmi_allocator_impl::dpmi_unlock_memory(track, track_size * sizeof(TrackInfo));
#endif
        std::free(track);
        track = NULL;
    }

    track_size = 0;
}

void BW_MidiSequencer::Position::assignOneTrack(const Position *o, size_t tk)
{
    absTimePosition = o->absTimePosition;
    wait = o->wait;
    began = o->began;
    absTickPosition = o->absTickPosition;
    tracks_resize(1);
    std::memcpy(&track[0], &o->track[tk], sizeof(TrackInfo));
}

/**********************************************************************************
 *                                 MidiTrackRow                                   *
 **********************************************************************************/

int BW_MidiSequencer::typePriority(const BW_MidiSequencer::MidiEvent &evt)
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
        case MidiEvent::ST_SONG_BEGIN_HOOK:
            return -1; // This should be really first event in the row!

        case MidiEvent::ST_MARKER:
        case MidiEvent::ST_DEVICESWITCH:
        case MidiEvent::ST_LOOPSTART:
        case MidiEvent::ST_LOOPEND:
        case MidiEvent::ST_LOOPSTACK_BEGIN:
        case MidiEvent::ST_LOOPSTACK_BEGIN_ID:
        case MidiEvent::ST_LOOPSTACK_END:
        case MidiEvent::ST_LOOPSTACK_END_ID:
        case MidiEvent::ST_LOOPSTACK_BREAK:
        case MidiEvent::ST_TRACK_LOOPSTACK_BEGIN:
        case MidiEvent::ST_TRACK_LOOPSTACK_BEGIN_ID:
        case MidiEvent::ST_TRACK_LOOPSTACK_END:
        case MidiEvent::ST_TRACK_LOOPSTACK_END_ID:
        case MidiEvent::ST_TRACK_LOOPSTACK_BREAK:
        case MidiEvent::ST_BRANCH_LOCATION:
        case MidiEvent::ST_BRANCH_TO:
        case MidiEvent::ST_TRACK_BRANCH_LOCATION:
        case MidiEvent::ST_TRACK_BRANCH_TO:
        case MidiEvent::ST_CHANNEL_LOCK:
        case MidiEvent::ST_CHANNEL_UNLOCK:
        case MidiEvent::ST_CHANNEL_PRIORITY:
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

void BW_MidiSequencer::sortEvents(MidiTrackRow &row, MidiEventsList &eventsBank, bool *noteStates)
{
    size_t arr_size = 0, max_size, i, j, k, note_i, note_j;
    MidiEvent tmp, *dst, *src, *mi, *mj, *arr = NULL, *arr_end = NULL;
    int tmp_pri, noteOffsOnSameNote;
    bool wasOn;

    /*
     * Sort events by type priority
     */
    if(row.events_begin != row.events_end && row.events_begin < row.events_end)
    {
        arr = eventsBank.data + row.events_begin;
        arr_end = eventsBank.data + row.events_end;
        arr_size = row.events_end - row.events_begin;

        for(mi = arr + 1; mi < arr_end; ++mi)
        {
            std::memcpy(&tmp, mi, sizeof(MidiEvent));
            tmp_pri = typePriority(tmp);

            for(mj = mi; mj > arr && tmp_pri < typePriority(*(mj - 1)); --mj)
                std::memcpy(mj, mj - 1, sizeof(MidiEvent));

            std::memcpy(mj, &tmp, sizeof(MidiEvent));
        }
    }

    /*
     * If Note-Off and it's Note-On is on the same row - move this damned note off down!
     */
    if(noteStates && arr && arr_size > 0)
    {
        max_size = arr_size;

        if(arr_size > 1)
        {
            for(i = arr_size - 1; ; --i)
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

                note_i = (static_cast<size_t>(e.channel) << 7) | (e.data_loc[0] & 0x7F);

                //Check, was previously note is on or off
                wasOn = noteStates[note_i];

                // Detect zero-length notes are following previously pressed note
                noteOffsOnSameNote = 0;

                for(j = 0; j < max_size; )
                {
                    const MidiEvent &o = arr[j];
                    if(o.type == MidiEvent::T_NOTEON)
                        break;

                    if(o.type != MidiEvent::T_NOTEOFF)
                    {
                        ++j;
                        continue;
                    }

                    note_j = (static_cast<size_t>(o.channel) << 7) | (o.data_loc[0] & 0x7F);

                    // If note was off, and note-off on same row with note-on - move it down!
                    if(note_j == note_i)
                    {
                        // If note is already off OR more than one note-off on same row and same note
                        if(!wasOn || (noteOffsOnSameNote != 0))
                        {
                            if(j < arr_size - 1)
                            {
                                dst = arr + j;
                                src = dst + 1;

                                // Move this event to end of the list
                                std::memcpy(&tmp, &o, sizeof(MidiEvent));

                                for(k = j + 1; k < arr_size; ++k)
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
        for(i = 0; i < arr_size ; ++i)
        {
            const MidiEvent &e = arr[i];

            if(e.type == MidiEvent::T_NOTEON)
                noteStates[(static_cast<size_t>(e.channel) << 7) | (e.data_loc[0] & 0x7F)] = true;
            else if(e.type == MidiEvent::T_NOTEOFF)
                noteStates[(static_cast<size_t>(e.channel) << 7) | (e.data_loc[0] & 0x7F)] = false;
        }
    }
}


BW_MidiSequencer::MidiTrackState::MidiTrackState() :
    deviceMask(BW_MidiSequencer::Device_ANY),
    disabled(false),
    stateRestoreSetup(TRACK_RESTORE_DEFAULT)
{
    loop.reset();
    loop.invalidLoop = false;

    std::memset(&state, 0, sizeof(state));
    std::memset(&state.cc_values, 0xFF, sizeof(state.cc_values));
    std::memset(&state.reserve_note_att, 0xFF, sizeof(state.reserve_note_att));
    state.reserve_patch = 0xFF;
    state.reserve_wheel[0] = 0xFF;
    state.reserve_wheel[1] = 0xFF;
    state.reserve_channel_att = 0xFF;

    std::memset(&duratedNotes, 0, sizeof(DuratedNotesCache));
}

#endif /* BW_MIDISEQ_READ_SMF_IMPL_HPP */
