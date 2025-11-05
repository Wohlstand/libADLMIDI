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
#ifndef BW_MIDISEQ_READ_HMI_IMPL_HPP
#define BW_MIDISEQ_READ_HMI_IMPL_HPP

#include <cstring>

#include "../midi_sequencer.hpp"
#include "common.hpp"

#define HMI_OFFSET_DIVISION         0xD4
#define HMI_OFFSET_TRACKS_COUNT     0xE4
#define HMI_OFFSET_TRACK_DIR        0xE8

#define HMI_OFFSET_TRACK_LEN            0x04
#define HMI_OFFSET_TRACK_DATA_OFFSET    0x57
#define HMI_OFFSET_TRACK_DESIGNATION    0x99

#define HMI_SIZE_TRACK_DIR_HEAD     4

#define HMI_MAX_DESIGNATIONS    8


#define HMP_OFFSET_TRACK_DATA   12


struct HMITrackDir
{
    size_t start;
    size_t len;
    size_t end;
    size_t offset;
    size_t midichan;
    uint16_t designations[8];
};

struct HMPHeader
{
    char magic[32];
    size_t branch_offset; // 4 bytes
    // 12 bytes of padding
    size_t tracksCount; // 4 bytes
    size_t tpqn; // 4 bytes
    size_t division; // 4 bytes
    size_t timeDuration; // 4 bytes
    uint32_t priorities[16]; // 64 bytes
    uint32_t trackDevice[32][5]; // 640 bytes
    uint8_t controlInit[128]; // 128 bytes
    // 8 bytes of the padding
};

static const uint8_t s_hmi_evtSize[16] =
{
//  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
    0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 1, 1, 2, 0
};

static const uint8_t s_hmi_evtSizeCtrl[16] =
{
//  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
    0, 1, 2, 1, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2
};


bool BW_MidiSequencer::parseHMI(FileAndMemReader &fr)
{
    char readBuf[20];
    size_t fsize, track_dir, tracks_offset, file_size,
           totalGotten, abs_position, track_number;
    HMPHeader hmp_head;
    uint64_t (*readHmiVarLen)(FileAndMemReader &fr, const size_t end, bool &ok) = NULL;
    bool isHMP = false, ok = false;
    uint8_t byte;
    int status = 0;
    MidiTrackRow evtPos;
    LoopPointParseState loopState;
    MidiEvent event;

    std::vector<TempoEvent> temposList;
    std::vector<HMITrackDir> dir;

    std::memset(&loopState, 0, sizeof(loopState));
    std::memset(&hmp_head, 0, sizeof(hmp_head));

    file_size = fr.fileSize();

    if(file_size < 0x100)
    {
        m_errorString.set("HMI/HMP: Too small file!\n");
        return false;
    }

    m_format = Format_HMI;
    totalGotten = 0;

    fsize = fr.read(readBuf, 1, sizeof(readBuf));
    if(fsize < sizeof(readBuf))
    {
        m_errorString.set("HMI/HMP: Unexpected end of file at header!\n");
        return false;
    }

    if(std::memcmp(readBuf, "HMI-MIDISONG061595", 19) == 0)
    {
        isHMP = false;
        readHmiVarLen = readVarLenEx; // Same as SMF!

        fr.seek(HMI_OFFSET_DIVISION, FileAndMemReader::SET);
        if(!readUInt16LE(hmp_head.division, fr))
        {
            m_errorString.set("HMI/HMP: Failed to read division value!\n");
            return false;
        }

        hmp_head.division <<= 2;

        fr.seek(HMI_OFFSET_TRACKS_COUNT, FileAndMemReader::SET);
        if(!readUInt16LE(hmp_head.tracksCount, fr))
        {
            m_errorString.set("HMI: Failed to read tracks count!\n");
            return false;
        }

        fr.seek(2, FileAndMemReader::CUR);
        if(!readUInt32LE(track_dir, fr))
        {
            m_errorString.set("HMI: Failed to read track dir pointer!\n");
            return false;
        }

#ifdef DEBUG_HMI_PARSE
        printf("== Division: %lu\n", hmp_head.division);
        fflush(stdout);
#endif

        m_invDeltaTicks = fraction<uint64_t>(1, 1000000l * static_cast<uint64_t>(hmp_head.division));
        m_tempo         = fraction<uint64_t>(1, static_cast<uint64_t>(hmp_head.division));

        dir.resize(hmp_head.tracksCount);
        std::memset(dir.data(), 0, sizeof(HMITrackDir) * hmp_head.tracksCount);

        // Read track sizes
        for(size_t tk = 0; tk < hmp_head.tracksCount; ++tk)
        {
            HMITrackDir &d = dir[tk];

            fr.seek(track_dir + (tk * 4), FileAndMemReader::SET);
            if(!readUInt32LE(d.start, fr))
            {
                m_errorString.set("HMI: Failed to read track start offset!\n");
                return false;
            }

            if(d.start > file_size - HMI_OFFSET_TRACK_DESIGNATION - 4)
            {
                d.len = 0;
                continue; // Track is incomplete
            }

            fr.seek(d.start, FileAndMemReader::SET);
            if(fr.read(readBuf, 1, 13) != 13)
            {
                m_errorString.set("HMI: Failed to read track magic!\n");
                return false;
            }

            if(std::memcmp(readBuf, "HMI-MIDITRACK", 13) != 0)
            {
                d.len = 0;
                continue; // Invalid track magic
            }

            if(tk == hmp_head.tracksCount - 1)
            {
                d.len = file_size - d.start;
                d.end = d.start + d.len;
            }
            else
            {
                fr.seek(track_dir + (tk * HMI_SIZE_TRACK_DIR_HEAD) + HMI_OFFSET_TRACK_LEN, FileAndMemReader::SET);
                if(!readUInt32LE(d.len, fr))
                {
                    m_errorString.set("HMI: Failed to read track start offset!\n");
                    return false;
                }

                if(d.len < d.start)
                {
                    d.len = 0;
                    continue;
                }

                d.len -= d.start;

                if(file_size - d.start < d.len)
                    d.len = file_size - d.start;

                d.end = d.start + d.len;
            }

            fr.seek(d.start + HMI_OFFSET_TRACK_DATA_OFFSET, FileAndMemReader::SET);
            if(!readUInt32LE(d.offset, fr))
            {
                m_errorString.set("HMI: Failed to read MIDI events offset!\n");
                return false;
            }

            if(d.len < d.offset)
            {
                d.len = 0;
                continue;
            }

            d.len -= d.offset;
            totalGotten += d.len;

            fr.seek(d.start + HMI_OFFSET_TRACK_DESIGNATION, FileAndMemReader::SET);
            for(size_t j = 0; j < HMI_MAX_DESIGNATIONS; ++j)
            {
                if(!readUInt16LE(d.designations[j], fr))
                {
                    m_errorString.set("HMI: Failed to read track destignation value!\n");
                    return false;
                }
            }
        }
    }
    else if(std::memcmp(readBuf, "HMIMIDIP", 8) == 0)
    {
        isHMP = true;
        readHmiVarLen = readHMPVarLenEx; // Has different format!

        if(readBuf[8] == 0)
            tracks_offset = 0x308;
        else if(std::memcmp(readBuf + 8, "013195", 6) == 0)
            tracks_offset = 0x388;
        else
        {
            m_errorString.set("HMP: Unknown version of the HMIMIDIP!\n");
            return false;
        }

        // Magic number ends here
        fr.seek(0x20, FileAndMemReader::SET);

        // File length value
        if(!readUInt32LE(hmp_head.branch_offset, fr))
        {
            m_errorString.set("HMP: Failed to read file length field value!\n");
            return false;
        }

        // Skip 12 bytes of padding
        fr.seek(12, FileAndMemReader::CUR);

        // Tracks count value
        if(!readUInt32LE(hmp_head.tracksCount, fr))
        {
            m_errorString.set("HMP: Failed to read tracks count!\n");
            return false;
        }

        if(hmp_head.tracksCount > 32)
        {
            m_errorString.setFmt("HMP: File contains more than 32 tracks (%u)!\n", (unsigned)hmp_head.tracksCount);
            return false;
        }

        if(!readUInt32LE(hmp_head.tpqn, fr))
        {
            m_errorString.set("HMP: Failed to read TPQN value!\n");
            return false;
        }

        // Beats per minute
        if(!readUInt32LE(hmp_head.division, fr))
        {
            m_errorString.set("HMP: Failed to read division value!\n");
            return false;
        }

        if(!readUInt32LE(hmp_head.timeDuration, fr))
        {
            m_errorString.set("HMP: Failed to read time duration value!\n");
            return false;
        }

        for(size_t i = 0; i < 16; ++i)
        {
            if(!readUInt32LE(hmp_head.priorities[i], fr))
            {
                m_errorString.set("HMP: Failed to read one of channel priorities values!\n");
                return false;
            }
        }

        for(size_t i = 0; i < 32; ++i)
        {
            for(size_t j = 0; j < 5; ++j)
            {
                if(!readUInt32LE(hmp_head.trackDevice[i][j], fr))
                {
                    m_errorString.set("HMP: Failed to read one of track device map values!\n");
                    return false;
                }
            }
        }

        if(!fr.read(hmp_head.controlInit, 1, 128))
        {
            m_errorString.set("HMP: Failed to read one of control init bytes!\n");
            return false;
        }



        m_invDeltaTicks = fraction<uint64_t>(1, 1000000l * static_cast<uint64_t>(hmp_head.division));
        m_tempo         = fraction<uint64_t>(1, static_cast<uint64_t>(hmp_head.division));

        dir.resize(hmp_head.tracksCount);
        std::memset(dir.data(), 0, sizeof(HMITrackDir) * hmp_head.tracksCount);

        for(size_t tk = 0; tk < hmp_head.tracksCount; ++tk)
        {
            HMITrackDir &d = dir[tk];

            fr.seek(tracks_offset, FileAndMemReader::SET);
            d.start = tracks_offset;

            if(d.start > file_size - HMP_OFFSET_TRACK_DATA)
            {
                d.len = 0;
                break; // Track is incomplete
            }

            if(!readUInt32LE(track_number, fr))
            {
                m_errorString.set("HMP: Failed to read track number value!\n");
                return false;
            }

            if(track_number != tk)
            {
                m_errorString.set("HMP: Captured track number value is not matching!\n");
                return false;
            }

            if(!readUInt32LE(d.len, fr))
            {
                m_errorString.set("HMP: Failed to read track start offset!\n");
                return false;
            }

            if(!readUInt32LE(d.midichan, fr))
            {
                m_errorString.set("HMP: Failed to read track's MIDI channel value!\n");
                return false;
            }

            tracks_offset += d.len;
            d.end = d.start + d.len;

            if(file_size - d.start < d.len)
                d.len = file_size - d.start;

            if(d.len <= HMP_OFFSET_TRACK_DATA)
            {
                d.len = 0;
                continue;
            }

            d.offset = HMP_OFFSET_TRACK_DATA;
            d.len -= HMP_OFFSET_TRACK_DATA; // Track header size!

            d.designations[0] = 0xA000;
            d.designations[1] = 0xA00A;
            d.designations[2] = 0xA002;
            d.designations[3] = 0;

            totalGotten += d.len;
        }
    }
    else
    {
        m_errorString.set("HMI/HMP: Invalid magic number!\n");
        return false;
    }

    if(totalGotten == 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Empty track data");
        return false;
    }


    buildSmfSetupReset(hmp_head.tracksCount);

    m_loopFormat = Loop_HMI;

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
    if(isHMP)
    {
        event.data_loc[0] = 0x0F;
        event.data_loc[1] = 0x42;
        event.data_loc[2] = 0x40;
    }
    else
    {
        event.data_loc[0] = 0x3D;
        event.data_loc[1] = 0x09;
        event.data_loc[2] = 0x00;
    }
    addEventToBank(evtPos, event);
    TempoEvent tempoEvent = {readBEint(event.data_loc, event.data_loc_size), 0};
    temposList.push_back(tempoEvent);

    evtPos.delay = 0;
    evtPos.absPos = 0;
    m_trackData[0].push_back(evtPos);
    evtPos.clear();

#ifdef DEBUG_HMI_PARSE
    printf("==Tempo %g, Div %g=========================\n", m_tempo.value(), m_invDeltaTicks.value());
    fflush(stdout);
#endif

    size_t tk_v = 0;

    for(size_t tk = 0; tk < hmp_head.tracksCount; ++tk)
    {
        HMITrackDir &d = dir[tk];

        if(d.len == 0)
            continue; // This track is broken

        status = 0;
        fr.seek(d.start + d.offset, FileAndMemReader::SET);

#ifdef DEBUG_HMI_PARSE
        printf("==Track %lu=(de-facto %lu)=============================\n", (unsigned long)tk, (unsigned long)tk_v);
        fflush(stdout);
#endif

        // Time delay that follows the first event in the track
        abs_position = readHmiVarLen(fr, d.end, ok);
        if(!ok)
        {
            m_errorString.set("HMI/HMP: Failed to read first event's delay\n");
            return false;
        }

        do
        {
            std::memset(&event, 0, sizeof(event));
            event.isValid = 1;

            if(fr.tell() + 1 > d.end)
            {
                // When track doesn't ends on the middle of event data, it's must be fine
                event.type = MidiEvent::T_SPECIAL;
                event.subtype = MidiEvent::ST_ENDTRACK;
                status = -1;
            }
            else
            {
                if(fr.read(&byte, 1, 1) != 1)
                {
                    m_errorString.set("HMI/HMP: Failed to read first byte of the event\n");
                    return false;
                }

                if(byte == MidiEvent::T_SYSEX || byte == MidiEvent::T_SYSEX2)
                {
                    uint64_t length = readHmiVarLen(fr, d.end, ok);
                    if(!ok || (fr.tell() + length > d.end))
                    {
                        m_errorString.append("HMI/HMP: Can't read SysEx event - Unexpected end of track data.\n");
                        return false;
                    }

#ifdef DEBUG_HMI_PARSE
                    printf("-- SysEx event\n");
                    fflush(stdout);
#endif

                    event.type = MidiEvent::T_SYSEX;
                    insertDataToBankWithByte(event, m_dataBank, byte, fr, length);
                }
                else if(byte == MidiEvent::T_SPECIAL)
                {
                    // Special event FF
                    uint8_t  evtype;
                    uint64_t length;

                    if(fr.read(&evtype, 1, 1) != 1)
                    {
                        m_errorString.append("HMI/HMP: Failed to read event type!\n");
                        event.isValid = 0;
                        return false;
                    }

#ifdef DEBUG_HMI_PARSE
                    printf("-- Special event 0x%02X\n", evtype);
                    fflush(stdout);
#endif

                    length = (evtype != MidiEvent::ST_ENDTRACK) ? readHmiVarLen(fr, d.end, ok) : 0;

                    if(!ok || (fr.tell() + length > d.end))
                    {
                        m_errorString.append("HMI/HMP: Can't read Special event - Unexpected end of track data.\n");
                        return false;
                    }

                    event.type = byte;
                    event.subtype = evtype;

                    if(event.subtype == MidiEvent::ST_TEMPOCHANGE)
                    {
                        if(length > 5)
                        {
                            m_errorString.append("HMI/HMP: Can't read one of special events - Too long event data (more than 5!).\n");
                            return false;
                        }

                        event.data_loc_size = length;
                        if(fr.read(event.data_loc, 1, length) != length)
                        {
                            m_errorString.append("HMI/HMP: Failed to read event's data (2).\n");
                            return false;
                        }
                    }
                    else if(evtype == MidiEvent::ST_ENDTRACK)
                    {
                        status = -1; // Finalize track
                    }
                    else
                    {
                        m_errorString.append("HMI/HMP: Unsupported meta event sub-type.\n");
                        return false;
                    }
                }
                else if(byte == 0xFE) // Unknown but valid HMI events to skip
                {
                    event.isValid = 0;
                    uint8_t evtype;
                    uint8_t skipSize;

                    if(fr.read(&evtype, 1, 1) != 1)
                    {
                        m_errorString.append("HMI/HMP: Failed to read event type!\n");
                        return false;
                    }
#ifdef DEBUG_HMI_PARSE
                    printf("-- HMI-specific tricky event 0x%02X\n", evtype);
                    fflush(stdout);
#endif

                    switch(evtype)
                    {
                    case 0x13:
                    case 0x15:
                        fr.seek(6, FileAndMemReader::CUR); // Skip 6 bytes
                        break;

                    case 0x12:
                    case 0x14:
                        fr.seek(2, FileAndMemReader::CUR); // Skip 2 bytes
                        break;

                    case 0x10:
                        fr.seek(2, FileAndMemReader::CUR); // Skip 2 bytes

                        if(fr.read(&skipSize, 1, 1) != 1)
                        {
                            m_errorString.append("HMI/HMP: Failed to read unknown event length!\n");
                            return false;
                        }

                        fr.seek(skipSize + 4, FileAndMemReader::CUR); // Skip bytes of gotten length
                        break;

                    default:
                        m_errorString.append("HMI/HMP: Unsupported unknown event type.\n");
                        return false;
                    }
                }
                else if(byte == 0xF1 || byte == 0xF4 || byte == 0xF5 || byte == 0xF6 || (byte >= 0xF8 && byte <= 0xFD))
                {
                    m_errorString.append("HMI/HMP: Totally unsupported event byte!\n");
                    return false;
                }
                else
                {
                    uint8_t midCh, evType;
                    size_t locSize;

#ifdef DEBUG_HMI_PARSE
                    printf("Byte=0x%02X (off=%ld = 0x%lX) : ", byte, fr.tell(), fr.tell());
#endif

                    // Any normal event (80..EF)
                    if(byte < 0x80)
                    {
                        // Extra data for the same type of event and channel
                        byte = static_cast<uint8_t>(status | 0x80);
                        fr.seek(-1, FileAndMemReader::CUR);
                    }

                    status = byte;

                    if(byte >= 0xF0)
                    {
                        locSize = s_hmi_evtSizeCtrl[byte & 0x0F];

                        if(fr.tell() + locSize > d.end)
                        {
                            m_parsingErrorsString.appendFmt("HMI/HMP: Can't read event of type 0x%02X- Unexpected end of track data.\n", byte);
                            return false;
                        }

                        if(locSize > 0)
                            fr.read(event.data_loc, 1, locSize);

                        event.type = byte;
                        event.channel = 0;
                        event.data_loc_size = locSize;
                    }
                    else
                    {
                        midCh = byte & 0x0F;
                        evType = (byte >> 4) & 0x0F;
                        locSize = s_hmi_evtSize[evType];
                        event.channel = midCh;
                        event.type = evType;

                        if(fr.tell() + locSize > d.end)
                        {
                            m_errorString.appendFmt("HMI/HMP: Can't read regular %u-byte event of type 0x%X0- Unexpected end of track data.\n", (unsigned)locSize, (unsigned)evType);
                            return false;
                        }

                        if(locSize > 0)
                            fr.read(event.data_loc, 1, locSize);
                        event.data_loc_size = locSize;

#ifdef DEBUG_HMI_PARSE
                        printf("-- Regular %u-byte event 0x%02X, chan=%u\n", (unsigned)locSize, evType, midCh);
                        fflush(stdout);
#endif

                        if(evType == MidiEvent::T_NOTEON)
                        {
                            if(!isHMP) // Turn into Note-ON with duration
                            {
                                uint64_t duration;
                                duration = readHmiVarLen(fr, d.end, ok);
                                if(!ok)
                                {
                                    m_errorString.append("HMI/HMP: Can't read the duration of timed note.\n");
                                    return false;
                                }

                                event.type = MidiEvent::T_NOTEON_DURATED;
                                if(duration > 0xFFFFFF)
                                    duration = 0xFFFFFF; // Fit to 3 bytes (maximum 16777215 ticks duration)

                                event.data_loc_size = 5;
                                event.data_loc[2] = ((duration << 16) & 0xFF0000);
                                event.data_loc[3] = ((duration << 8) & 0x00FF00);
                                event.data_loc[4] = (duration & 0x0000FF);
                            }
                            else if(event.data_loc[1] == 0) // Note ON with zero velocity is Note OFF!
                                event.type = MidiEvent::T_NOTEOFF;
                        }
                        else if(evType == MidiEvent::T_CTRLCHANGE)
                        {
                            switch(event.data_loc[0])
                            {
                            case 103: // Enable controller restaration when branching and looping
                                event.isValid = 0; // Skip this event for now
                                break;
                            case 104: // Disable controller restaration when branching and looping
                                event.isValid = 0; // Skip this event for now
                                break;

                            case 106: // Lock the channel
                                event.isValid = 0; // Skip this event for now
                                break;
                            case 107: // Set the channel priority
                                event.isValid = 0; // Skip this event for now
                                break;

                            case 108: // Local branch location
                                event.isValid = 0; // Skip this event for now
                                break;
                            case 109: // branch to local branch location
                                event.isValid = 0; // Skip this event for now
                                break;

                            case 110: // Global loop start
                                event.type = MidiEvent::T_SPECIAL;
                                event.subtype = MidiEvent::ST_LOOPSTART;
                                event.data_loc_size = 0;
                                break;
                            case 111: // Global loop end
                                event.type = MidiEvent::T_SPECIAL;
                                event.subtype = MidiEvent::ST_LOOPEND;
                                event.data_loc_size = 0;
                                break;

                            case 113: // Global branch location
                                event.isValid = 0; // Skip this event for now
                                break;
                            case 114: // branch to global branch location
                                event.isValid = 0; // Skip this event for now
                                break;

                            case 116: // Local loop start
                                event.type = MidiEvent::T_SPECIAL;
                                event.subtype = MidiEvent::ST_LOOPSTACK_BEGIN;
                                event.data_loc[0] = event.data_loc[1];
                                event.data_loc_size = 1;
                                break;
                            case 117: // Local loop end
                                event.type = MidiEvent::T_SPECIAL;
                                event.subtype = MidiEvent::ST_LOOPSTACK_END;
                                event.data_loc_size = 0;
                                break;

                            case 119:  // Callback Trigger
                                event.type = MidiEvent::T_SPECIAL;
                                event.subtype = MidiEvent::ST_CALLBACK_TRIGGER;
                                event.data_loc[0] = event.data_loc[1];
                                event.data_loc_size = 1;
                                break;
                            }
                        }
                    }
                }

                if(event.isValid)
                    addEventToBank(evtPos, event);

                if(event.type == MidiEvent::T_SPECIAL)
                {
                    if(event.subtype == MidiEvent::ST_TEMPOCHANGE)
                    {
                        TempoEvent t = {readBEint(event.data_loc, event.data_loc_size), abs_position};
                        temposList.push_back(t);
                    }
                    else
                        analyseLoopEvent(loopState, event, abs_position);
                }

                if(event.type != MidiEvent::T_SPECIAL || event.subtype != MidiEvent::ST_ENDTRACK)
                {
                    evtPos.delay = readHmiVarLen(fr, d.end, ok);
                    if(!ok)
                    {
                        /* End of track has been reached! However, there is no EOT event presented */
                        event.type = MidiEvent::T_SPECIAL;
                        event.subtype = MidiEvent::ST_ENDTRACK;
                        event.isValid = 1;
                    }
                }

#ifdef ENABLE_END_SILENCE_SKIPPING
                //Have track end on its own row? Clear any delay on the row before
                if(event.type == MidiEvent::T_SPECIAL && event.subtype == MidiEvent::ST_ENDTRACK && (evtPos.events_end - evtPos.events_begin) == 1)
                {
                    if(!m_trackData[tk_v].empty())
                    {
                        MidiTrackRow &previous = m_trackData[tk_v].back();
                        previous.delay = 0;
                        previous.timeDelay = 0;
                    }
                }
#endif

                if((evtPos.delay > 0) || (event.subtype == MidiEvent::ST_ENDTRACK))
                {
#ifdef DEBUG_HMI_PARSE
                    printf("- Delay %lu, Position %lu, stored events: %lu, time: %g seconds\n",
                            (unsigned long)evtPos.delay, abs_position, evtPos.events_end - evtPos.events_begin,
                            (abs_position * m_tempo).value());
                    fflush(stdout);
#endif

                    evtPos.absPos = abs_position;
                    abs_position += evtPos.delay;
                    m_trackData[tk_v].push_back(evtPos);
                    evtPos.clear();
                    loopState.gotLoopEventsInThisRow = false;
                }

                if(status < 0 && evtPos.events_begin != evtPos.events_end) // Last row in the track
                {
                    evtPos.delay = 0;
                    evtPos.absPos = abs_position;
                    m_trackData[tk_v].push_back(evtPos);
                    evtPos.clear();
                    loopState.gotLoopEventsInThisRow = false;
                }
            }
        } while((fr.tell() <= d.end) && (event.subtype != MidiEvent::ST_ENDTRACK));

#ifdef DEBUG_HMI_PARSE
        printf("==Track %lu==END=========================\n", (unsigned long)tk_v);
        fflush(stdout);
#endif

        if(loopState.ticksSongLength < abs_position)
            loopState.ticksSongLength = abs_position;

        // Set the chain of events begin
        initTracksBegin(tk_v);

        ++tk_v;
    }

    // Shrink tracks store if real number of tracks is smaller
    if(m_tracksCount != tk_v)
        buildSmfResizeTracks(tk_v);

    installLoop(loopState);
    buildTimeLine(temposList, loopState.loopStartTicks, loopState.loopEndTicks);

    return true;
}

#endif /* BW_MIDISEQ_READ_HMI_IMPL_HPP */
