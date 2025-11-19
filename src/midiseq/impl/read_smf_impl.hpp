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
#ifndef BW_MIDISEQ_READ_SMF_IMPL_HPP
#define BW_MIDISEQ_READ_SMF_IMPL_HPP

#include <cstdlib>
#include <cstring>

#include "../midi_sequencer.hpp"
#include "common.hpp"


static const uint8_t s_midi_evtSize[16] =
{
//  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
    0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 1, 1, 2, 0
};

static const uint8_t s_midi_evtSizeCtrls[16] =
{
//  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
    0, 1, 2, 1, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2
};


bool BW_MidiSequencer::smf_buildTracks(FileAndMemReader &fr, const size_t tracks_offset, const size_t tracks_count)
{
    uint8_t headBuf[8];
    size_t fsize = 0;
    size_t trackLength;
    size_t offset_next;
    //! Tempo change events list
    std::vector<TempoEvent> temposList;
    LoopPointParseState loopState;

    std::memset(&loopState, 0, sizeof(loopState));

    buildSmfSetupReset(tracks_count);

    offset_next = tracks_offset;

    for(size_t tk = 0; tk < tracks_count; ++tk)
    {
        // Read current track from here
        fr.seek(offset_next, FileAndMemReader::SET);

        fsize = fr.read(headBuf, 1, 8);
        if((fsize < 8) || (std::memcmp(headBuf, "MTrk", 4) != 0))
        {
            m_parsingErrorsString.set(fr.fileName().c_str());
            m_parsingErrorsString.append(": Invalid format, MTrk signature is not found!\n");
            return false;
        }

        trackLength = (size_t)readBEint(headBuf + 4, 4);
        offset_next += trackLength + 8; // Track length plus header size

        if(!smf_buildOneTrack(fr, tk, trackLength, temposList, loopState))
            return false; // Failed to parse track (error already written!)
    }

    installLoop(loopState);
    buildTimeLine(temposList, loopState.loopStartTicks, loopState.loopEndTicks);

    return true;
}

bool BW_MidiSequencer::smf_buildOneTrack(FileAndMemReader &fr,
                                     const size_t track_idx,
                                     const size_t track_size,
                                     std::vector<TempoEvent> &temposList,
                                     LoopPointParseState &loopState)
{
    MidiTrackRow evtPos;
    MidiEvent event;
    uint64_t abs_position = 0;
    int status = 0;
    const size_t end = fr.tell() + track_size;
    bool ok = false, trackChannelNeeded = false, trackChannelHas = false;

    //! Caches note on/off states.
    bool noteStates[0x7FF]; // [ccc|cnnnnnnn] - c = channel, n = note
    /* This is required to carefully detect zero-length notes           *
     * and avoid a move of "note-off" event over "note-on" while sort.  *
     * Otherwise, after sort those notes will play infinite sound       */

    std::memset(noteStates, 0, sizeof(noteStates));

    // Time delay that follows the first event in the track
    if(m_format == Format_RSXX)
        ok = true;
    else
        evtPos.delay = readVarLenEx(fr, end, ok);

    if(!ok)
    {
        m_parsingErrorsString.appendFmt("buildTrackData: Can't read variable-length value at begin of track %lu.\n", (unsigned long)track_idx);
        return false;
    }

    // HACK: Begin every track with "Reset all controllers" event to avoid controllers state break came from end of song
    if(track_idx == 0)
    {
        MidiEvent resetEvent;
        std::memset(&resetEvent, 0, sizeof(resetEvent));
        resetEvent.isValid = 1;
        resetEvent.type = MidiEvent::T_SPECIAL;
        resetEvent.subtype = MidiEvent::ST_SONG_BEGIN_HOOK;
        addEventToBank(evtPos, resetEvent);
    }

    evtPos.absPos = abs_position;
    abs_position += evtPos.delay;
    m_trackData[track_idx].push_back(evtPos);
    evtPos.clear();

    m_trackState[track_idx].state.track_channel = 0xFF;

    if((m_format == Format_MIDI && m_smfFormat == 1 && track_idx > 0) || m_format == Format_HMI)
        trackChannelNeeded = true;

    do
    {
        event = smf_parseEvent(fr, end, status);
        if(!event.isValid)
        {
            m_parsingErrorsString.appendFmt("buildTrackData: Fail to parse event in the track %lu.\n", (unsigned long)track_idx);
            return false;
        }

        addEventToBank(evtPos, event);

        if(trackChannelNeeded && !trackChannelHas && event.type > 0x00 && event.type < 0xF0)
        {
            m_trackState[track_idx].state.track_channel = event.channel;
            trackChannelHas = true;
        }

        if(event.type == MidiEvent::T_SPECIAL)
        {
            if(event.subtype == MidiEvent::ST_TEMPOCHANGE)
            {
                TempoEvent t = {readBEint(event.data_loc, event.data_loc_size), abs_position};
                temposList.push_back(t);
            }
            else
                analyseLoopEvent(loopState, event, abs_position, &m_trackState[track_idx].loop);
        }

        // Don't try to read delta after EndOfTrack event!
        if(event.type != MidiEvent::T_SPECIAL || event.subtype != MidiEvent::ST_ENDTRACK)
        {
            evtPos.delay = readVarLenEx(fr, end, ok);
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
            if (!m_trackData[track_idx].empty())
            {
                MidiTrackRow &previous = m_trackData[track_idx].back();
                previous.delay = 0;
                previous.timeDelay = 0;
            }
        }
#endif

        if((evtPos.delay > 0) || loopState.gotLoopEventsInThisRow > 0 || (event.subtype == MidiEvent::ST_ENDTRACK))
        {
            evtPos.sortEvents(m_eventBank, noteStates);
            smf_flushRow(evtPos, abs_position, track_idx, loopState);
        }
    }
    while((fr.tell() <= end) && (event.subtype != MidiEvent::ST_ENDTRACK));

    if(loopState.ticksSongLength < abs_position)
        loopState.ticksSongLength = abs_position;

    // Set the chain of events begin
    initTracksBegin(track_idx);

    return true;
}


BW_MidiSequencer::MidiEvent BW_MidiSequencer::smf_parseEvent(FileAndMemReader &fr, const size_t end, int &status)
{
    uint8_t byte, midCh, evType;
    size_t locSize;
    bool ok = false;
    BW_MidiSequencer::MidiEvent evt;

    std::memset(&evt, 0, sizeof(evt));
    evt.isValid = 1;

    if(fr.tell() + 1 > end)
    {
        // When track doesn't ends on the middle of event data, it's must be fine
        evt.type = MidiEvent::T_SPECIAL;
        evt.subtype = MidiEvent::ST_ENDTRACK;
        return evt;
    }


    if(fr.read(&byte, 1, 1) != 1)
    {
        m_parsingErrorsString.append("parseEvent: Failed to read first byte of the event\n");
        evt.isValid = 0;
        return evt;
    }

    if(byte == MidiEvent::T_SYSEX || byte == MidiEvent::T_SYSEX2) // Parse SysEx
    {
        uint64_t length = readVarLenEx(fr, end, ok);
        if(!ok || (fr.tell() + length > end))
        {
            m_parsingErrorsString.append("parseEvent: Can't read SysEx event - Unexpected end of track data.\n");
            evt.isValid = 0;
            return evt;
        }

        evt.type = MidiEvent::T_SYSEX;
        insertDataToBankWithByte(evt, m_dataBank, byte, fr, length);
        return evt;
    }

    if(byte == MidiEvent::T_SPECIAL)
    {
        // Special event FF
        uint8_t  evtype;
        uint64_t length;
        const char *entry;

        if(fr.read(&evtype, 1, 1) != 1)
        {
            m_parsingErrorsString.append("parseEvent: Failed to read event type!\n");
            evt.isValid = 0;
            return evt;
        }

        length = readVarLenEx(fr, end, ok);

        if(!ok || (fr.tell() + length > end))
        {
            m_parsingErrorsString.append("parseEvent: Can't read Special event - Unexpected end of track data.\n");
            evt.isValid = 0;
            return evt;
        }

        evt.type = byte;
        evt.subtype = evtype;

        switch(evt.subtype)
        {
        case MidiEvent::ST_SEQNUMBER:
        case MidiEvent::ST_MIDICHPREFIX:
        case MidiEvent::ST_TEMPOCHANGE:
        case MidiEvent::ST_SMPTEOFFSET:
        case MidiEvent::ST_TIMESIGNATURE:
        case MidiEvent::ST_KEYSIGNATURE:
            if(length > 5)
            {
                m_parsingErrorsString.append("parseEvent: Can't read one of special events - Too long event data (more than 5!).\n");
                evt.isValid = 0;
                return evt;
            }

            evt.data_loc_size = length;
            if(fr.read(evt.data_loc, 1, length) != length)
            {
                m_parsingErrorsString.append("parseEvent: Failed to read event's data (1).\n");
                evt.isValid = 0;
                return evt;
            }

#if 0 /* Print all tempo events */
            if(evt.subtype == MidiEvent::ST_TEMPOCHANGE && hooks.onDebugMessage)
                hooks.onDebugMessage(hooks.onDebugMessage_userData, "Temp Change: %02X%02X%02X", evt.data_loc[0], evt.data_loc[1], evt.data_loc[2]);
#endif
            break;
        case MidiEvent::ST_COPYRIGHT:
            insertDataToBankWithTerm(evt, m_dataBank, fr, length);
            entry = reinterpret_cast<const char*>(getData(evt.data_block));

            if(m_musCopyright.size == 0)
            {
                m_musCopyright = evt.data_block;

                if(m_interface->onDebugMessage)
                    m_interface->onDebugMessage(m_interface->onDebugMessage_userData, "Music copyright: %s", entry);
            }
            else if(m_interface->onDebugMessage)
                m_interface->onDebugMessage(m_interface->onDebugMessage_userData, "Extra copyright event: %s", entry);
            break;

        case MidiEvent::ST_SQTRKTITLE:
            insertDataToBankWithTerm(evt, m_dataBank, fr, length);
            entry = reinterpret_cast<const char*>(getData(evt.data_block));

            if(m_musTitle.size == 0)
            {
                m_musTitle = evt.data_block;
                if(m_interface->onDebugMessage)
                    m_interface->onDebugMessage(m_interface->onDebugMessage_userData, "Music title: %s", entry);
            }
            else
            {
                m_musTrackTitles.push_back(evt.data_block);

                if(m_interface->onDebugMessage)
                    m_interface->onDebugMessage(m_interface->onDebugMessage_userData, "Track title: %s", entry);
            }
            break;

        case MidiEvent::ST_INSTRTITLE:
            insertDataToBankWithTerm(evt, m_dataBank, fr, length);
            entry = reinterpret_cast<const char*>(getData(evt.data_block));

            if(m_interface->onDebugMessage)
                m_interface->onDebugMessage(m_interface->onDebugMessage_userData, "Instrument: %s", entry);
            break;

        case MidiEvent::ST_MARKER:
            insertDataToBankWithTerm(evt, m_dataBank, fr, length);
            entry = reinterpret_cast<const char*>(getData(evt.data_block));

            if(strEqual(entry, length, "loopstart"))
            {
                // Return a custom Loop Start event instead of Marker
                evt.subtype = MidiEvent::ST_LOOPSTART;
                return evt;
            }
            else if(strEqual(entry, length, "loopend"))
            {
                // Return a custom Loop End event instead of Marker
                evt.subtype = MidiEvent::ST_LOOPEND;
                return evt;
            }
            else if(length > 10 && strEqual(entry, 10, "loopstart="))
            {
                char loop_key[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                size_t key_len = length - 10;
                std::memcpy(loop_key, entry + 10, key_len > 10 ? 10 : key_len);

                evt.type = MidiEvent::T_SPECIAL;
                evt.subtype = MidiEvent::ST_LOOPSTACK_BEGIN;
                evt.data_loc_size = 1;
                evt.data_loc[0] = static_cast<uint8_t>(std::atoi(loop_key));

                if(m_interface->onDebugMessage)
                {
                    m_interface->onDebugMessage(
                        m_interface->onDebugMessage_userData,
                        "Stack Marker Loop Start at %d to %d level with %d loops",
                        m_loop.stackLevel,
                        m_loop.stackLevel + 1,
                        evt.data_loc[0]
                    );
                }

                return evt;
            }
            else if(length > 8 && strEqual(entry, 8, "loopend="))
            {
                evt.type = MidiEvent::T_SPECIAL;
                evt.subtype = MidiEvent::ST_LOOPSTACK_END;
                evt.data_loc_size = 0;

                if(m_interface->onDebugMessage)
                {
                    m_interface->onDebugMessage(
                        m_interface->onDebugMessage_userData,
                        "Stack Marker Loop %s at %d to %d level",
                        (evt.subtype == MidiEvent::ST_LOOPSTACK_END ? "End" : "Break"),
                        m_loop.stackLevel,
                        m_loop.stackLevel - 1
                    );
                }
                return evt;
            }
            break;

        case MidiEvent::ST_ENDTRACK:
            status = -1; // Finalize track
            break;

        default: // Unknown special event
            insertDataToBank(evt, m_dataBank, fr, length);
            break;
        }

        return evt;
    }

    // Any normal event (80..EF)
    if(byte < 0x80)
    {
        // Extra data for the same type of event and channel
        byte = static_cast<uint8_t>(status | 0x80);
        fr.seek(-1, FileAndMemReader::CUR);
    }

    status = byte;

    // RSXX-specific song end event
    if(m_format == Format_RSXX && byte == 0xFC)
    {
        if(fr.tell() + 1 > end)
        {
            m_parsingErrorsString.appendFmt("parseEvent: Can't read RSXX specific event of type 0x%02X- Unexpected end of track data.\n", byte);
            evt.isValid = 0;
            return evt;
        }

        fr.read(evt.data_loc, 1, 1);

        evt.type = MidiEvent::T_SPECIAL;
        if(evt.data_loc[0] == 0x80)
            evt.subtype = MidiEvent::ST_LOOPEND;
        else
        {
            evt.subtype = MidiEvent::ST_ENDTRACK;
            status = -1;
        }

        evt.channel = 0;
        evt.data_loc_size = 0;
        return evt;
    }

    // Sys Com Song Select(Song #) [0-127]
    // Sys Com Song Position Pntr [LSB, MSB]
    if(byte >= 0xF0)
    {
        locSize = s_midi_evtSizeCtrls[byte & 0x0F];

        if(fr.tell() + locSize > end)
        {
            m_parsingErrorsString.appendFmt("parseEvent: Can't read event of type 0x%02X- Unexpected end of track data.\n", byte);
            evt.isValid = 0;
            return evt;
        }

        if(locSize > 0)
            fr.read(evt.data_loc, 1, locSize);

        evt.type = byte;
        evt.channel = 0;
        evt.data_loc_size = locSize;
        return evt;
    }

    midCh = byte & 0x0F, evType = (byte >> 4) & 0x0F;
    evt.channel = midCh;
    evt.type = evType;
    locSize = s_midi_evtSize[evType];

    if(fr.tell() + locSize > end)
    {
        m_parsingErrorsString.appendFmt("parseEvent: Can't read regular %u-byte event - Unexpected end of track data.\n", (unsigned)locSize);
        evt.isValid = 0;
        return evt;
    }

    if(locSize > 0)
        fr.read(evt.data_loc, 1, locSize);

    evt.data_loc_size = locSize;

    // Handle some special cases of events
    switch(evType)
    {
    case MidiEvent::T_NOTEON:
        if(evt.data_loc[1] == 0)
            evt.type = MidiEvent::T_NOTEOFF; // Note ON with zero velocity is Note OFF!
        break;

    case MidiEvent::T_CTRLCHANGE:
        switch(m_format)
        {
        case Format_MIDI:
            switch(evt.data_loc[0])
            {
            case 110:
                if(m_loopFormat == Loop_Default)
                {
                    // Change event type to custom Loop Start event and clear data
                    evt.type = MidiEvent::T_SPECIAL;
                    evt.subtype = MidiEvent::ST_LOOPSTART;
                    m_loopFormat = Loop_HMI;
                }
                else if(m_loopFormat == Loop_HMI)
                {
                    // Repeating of 110'th point is BAD practice, treat as EMIDI
                    m_loopFormat = Loop_EMIDI;
                }
                break;

            case 111:
                if(m_loopFormat == Loop_HMI)
                {
                    // Change event type to custom Loop End event and clear data
                    evt.type = MidiEvent::T_SPECIAL;
                    evt.subtype = MidiEvent::ST_LOOPEND;
                }
                else if(m_loopFormat != Loop_EMIDI)
                {
                    // Change event type to custom Loop Start event and clear data
                    evt.type = MidiEvent::T_SPECIAL;
                    evt.subtype = MidiEvent::ST_LOOPSTART;
                }
                break;

            case 113:
                if(m_loopFormat == Loop_EMIDI)
                {
                    // EMIDI does using of CC113 with same purpose as CC7
                    evt.data_loc[0] = 7;
                }
                break;
#if 0 //WIP
            case 116:
                if(m_loopFormat == Loop_EMIDI)
                {
                    evt.type = MidiEvent::T_SPECIAL;
                    evt.subtype = MidiEvent::ST_LOOPSTACK_BEGIN;
                    evt.data_loc[0] = evt.data_loc[1];

                    if(m_interface->onDebugMessage)
                    {
                        m_interface->onDebugMessage(
                            m_interface->onDebugMessage_userData,
                            "Stack EMIDI Loop Start at %d to %d level with %d loops",
                            m_loop.stackLevel,
                            m_loop.stackLevel + 1,
                            evt.data_loc[0]
                        );
                    }
                }
                break;

            case 117:  // Next/Break Loop Controller
                if(m_loopFormat == Loop_EMIDI)
                {
                    evt.type = MidiEvent::T_SPECIAL;
                    evt.subtype = MidiEvent::ST_LOOPSTACK_END;

                    if(m_interface->onDebugMessage)
                    {
                        m_interface->onDebugMessage(
                            m_interface->onDebugMessage_userData,
                            "Stack EMIDI Loop End at %d to %d level",
                            m_loop.stackLevel,
                            m_loop.stackLevel - 1
                        );
                    }
                }
                break;
#endif
            }
            break;

        case Format_XMIDI:
            switch(evt.data_loc[0])
            {
            case 116:  // For Loop Controller
                evt.type = MidiEvent::T_SPECIAL;
                evt.subtype = MidiEvent::ST_LOOPSTACK_BEGIN;
                evt.data_loc[0] = evt.data_loc[1];
                evt.data_loc_size = 1;

                if(m_interface->onDebugMessage)
                {
                    m_interface->onDebugMessage(
                        m_interface->onDebugMessage_userData,
                        "Stack XMI Loop Start at %d to %d level with %d loops",
                        m_loop.stackLevel,
                        m_loop.stackLevel + 1,
                        evt.data_loc[0]
                    );
                }
                break;

            case 117:  // Next/Break Loop Controller
                evt.type = MidiEvent::T_SPECIAL;
                evt.subtype = evt.data_loc[1] < 64 ?
                            MidiEvent::ST_LOOPSTACK_BREAK :
                            MidiEvent::ST_LOOPSTACK_END;
                evt.data_loc_size = 0;

                if(m_interface->onDebugMessage)
                {
                    m_interface->onDebugMessage(
                        m_interface->onDebugMessage_userData,
                        "Stack XMI Loop %s at %d to %d level",
                        (evt.subtype == MidiEvent::ST_LOOPSTACK_END ? "End" : "Break"),
                        m_loop.stackLevel,
                        m_loop.stackLevel - 1
                    );
                }
                break;

            case 119:  // Callback Trigger
                evt.type = MidiEvent::T_SPECIAL;
                evt.subtype = MidiEvent::ST_CALLBACK_TRIGGER;
                evt.data_loc[0] = evt.data_loc[1];
                evt.data_loc_size = 1;
                break;
            }
            break;

        case Format_CMF:
            switch(evt.data_loc[0])
            {
            case 102: // Song markers (0x66)
                evt.type = MidiEvent::T_SPECIAL;
                evt.subtype = MidiEvent::ST_CALLBACK_TRIGGER;
                evt.data_loc[0] = evt.data_loc[1];
                evt.data_loc_size = 1;
                break;

            case 104: // Transpose Up (0x68), convert into pitch bend
            {
                int16_t bend = 8192 + (((int)evt.data_loc[1] * 8192) / 128);
                evt.type = MidiEvent::T_WHEEL;
                evt.data_loc[0] = (bend & 0x7F);
                evt.data_loc[1] = ((bend >> 7) & 0x7F);
                evt.data_loc_size = 2;
                break;
            }

            case 105: // Transpose Down (0x69), convert into pitch bend
            {
                int16_t bend = 8192 - (((int)evt.data_loc[1] * 8192) / 128);
                evt.type = MidiEvent::T_WHEEL;
                evt.data_loc[0] = (bend & 0x7F);
                evt.data_loc[1] = ((bend >> 7) & 0x7F);
                evt.data_loc_size = 2;
                break;
            }
            }
            break;

        default:
            break;
        }

        break;

    default:
        break;
    }

    return evt;
}

void BW_MidiSequencer::smf_flushRow(MidiTrackRow &evtPos, uint64_t &abs_position, size_t track_num, LoopPointParseState &loopState, bool finish)
{
    evtPos.absPos = abs_position;

    if(finish)
        evtPos.delay = 0;
    else
        abs_position += evtPos.delay;

    m_trackData[track_num].push_back(evtPos);
    evtPos.clear();
    loopState.gotLoopEventsInThisRow = 0;
}

bool BW_MidiSequencer::parseSMF(FileAndMemReader &fr)
{
    const size_t headerSize = 14; // 4 + 4 + 2 + 2 + 2
    char headerBuf[headerSize] = "";
    size_t fsize = 0;
    size_t deltaTicks = 192, trackCount = 1;
    unsigned smfFormat = 0;

    fsize = fr.read(headerBuf, 1, headerSize);
    if(fsize < headerSize)
    {
        m_errorString.set("Unexpected end of file at header!\n");
        return false;
    }

    if(std::memcmp(headerBuf, "MThd\0\0\0\6", 8) != 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Invalid format, MThd signature is not found!\n");
        return false;
    }

    smfFormat  = static_cast<unsigned>(readBEint(headerBuf + 8,  2));
    trackCount = static_cast<size_t>(readBEint(headerBuf + 10, 2));
    deltaTicks = static_cast<size_t>(readBEint(headerBuf + 12, 2));

    if(smfFormat > 2)
        smfFormat = 1;

    m_invDeltaTicks.nom = 1;
    m_invDeltaTicks.denom = 1000000l * deltaTicks;
    m_tempo.nom = 1;
    m_tempo.denom = deltaTicks * 2;
    m_smfFormat = smfFormat;

    size_t totalGotten = 0;
    size_t tracks_begin = fr.tell();

    // Read track sizes
    for(size_t tk = 0; tk < trackCount; ++tk)
    {
        // Read track header
        size_t trackLength, prev_pos;

        fsize = fr.read(headerBuf, 1, 8);
        if((fsize < 8) || (std::memcmp(headerBuf, "MTrk", 4) != 0))
        {
            m_errorString.set(fr.fileName().c_str());
            m_errorString.append(": Invalid format, MTrk signature is not found!\n");
            return false;
        }

        trackLength = (size_t)readBEint(headerBuf + 4, 4);
        prev_pos = fr.tell();

        fr.seek(trackLength, FileAndMemReader::CUR);

        if(fr.tell() - trackLength != prev_pos)
        {
            m_errorString.set(fr.fileName().c_str());
            m_errorString.append(": Unexpected file ending while getting raw track data!\n");
            return false;
        }

        totalGotten += trackLength;
    }

    if(totalGotten == 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Empty track data");
        return false;
    }

    // Build new MIDI events table
    if(!smf_buildTracks(fr, tracks_begin, trackCount))
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": MIDI data parsing error has occouped!\n");
        m_errorString.append(m_parsingErrorsString.c_str());
        return false;
    }

    m_loop.stackLevel   = -1;

    return true;
}

bool BW_MidiSequencer::parseRMI(FileAndMemReader &fr)
{
    const size_t headerSize = 4 + 4 + 2 + 2 + 2; // 14
    char headerBuf[headerSize] = "";

    size_t fsize = fr.read(headerBuf, 1, headerSize);
    if(fsize < headerSize)
    {
        m_errorString.set("Unexpected end of file at header!\n");
        return false;
    }

    if(std::memcmp(headerBuf, "RIFF", 4) != 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Invalid format, RIFF signature is not found!\n");
        return false;
    }

    m_format = Format_MIDI;

    fr.seek(6l, FileAndMemReader::CUR);
    return parseSMF(fr);
}

#endif /* BW_MIDISEQ_READ_SMF_IMPL_HPP */
