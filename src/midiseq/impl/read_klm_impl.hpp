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
#ifndef BW_MIDISEQ_READ_KLM_IMPL_HPP
#define BW_MIDISEQ_READ_KLM_IMPL_HPP

#ifdef BWMIDI_ENABLE_OPL_MUSIC_SUPPORT

#include <cstring>
#include "../midi_sequencer.hpp"
#include "common.hpp"


bool BW_MidiSequencer::parseKLM(FileAndMemReader &fr)
{
// #define KLM_DEBUG
    const size_t headerSize = 5;
    char headerBuf[headerSize] = {0, 0, 0, 0, 0};
    size_t fsize = 0, file_size;
    uint64_t tempo = 0, musOffset = 0;
    uint64_t abs_position = 0;

    MidiTrackRow    evtPos;
    MidiEvent       event;

    std::memset(&event, 0, sizeof(event));
    event.isValid = 1;

    fsize = fr.read(headerBuf, 1, headerSize);
    if(fsize < headerSize)
    {
        fr.close();
        m_errorString.set("Unexpected end of file at header!\n");
        return false;
    }

    file_size = fr.fileSize();
    tempo = readLEint(headerBuf + 0, 2);
    musOffset = readLEint(headerBuf + 3, 2);

    if(musOffset >= file_size)
    {
        fr.close();
        m_errorString.set("Song data offset is out of file size\n");
        return false;
    }

    m_format = Format_KLM;

    buildSmfSetupReset(1);

    m_invDeltaTicks.nom = 1;
    m_invDeltaTicks.denom = 1000000l * tempo;
    m_tempo.nom = 1;
    m_tempo.denom = tempo * 2;

    uint64_t ins_count = 0;

    // Used temporarily
    m_cmfInstruments.reserve(static_cast<size_t>(ins_count));
    CmfInstrument inst;

    while(fr.tell() < musOffset && !fr.eof())
    {
        fsize = fr.read(inst.data, 1, 11);
        if(fsize < 11)
        {
            fr.close();
            m_cmfInstruments.clear();
            m_errorString.set("Unexpected file ending on attempt to read KLM instruments raw data!");
            return false;
        }
        m_cmfInstruments.push_back(inst);
    }

    if(fr.tell() != musOffset)
    {
        fr.close();
        m_cmfInstruments.clear();
        m_errorString.set("Invalid KLM file: instrument data goes after the song offset!");
        return false;
    }

    // Define the draft for IMF events
    event.type = MidiEvent::T_SPECIAL;
    event.subtype = MidiEvent::ST_RAWOPL;
    event.data_loc_size = 2;

#ifdef KLM_DEBUG
    size_t err_off = 0;
#endif
    uint8_t cmd = 0, chan = 0, data[2], eof_reached = 0;
    uint8_t reg_bd_state = 0x00;
    uint8_t reg_b0_state[11] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t reg_43_state[11] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t inst_off_car = 0;
    uint8_t inst_off_mod = 0;

    const uint8_t op_map[12] =
    {
        0x00, 0x03,
        0x01, 0x04,
        0x02, 0x05,
        0x08, 0x0B,
        0x09, 0x0C,
        0x0A, 0x0D
    };

    const uint8_t rm_map[10] =
    {
        0x10, 0x13,
        0xFF, 0x14,
        0x12, 0xFF,
        0xFF, 0x15,
        0x11, 0xFF
    };

    const uint8_t rm_vol_map[5] =
    {
        0x13,
        0x14,
        0x12,
        0x15,
        0x11,
    };

    // Activate rhythm mode
    reg_bd_state = 0x20;
    event.data_loc[0] = 0xBD;
    event.data_loc[1] = reg_bd_state;
    event.isValid = 1;
    addEventToBank(evtPos, event);
    evtPos.delay = 0;

    // Initial rhythm frequencies
    const int rhythm_a0[] = {0x57, 0x03, 0x57};
    const int rhythm_b0[] = {0x0A, 0x0A, 0x09};

    for(int c = 6; c <= 8; ++c)
    {
        event.data_loc[0] = 0xA0 + c;
        event.data_loc[1] = rhythm_a0[c - 6];
        addEventToBank(evtPos, event);

        reg_b0_state[c] = rhythm_b0[c - 6] & 0xDF;
        event.data_loc[0] = 0xB0 + c;
        event.data_loc[1] = rhythm_b0[c - 6] & 0xDF;
        addEventToBank(evtPos, event);
    }

#ifdef KLM_DEBUG
    err_off = fr.tell();
    printf("Instriments in KML: %u\n", static_cast<unsigned>(m_cmfInstruments.size()));
    fflush(stdout);
#endif

    while(!eof_reached && !fr.eof())
    {
#ifdef KLM_DEBUG
        err_off = fr.tell();
#endif
        fsize = fr.read(&cmd, 1, 1);
        if(fsize < 1)
        {
            fr.close();
            m_cmfInstruments.clear();
            m_errorString.set("Unexpected file ending on attempt to read KLM song command data!");
            return false;
        }

        chan = cmd & 0x0F;

#ifdef KLM_DEBUG
        printf("0x%X: CMD=0x%02X, chan=0x%02X (Full byte 0x%02X)\n", static_cast<unsigned>(err_off), cmd & 0xF0, chan, cmd);
        fflush(stdout);
#endif

        if((cmd & 0xF0) != 0xF0 && chan >= 11)
        {
            fr.close();
            m_cmfInstruments.clear();
            m_errorString.set("Channel out of range!");
            return false;
        }

        switch(cmd & 0xF0)
        {
        case 0x00: // Note OFF;
            switch(chan)
            {
            case 0: case 1: case 2: case 3: case 4: case 5:
                reg_b0_state[chan] &= 0xDF;
                event.data_loc[0] = 0xB0 + chan;
                event.data_loc[1] = reg_b0_state[chan];
                event.isValid = 1;
                addEventToBank(evtPos, event);
                break;

            default:
                switch(chan)
                {
                case 6:
                    reg_bd_state &= ~0x10;
                    break;
                case 7:
                    reg_bd_state &= ~0x08;
                    break;
                case 8:
                    reg_bd_state &= ~0x04;
                    break;
                case 9:
                    reg_bd_state &= ~0x02;
                    break;
                case 0x0A:
                    reg_bd_state &= ~0x01;
                    break;
                }

                event.data_loc[0] = 0xBD;
                event.data_loc[1] = reg_bd_state;
                event.isValid = 1;
                addEventToBank(evtPos, event);
                break;
            }

            break;
        case 0x10: // Note ON with frequency (only channels 0 - 5, and bass drum at 6, other channels cmd gets replaced with 0x40)
            if(chan > 6)
            {
                switch(chan)
                {
                case 6:
                    reg_bd_state |= 0x10;
                    break;
                case 7:
                    reg_bd_state |= 0x08;
                    break;
                case 8:
                    reg_bd_state |= 0x04;
                    break;
                case 9:
                    reg_bd_state |= 0x02;
                    break;
                case 0x0A:
                    reg_bd_state |= 0x01;
                    break;
                }

                event.data_loc[0] = 0xBD;
                event.data_loc[1] = reg_bd_state;
                addEventToBank(evtPos, event);
                break;
            }

            fsize = fr.read(data, 1, 2);
            if(fsize < 2)
            {
                fr.close();
                m_cmfInstruments.clear();
                m_errorString.set("Unexpected file ending on attempt to read KLM song note-on frequency data!");
                return false;
            }

#ifdef KLM_DEBUG
            printf(" -- Data 2 byte\n");
            fflush(stdout);
#endif

            event.isValid = 1;

            event.data_loc[0] = 0xA0 + chan;
            event.data_loc[1] = data[0];
            addEventToBank(evtPos, event);

            if(chan < 6)
            {
                reg_b0_state[chan] = data[1] & 0xDF;
                reg_b0_state[chan] |= 0x20;
            }
            else if(chan <= 8)
                reg_b0_state[chan] = data[1] & 0xDF;

            event.data_loc[0] = 0xB0 + chan;
            event.data_loc[1] = reg_b0_state[chan];
            addEventToBank(evtPos, event);

            break;

        case 0x20: // Volume
            fsize = fr.read(data, 1, 1);
            if(fsize < 1)
            {
                fr.close();
                m_cmfInstruments.clear();
                m_errorString.set("Unexpected file ending on attempt to read KLM song volume data!");
                return false;
            }

#ifdef KLM_DEBUG
            printf(" -- Data 1 byte\n");
            fflush(stdout);
#endif

            reg_43_state[chan] &= 0xC0;
            reg_43_state[chan] |= 0x3F & ((127 - data[0]) / 2);

            if(chan < 6)
                event.data_loc[0] = 0x40 + op_map[(chan * 2) + 1];
            else if(chan <= 11)
                event.data_loc[0] = 0x40 + rm_vol_map[chan - 6];

            event.data_loc[1] = reg_43_state[chan];
            event.isValid = 1;
            addEventToBank(evtPos, event);
            break;

        case 0x30: // Set Instrument
            fsize = fr.read(data, 1, 1);
            if(fsize < 1)
            {
                fr.close();
                m_cmfInstruments.clear();
                m_errorString.set("Unexpected file ending on attempt to read KLM song instrument select data!");
                return false;
            }

#ifdef KLM_DEBUG
            printf(" -- Data 1 byte (0x%02X)\n", data[0]);
            fflush(stdout);
#endif

            if(data[0] >= m_cmfInstruments.size())
            {
                fr.close();
                m_cmfInstruments.clear();
                m_errorString.set("Selected instrument in KLM file is out of range!");
                return false;
            }

            if(chan < 6)
            {
                inst_off_mod = op_map[chan * 2];
                inst_off_car = op_map[(chan * 2) + 1];
            }
            else
            {
                inst_off_mod = rm_map[(chan - 6) * 2];
                inst_off_car = rm_map[((chan - 6) * 2) + 1];
            }

            event.isValid = 1;

            if(inst_off_mod != 0xFF)
            {
                uint8_t *ins = m_cmfInstruments[data[0]].data;
                event.data_loc[0] = 0x40 + inst_off_mod;
                event.data_loc[1] = ins[0];
                addEventToBank(evtPos, event);

                event.data_loc[0] = 0x60 + inst_off_mod;
                event.data_loc[1] = ins[2];
                addEventToBank(evtPos, event);

                event.data_loc[0] = 0x80 + inst_off_mod;
                event.data_loc[1] = ins[4];
                addEventToBank(evtPos, event);

                event.data_loc[0] = 0x20 + inst_off_mod;
                event.data_loc[1] = ins[6];
                addEventToBank(evtPos, event);

                event.data_loc[0] = 0xE0 + inst_off_mod;
                event.data_loc[1] = ins[8];
                addEventToBank(evtPos, event);
            }

            if(inst_off_car != 0xFF)
            {
                uint8_t *ins = m_cmfInstruments[data[0]].data;

                reg_43_state[chan] = ins[1];
                event.data_loc[0] = 0x40 + inst_off_car;
                event.data_loc[1] = reg_43_state[chan];
                addEventToBank(evtPos, event);

                event.data_loc[0] = 0x60 + inst_off_car;
                event.data_loc[1] = ins[3];
                addEventToBank(evtPos, event);

                event.data_loc[0] = 0x80 + inst_off_car;
                event.data_loc[1] = ins[5];
                addEventToBank(evtPos, event);

                event.data_loc[0] = 0x20 + inst_off_car;
                event.data_loc[1] = ins[7];
                addEventToBank(evtPos, event);

                event.data_loc[0] = 0xE0 + inst_off_car;
                event.data_loc[1] = ins[9];
                addEventToBank(evtPos, event);
            }

            if(chan <= 6) // Only melodic and bass drum!
            {
                uint8_t *ins = m_cmfInstruments[data[0]].data;
                event.data_loc[0] = 0xC0 + chan;
                event.data_loc[1] = ins[10] | 0x30;
                addEventToBank(evtPos, event);
            }
            break;

        case 0x40: // Note ON without frequency
            event.isValid = 1;

            if(chan < 6)
            {
                reg_b0_state[chan] |= 0x20;
                event.data_loc[0] = 0xB0 + chan;
                event.data_loc[1] = reg_b0_state[chan];
                addEventToBank(evtPos, event);
            }
            else
            {
                switch(chan)
                {
                case 6:
                    reg_bd_state |= 0x10;
                    break;
                case 7:
                    reg_bd_state |= 0x08;
                    break;
                case 8:
                    reg_bd_state |= 0x04;
                    break;
                case 9:
                    reg_bd_state |= 0x02;
                    break;
                case 0x0A:
                    reg_bd_state |= 0x01;
                    break;
                }

                event.data_loc[0] = 0xBD;
                event.data_loc[1] = reg_bd_state;
                addEventToBank(evtPos, event);
            }
            break;

        case 0xF0: // Special event
            switch(cmd)
            {
            case 0xFD: // Delay
                fsize = fr.read(data, 1, 1);
                if(fsize < 1)
                {
                    fr.close();
                    m_cmfInstruments.clear();
                    m_errorString.set("Unexpected file ending on attempt to read KLM song short delay data!");
                    return false;
                }

#ifdef KLM_DEBUG
                printf(" -- DELAY 1 byte (%d)\n", data[0]);
                fflush(stdout);
#endif

                evtPos.delay = data[0];
                evtPos.delay *= 2;
                if(evtPos.delay > 0)
                {
                    evtPos.absPos = abs_position;
                    abs_position += evtPos.delay;
                    m_trackData[0].push_back(evtPos);
                    evtPos.clear();
                }
                break;

            case 0xFE: // Long delay
                fsize = fr.read(data, 1, 2);
                if(fsize < 2)
                {
                    fr.close();
                    m_cmfInstruments.clear();
                    m_errorString.set("Unexpected file ending on attempt to read KLM song short delay data!");
                    return false;
                }

#ifdef KLM_DEBUG
                printf(" -- DELAY 2 bytes (%u)\n", data[0] + (static_cast<uint16_t>(data[1]) << 8));
                fflush(stdout);
#endif

                evtPos.delay = data[0];
                evtPos.delay += static_cast<uint16_t>(data[1]) << 8;
                evtPos.delay *= 2;
                if(evtPos.delay > 0)
                {
                    evtPos.absPos = abs_position;
                    abs_position += evtPos.delay;
                    m_trackData[0].push_back(evtPos);
                    evtPos.clear();
                }
                break;

            case 0xFF: // Song End
                eof_reached = 1;
                if(evtPos.events_begin < evtPos.events_end) // If anything left not written, write!
                {
                    evtPos.absPos = abs_position;
                    abs_position += evtPos.delay;
                    m_trackData[0].push_back(evtPos);
                    evtPos.events_begin = 0;
                    evtPos.events_end = 0;
                    evtPos.clear();
                }
                break;

            default: // Forbidden value!
                fr.close();
                m_cmfInstruments.clear();
                m_errorString.set("Received unsupported special song command value!");
                return false;
            }
            break;

        default: // Forbidden value!
#ifdef KLM_DEBUG
            err_off = fr.tell();
#endif
            fr.close();
            m_cmfInstruments.clear();
            m_errorString.set("Received unsupported normal song command value!");
            return false;
        }
    }

    m_cmfInstruments.clear();

    // Add final row
    evtPos.absPos = abs_position;
    m_trackData[0].push_back(evtPos);
    initTracksBegin(0);

    buildTimeLine(std::vector<TempoEvent>());

    return true;
}

#endif /* BWMIDI_ENABLE_OPL_MUSIC_SUPPORT */

#endif /* BW_MIDISEQ_READ_KLM_IMPL_HPP */
