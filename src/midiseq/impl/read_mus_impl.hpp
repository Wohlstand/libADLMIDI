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
#ifndef BW_MIDISEQ_READ_MUS_IMPL_HPP
#define BW_MIDISEQ_READ_MUS_IMPL_HPP

#include <cstring>

#include "../midi_sequencer.hpp"
#include "common.hpp"


enum MUS_EventType
{
    MUS_NoteOFF     = 0,
    MUS_NoteON      = 1,
    MUS_PitchBend   = 2,
    MUS_SystemEvent = 3,
    MUS_Controller  = 4,
    MUS_EndMeasure  = 5,
    MUS_EndOfTrack  = 6,
    MUS_Unused      = 7
};


bool BW_MidiSequencer::parseMUS(FileAndMemReader &fr)
{
    const size_t headerSize = 16;
    size_t fsize = 0;
    uint8_t headerBuf[headerSize];
    uint16_t mus_lenSong, mus_offSong, mus_channels1, /*mus_channels2,*/ mus_numInstr, song_read = 0;
    uint8_t numBuffer[2];
    int8_t  channel_map[16];
    uint8_t channel_volume[16];
    uint8_t channel_cur = 0;
    uint64_t abs_position = 0;
    int32_t delay = 0;
    std::vector<uint16_t> mus_instrs;
    std::vector<TempoEvent> temposList;

    const uint8_t controller_map[15] =
    {
        0,    0,    0x01, 0x07, 0x0A,
        0x0B, 0x5B, 0x5D, 0x40, 0x43,
        0x78, 0x7B, 0x7E, 0x7F, 0x79,
    };

    size_t mus_len = fr.fileSize();

    fsize = fr.read(headerBuf, 1, headerSize);
    if(fsize < headerSize)
    {
        m_errorString.set("MUS: Unexpected end of file at header!\n");
        return false;
    }

    if(std::memcmp(headerBuf, "MUS\x1A", 4) != 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Invalid format, MUS\\x1A signature is not found!\n");
        return false;
    }

    mus_lenSong = readLEint16(headerBuf + 4, 2);
    mus_offSong = readLEint16(headerBuf + 6, 2);
    mus_channels1 = readLEint16(headerBuf + 8, 2);
    // mus_channels2 = readLEint16(headerBuf + 12, 2);
    mus_numInstr = readLEint16(headerBuf + 14, 2);

    if(headerSize + (static_cast<size_t>(mus_numInstr) * 2) > mus_offSong)
    {
        m_errorString.set("MUS file is invalid: instruments list is larger than song offset!\n");
        return false;
    }

    if(mus_len < mus_lenSong + mus_offSong)
    {
        m_errorString.setFmt("MUS file is invalid: song length (%ul) is longer than file size (%ul)!\n", (unsigned long)(mus_lenSong + mus_offSong), (unsigned long)(mus_len));
        return false;
    }

    if(mus_channels1 > 15)
    {
        m_errorString.setFmt("MUS file is invalid: more than 15 primary channels! (Actual value: %u)\n", mus_channels1);
        return false;
    }

    mus_instrs.reserve(mus_numInstr);

    for(uint16_t i = 0; i < mus_numInstr; ++i)
    {
        fsize = fr.read(numBuffer, 1, 2);
        if(fsize < 2)
        {
            m_errorString.set("Unexpected end of file at instruments numbers list!\n");
            return false;
        }

        mus_instrs.push_back(readLEint16(numBuffer, 2));
    }

    fr.seek(mus_offSong, FileAndMemReader::SET);

    buildSmfSetupReset(1);

    m_invDeltaTicks.nom = 1;
    m_invDeltaTicks.denom = 1000000l * 0x101;
    tempo_mul(&m_tempo, &m_invDeltaTicks, 0x101 * 2); // MUS has the fixed tempo

    for(int i = 0; i < 16; ++i)
    {
        channel_map[i] = -1;
        channel_volume[i] = 0x40;
    }

    channel_map[15] = 9; // 15'th channel is mapped to 9'th

    MidiTrackRow evtPos;
    MidiEvent event;
    std::memset(&event, 0, sizeof(event));
    event.isValid = 1;
    event.type = MidiEvent::T_SPECIAL;
    event.subtype = MidiEvent::ST_SONG_BEGIN_HOOK;
    // HACK: Begin every track with "Reset all controllers" event to avoid controllers state break came from end of song
    addEventToBank(evtPos, event);

    std::memset(&event, 0, sizeof(event));
    event.isValid = 1;
    event.type = MidiEvent::T_SPECIAL;
    event.subtype = MidiEvent::ST_TEMPOCHANGE;
    event.data_loc_size = 3;
    event.data_loc[0] = 0x1B;
    event.data_loc[1] = 0x8A;
    event.data_loc[2] = 0x06;
    addEventToBank(evtPos, event);
    TempoEvent tempoEvent = {readBEint(event.data_loc, event.data_loc_size), abs_position};
    temposList.push_back(tempoEvent);

    // Begin percussion channel with volume 100
    std::memset(&event, 0, sizeof(event));
    event.isValid = 1;
    event.type = MidiEvent::T_CTRLCHANGE;
    event.channel = 9;
    event.data_loc_size = 2;
    event.data_loc[0] = 7;
    event.data_loc[1] = 100;
    addEventToBank(evtPos, event);

    delay = 0;
    channel_cur = 0;

    while(song_read < mus_lenSong)
    {
        uint8_t mus_event, mus_channel, bytes[2];
        std::memset(&event, 0, sizeof(event));
        event.isValid = 1;

        fsize = fr.read(&mus_event, 1, 1);
        if(fsize < 1)
        {
            m_errorString.set("Failed to read MUS data: Failed to read event type!\n");
            return false;
        }

        ++song_read;
        mus_channel = (mus_event & 15);
        evtPos.delay = delay;

        // Insert volume for every used channel
        if(channel_map[mus_channel] < 0)
        {
            event.type = MidiEvent::T_CTRLCHANGE;
            event.data_loc_size = 2;
            event.data_loc[0] = 7;
            event.data_loc[1] = 100;
            addEventToBank(evtPos, event);
            std::memset(&event, 0, sizeof(event));
            event.isValid = 1;

            channel_map[mus_channel] = channel_cur++;
            if(channel_cur == 9)
                ++channel_cur;
        }

        event.channel = channel_map[mus_channel];

        switch((mus_event >> 4) & 0x07)
        {
        case MUS_NoteOFF:
            fsize = fr.read(&bytes, 1, 1);
            if(fsize < 1)
            {
                m_errorString.set("Failed to read MUS data: Can't read Note OFF event data!\n");
                return false;
            }

            ++song_read;
            event.type = MidiEvent::T_NOTEOFF;
            event.data_loc_size = 2;
            event.data_loc[0] = bytes[0] & 0x7F;
            event.data_loc[1] = 0;
            addEventToBank(evtPos, event);
            break;

        case MUS_NoteON:
            fsize = fr.read(&bytes, 1, 1);
            if(fsize < 1)
            {
                m_errorString.set("Failed to read MUS data: Can't read Note ON event data!\n");
                return false;
            }

            ++song_read;
            event.type = MidiEvent::T_NOTEON;
            event.data_loc_size = 2;
            event.data_loc[0] = bytes[0] & 0x7F;

            if((bytes[0] & 0x80) != 0)
            {
                fsize = fr.read(&bytes, 1, 1);
                if(fsize < 1)
                {
                    m_errorString.set("Failed to read MUS data: Can't read Note ON's velocity data!\n");
                    return false;
                }

                ++song_read;
                channel_volume[event.channel] = bytes[0];
            }

            event.data_loc[1] = channel_volume[event.channel];
            addEventToBank(evtPos, event);
            break;

        case MUS_PitchBend:
            fsize = fr.read(&bytes, 1, 1);
            if(fsize < 1)
            {
                m_errorString.set("Failed to read MUS data: Can't read Pitch Bend event data!\n");
                return false;
            }

            ++song_read;
            event.type = MidiEvent::T_WHEEL;
            event.data_loc_size = 2;
            event.data_loc[0] = (bytes[0] & 1) >> 6;
            event.data_loc[1] = (bytes[0] >> 1);
            addEventToBank(evtPos, event);
            break;

        case MUS_SystemEvent:
            fsize = fr.read(&bytes, 1, 1);
            if(fsize < 1)
            {
                m_errorString.set("Failed to read MUS data: Can't read System Event data!\n");
                return false;
            }

            ++song_read;
            event.type = MidiEvent::T_CTRLCHANGE;
            event.data_loc_size = 2;

            if((bytes[0] & 0x7F) >= 15)
                break; // Skip invalid controller

            event.data_loc[0] = controller_map[bytes[0] & 0x7F];
            addEventToBank(evtPos, event);
            break;

        case MUS_Controller:
            fsize = fr.read(&bytes, 1, 2);
            if(fsize < 1)
            {
                m_errorString.set("Failed to read MUS data: Can't read System Event data!\n");
                return false;
            }

            song_read += 2;
            event.type = MidiEvent::T_CTRLCHANGE;
            event.data_loc_size = 2;

            if((bytes[0] & 0x7F) >= 15)
                break; // Skip invalid controller

            if((bytes[0] & 0x7F) == 0)
            {
                size_t j = 0;
                event.type = MidiEvent::T_PATCHCHANGE;

                for( ; j < mus_instrs.size(); ++j)
                {
                    if((bytes[1] & 0x7F) == mus_instrs[j])
                        break;
                }

                if(mus_numInstr > 0 && j >= mus_numInstr)
                {
                    m_errorString.set("Failed to read MUS data: Instrument number is not presented in the list!\n");
                    return false;
                }

                event.data_loc[0] = bytes[1] & 0x7F;
                event.data_loc[1] = 0;
                event.data_loc_size = 1;
            }
            else
            {
                event.data_loc[0] = controller_map[bytes[0] & 0x7F];
                event.data_loc[1] = bytes[1] & 0x7F;
            }

            addEventToBank(evtPos, event);
            break;
        case MUS_EndMeasure:
            break;

        case MUS_EndOfTrack:
            event.type = MidiEvent::T_SPECIAL;
            event.subtype = MidiEvent::ST_ENDTRACK;
            addEventToBank(evtPos, event);
            break;

        case MUS_Unused:
            break;
        }

        delay = 0;

        if((mus_event & 0x80) != 0)
        {
            do
            {
                fsize = fr.read(&bytes, 1, 1);
                if(fsize < 1)
                {
                    m_errorString.set("Failed to read MUS data: Can't read one of delay bytes!\n");
                    return false;
                }

                ++song_read;
                delay = (delay * 128) + (bytes[0] & 0x7F);

            } while((bytes[0] & 0x80) != 0);
        }

        if(delay > 0 || event.subtype == MidiEvent::ST_ENDTRACK || song_read >= mus_lenSong)
        {
            evtPos.delay = delay;
            evtPos.absPos = abs_position;
            abs_position += evtPos.delay;
            m_trackData[0].push_back(evtPos);
            evtPos.clear();
        }
    }

    if(!m_trackData[0].empty())
        initTracksBegin(0);

    buildTimeLine(temposList);

    m_smfFormat = 0;
    m_loop.stackLevel = -1;

    return true;
}

#endif /* BW_MIDISEQ_READ_MUS_IMPL_HPP */
