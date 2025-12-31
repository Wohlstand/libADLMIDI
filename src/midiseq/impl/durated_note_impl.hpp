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
#ifndef BW_MIDISEQ_DURATED_NOTE_IMPL_HPP
#define BW_MIDISEQ_DURATED_NOTE_IMPL_HPP

#include <cstring>

#include "../midi_sequencer.hpp"

bool BW_MidiSequencer::duratedNoteAlloc(size_t track, DuratedNote **note)
{
    DuratedNotesCache &cache = m_trackState[track].duratedNotes;

    if(cache.notes_count >= 128)
        return false; // Can't insert delayed note off!

    *note = cache.notes + cache.notes_count++;

    return true;
}

void BW_MidiSequencer::duratedNoteClear()
{
    for(std::vector<MidiTrackState>::iterator it = m_trackState.begin(); it != m_trackState.end(); ++it)
        it->duratedNotes.notes_count = 0;
}

void BW_MidiSequencer::duratedNoteTick(size_t track, int64_t ticks)
{
    DuratedNotesCache &cache = m_trackState[track].duratedNotes;

    for(size_t i = 0; i < cache.notes_count; ++i)
        cache.notes[i].ttl -= ticks;
}

void BW_MidiSequencer::duratedNotePop(size_t track, size_t i)
{
    DuratedNotesCache &cache = m_trackState[track].duratedNotes;

    if(i < cache.notes_count)
    {
        if(cache.notes_count > 1)
            std::memcpy(cache.notes + i, cache.notes + cache.notes_count - 1, sizeof(DuratedNote));
        --cache.notes_count;
    }
}

#endif /* BW_MIDISEQ_DURATED_NOTE_IMPL_HPP */
