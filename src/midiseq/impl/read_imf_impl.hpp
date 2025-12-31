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
#ifndef BW_MIDISEQ_READ_IMF_IMPL_HPP
#define BW_MIDISEQ_READ_IMF_IMPL_HPP


#ifdef BWMIDI_ENABLE_OPL_MUSIC_SUPPORT

#include <cstring>
#include "../midi_sequencer.hpp"
#include "common.hpp"

bool BW_MidiSequencer::parseIMF(FileAndMemReader &fr)
{
    const size_t    deltaTicks = 1;
    const size_t    trackCount = 1;
    // const uint32_t  imfTempo = 1428;
    size_t          imfEnd = 0;
    uint64_t        abs_position = 0;
    uint8_t         imfRaw[4];

    MidiTrackRow    evtPos;
    MidiEvent       event;

    std::memset(&event, 0, sizeof(event));
    event.isValid = 1;

    std::vector<TempoEvent> temposList;

    m_format = Format_IMF;

    buildSmfSetupReset(trackCount);

    m_invDeltaTicks.nom = 1;
    m_invDeltaTicks.denom = 1000000l * deltaTicks;
    m_tempo.nom = 1;
    m_tempo.denom = deltaTicks * 2;

    fr.seek(0, FileAndMemReader::SET);
    if(fr.read(imfRaw, 1, 2) != 2)
    {
        m_errorString.set("Unexpected end of file at header!\n");
        return false;
    }

    imfEnd = static_cast<size_t>(imfRaw[0]) + 256 * static_cast<size_t>(imfRaw[1]);

    // Define the playing tempo
    event.type = MidiEvent::T_SPECIAL;
    event.subtype = MidiEvent::ST_TEMPOCHANGE;
    event.data_loc_size = 3;
    event.data_loc[0] = 0x00; // static_cast<uint8_t>((imfTempo >> 16) & 0xFF);
    event.data_loc[1] = 0x05; // static_cast<uint8_t>((imfTempo >> 8) & 0xFF);
    event.data_loc[2] = 0x94; // static_cast<uint8_t>((imfTempo & 0xFF));
    addEventToBank(evtPos, event);
    TempoEvent tempoEvent = {readBEint(event.data_loc, event.data_loc_size), 0};
    temposList.push_back(tempoEvent);

    // Define the draft for IMF events
    event.type = MidiEvent::T_SPECIAL;
    event.subtype = MidiEvent::ST_RAWOPL;
    event.data_loc_size = 2;

    fr.seek((imfEnd > 0) ? 2 : 0, FileAndMemReader::SET);

    if(imfEnd == 0) // IMF Type 0 with unlimited file length
        imfEnd = fr.fileSize();

    while(fr.tell() < imfEnd && !fr.eof())
    {
        if(fr.read(imfRaw, 1, 4) != 4)
            break;

        event.data_loc[0] = imfRaw[0]; // port index
        event.data_loc[1] = imfRaw[1]; // port value
        event.isValid = 1;

        addEventToBank(evtPos, event);
        evtPos.delay = static_cast<uint64_t>(imfRaw[2]) + 256 * static_cast<uint64_t>(imfRaw[3]);

        if(evtPos.delay > 0)
        {
            evtPos.absPos = abs_position;
            abs_position += evtPos.delay;
            m_trackData[0].push_back(evtPos);
            evtPos.clear();
        }
    }

    // Add final row
    evtPos.absPos = abs_position;
    m_trackData[0].push_back(evtPos);
    initTracksBegin(0);

    buildTimeLine(temposList);

    return true;
}

#endif /* BWMIDI_ENABLE_OPL_MUSIC_SUPPORT */

#endif /* BW_MIDISEQ_READ_IMF_IMPL_HPP */
