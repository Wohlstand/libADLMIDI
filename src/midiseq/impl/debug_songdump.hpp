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
#ifndef BW_MIDISEQ_DEBUGDUMP_HPP
#define BW_MIDISEQ_DEBUGDUMP_HPP

#ifdef BWMIDI_ENABLE_DEBUG_SONG_DUMP
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <cmath>

#include "../midi_sequencer.hpp"


const char *evtName(BW_MidiSequencer::MidiEvent::Types type, BW_MidiSequencer::MidiEvent::SubTypes subType)
{
    switch(type)
    {
    default:
    case BW_MidiSequencer::MidiEvent::T_UNKNOWN:
        return "-Unknown-";
    case BW_MidiSequencer::MidiEvent::T_NOTEOFF:
        return "NoteOff";
    case BW_MidiSequencer::MidiEvent::T_NOTEON:
        return "NoteON";
    case BW_MidiSequencer::MidiEvent::T_NOTEON_DURATED:
        return "NoteOnDurated";
    case BW_MidiSequencer::MidiEvent::T_NOTETOUCH:
        return "NoteAfterTouch";
    case BW_MidiSequencer::MidiEvent::T_CTRLCHANGE:
        return "Controller";
    case BW_MidiSequencer::MidiEvent::T_PATCHCHANGE:
        return "Patch";
    case BW_MidiSequencer::MidiEvent::T_CHANAFTTOUCH:
        return "ChannelAfterTouch";
    case BW_MidiSequencer::MidiEvent::T_WHEEL:
        return "PitchWheel";
    case BW_MidiSequencer::MidiEvent::T_SYSEX:
        return "SysEx";
    case BW_MidiSequencer::MidiEvent::T_SYSCOMSNGSEL:
        return "SysComSongSel";
    case BW_MidiSequencer::MidiEvent::T_SYSCOMSPOSPTR:
        return "SysComSongPosPtr";
    case BW_MidiSequencer::MidiEvent::T_SYSEX2:
        return "SysEx2";
    case BW_MidiSequencer::MidiEvent::T_SPECIAL:
        switch(subType)
        {
        default:
            return "FF-Unknown";
        case BW_MidiSequencer::MidiEvent::ST_SEQNUMBER:
            return "FF-SeqNumber";
        case BW_MidiSequencer::MidiEvent::ST_TEXT:
            return "FF-Text";
        case BW_MidiSequencer::MidiEvent::ST_COPYRIGHT:
            return "FF-Copyright";
        case BW_MidiSequencer::MidiEvent::ST_SQTRKTITLE:
            return "FF-TrackTitle";
        case BW_MidiSequencer::MidiEvent::ST_INSTRTITLE:
            return "FF-InstrName";
        case BW_MidiSequencer::MidiEvent::ST_LYRICS:
            return "FF-Lyrics";
        case BW_MidiSequencer::MidiEvent::ST_MARKER:
            return "FF-Marker";
        case BW_MidiSequencer::MidiEvent::ST_CUEPOINT:
            return "FF-CuePoint";
        case BW_MidiSequencer::MidiEvent::ST_DEVICESWITCH:
            return "FF-DevSwitch";
        case BW_MidiSequencer::MidiEvent::ST_MIDICHPREFIX:
            return "FF-PortPrefix";
        case BW_MidiSequencer::MidiEvent::ST_ENDTRACK:
            return "FF-END-TRACK";
        case BW_MidiSequencer::MidiEvent::ST_TEMPOCHANGE:
            return "FF-Tempo";
        case BW_MidiSequencer::MidiEvent::ST_SMPTEOFFSET:
            return "FF-SMPTEOffset";
        case BW_MidiSequencer::MidiEvent::ST_KEYSIGNATURE:
            return "FF-KeySign";
        case BW_MidiSequencer::MidiEvent::ST_TIMESIGNATURE:
            return "FF-TimeSign";
        case BW_MidiSequencer::MidiEvent::ST_SEQUENCERSPEC:
            return "FF-SequencerSpec";

        case BW_MidiSequencer::MidiEvent::ST_SONG_BEGIN_HOOK:
            return "XX-SongBegin";

        case BW_MidiSequencer::MidiEvent::ST_LOOPSTART:
            return "XX-GlobalLoopStart";
        case BW_MidiSequencer::MidiEvent::ST_LOOPEND:
            return "XX-GlobalLoopEnd";

        case BW_MidiSequencer::MidiEvent::ST_LOOPSTACK_BEGIN:
            return "XX-LoopStack-Begin";
        case BW_MidiSequencer::MidiEvent::ST_LOOPSTACK_BEGIN_ID:
            return "XX-LoopStack-Begin-ID";
        case BW_MidiSequencer::MidiEvent::ST_LOOPSTACK_END:
            return "XX-LoopStack-End";
        case BW_MidiSequencer::MidiEvent::ST_LOOPSTACK_END_ID:
            return "XX-LoopStack-End-ID";
        case BW_MidiSequencer::MidiEvent::ST_LOOPSTACK_BREAK:
            return "XX-LoopStack-Break";

        case BW_MidiSequencer::MidiEvent::ST_CALLBACK_TRIGGER:
            return "XX-CallbackTrigger";

        case BW_MidiSequencer::MidiEvent::ST_TRACK_LOOPSTACK_BEGIN:
            return "XX-Track-LoopStack-Begin";
        case BW_MidiSequencer::MidiEvent::ST_TRACK_LOOPSTACK_BEGIN_ID:
            return "XX-Track-LoopStack-Begin-ID";
        case BW_MidiSequencer::MidiEvent::ST_TRACK_LOOPSTACK_END:
            return "XX-Track-LoopStack-End";
        case BW_MidiSequencer::MidiEvent::ST_TRACK_LOOPSTACK_END_ID:
            return "XX-Track-LoopStack-End-ID";
        case BW_MidiSequencer::MidiEvent::ST_TRACK_LOOPSTACK_BREAK:
            return "XX-Track-LoopStack-Break";

        case BW_MidiSequencer::MidiEvent::ST_BRANCH_LOCATION:
            return "XX-BranchLoc";
        case BW_MidiSequencer::MidiEvent::ST_BRANCH_TO:
            return "XX-BranchTo";

        case BW_MidiSequencer::MidiEvent::ST_TRACK_BRANCH_LOCATION:
            return "XT-Track-BranchLoc";
        case BW_MidiSequencer::MidiEvent::ST_TRACK_BRANCH_TO:
            return "XT-Track-BranchTo";

        case BW_MidiSequencer::MidiEvent::ST_CHANNEL_LOCK:
            return "XX-ChanLock";
        case BW_MidiSequencer::MidiEvent::ST_CHANNEL_UNLOCK:
            return "XX-ChanUnlock";

        case BW_MidiSequencer::MidiEvent::ST_ENABLE_RESTORE_CC_ON_LOOP:
            return "XX-EN-OnLoop-RestoreCC";
        case BW_MidiSequencer::MidiEvent::ST_ENABLE_NOTEOFF_ON_LOOP:
            return "XX-EN-OnLoop-NoteOffs";
        case BW_MidiSequencer::MidiEvent::ST_ENABLE_RESTORE_PATCH_ON_LOOP:
            return "XX-EN-OnLoop-Patch";
        case BW_MidiSequencer::MidiEvent::ST_ENABLE_RESTORE_WHEEL_ON_LOOP:
            return "XX-EN-OnLoop-PitchBend";
        case BW_MidiSequencer::MidiEvent::ST_ENABLE_RESTORE_NOTEAFTERTOUCH_ON_LOOP:
            return "XX-EN-OnLoop-NoteAfterTouch";
        case BW_MidiSequencer::MidiEvent::ST_ENABLE_RESTORE_CHANAFTERTOUCH_ON_LOOP:
            return "XX-EN-OnLoop-ChanAfterTouch";
        case BW_MidiSequencer::MidiEvent::ST_ENABLE_RESTORE_ALL_CC_ON_LOOP:
            return "XX-EN-OnLoop-RestoreAllCC";

        case BW_MidiSequencer::MidiEvent::ST_DISABLE_RESTORE_CC_ON_LOOP:
            return "XX-DS-OnLoop-RestoreCC";
        case BW_MidiSequencer::MidiEvent::ST_DISABLE_NOTEOFF_ON_LOOP:
            return "XX-DS-OnLoop-NoteOffs";
        case BW_MidiSequencer::MidiEvent::ST_DISABLE_RESTORE_PATCH_ON_LOOP:
            return "XX-DS-OnLoop-Patch";
        case BW_MidiSequencer::MidiEvent::ST_DISABLE_RESTORE_WHEEL_ON_LOOP:
            return "XX-DS-OnLoop-PitchBend";
        case BW_MidiSequencer::MidiEvent::ST_DISABLE_RESTORE_NOTEAFTERTOUCH_ON_LOOP:
            return "XX-DS-OnLoop-NoteAfterTouch";
        case BW_MidiSequencer::MidiEvent::ST_DISABLE_RESTORE_CHANAFTERTOUCH_ON_LOOP:
            return "XX-DS-OnLoop-ChanAfterTouch";
        case BW_MidiSequencer::MidiEvent::ST_DISABLE_RESTORE_ALL_CC_ON_LOOP:
            return "XX-DS-OnLoop-RestoreAllCC";

        case BW_MidiSequencer::MidiEvent::ST_CHANNEL_PRIORITY:
            return "XX-ChanPriority";
        }
    }
}

static void str2time(double time, char *timeBuff, size_t timeBuffSize)
{
    double sec;
    unsigned min, hours;

    sec = std::fmod(time, 60);
    min = (unsigned)std::fmod(std::floor(time / 60.0), 60.0);
    hours = (unsigned)std::floor(time / 3600);

    if(hours > 0)
        snprintf(timeBuff, timeBuffSize, "%02u:%02u:%06.3f", hours, min, sec);
    else
        snprintf(timeBuff, timeBuffSize, "   %02u:%06.3f", min, sec);
}

bool BW_MidiSequencer::debugDumpContents(const std::string &outFile)
{
    bool ret;
    FILE *out;

    if(m_tracksCount == 0)
    {
        m_errorString.setFmt("Song is not loaded!");
        return false;
    }

    out = fopen(outFile.c_str(), "wb");
    if(!out)
    {
        m_errorString.setFmt("Can't open file %s for write: ", outFile.c_str());
#ifndef _WIN32
        m_errorString.appendFmt("%s\n", std::strerror(errno));
#endif
        return false;
    }

    ret = debugDumpContents(out);

    fclose(out);

    return ret;
}

bool BW_MidiSequencer::debugDumpContents(FILE *out)
{
    char delayBuff[100];
    char timeBuff[100];

    if(m_tracksCount == 0)
    {
        m_errorString.setFmt("Song is not loaded!");
        return false;
    }

    if(!out)
    {
        m_errorString.setFmt("Invalid input stream!");
#ifndef _WIN32
        m_errorString.appendFmt("%s\n", std::strerror(errno));
#endif
        return false;
    }

    str2time(m_fullSongTimeLength, timeBuff, 100);

    fprintf(out, "- Total tracks: %u\r\n", (unsigned)m_tracksCount);
    fprintf(out, "- Full duration of song: %s\r\n", timeBuff);
    fprintf(out, "\r\n");

    for(size_t tk = 0; tk < m_tracksCount; ++tk)
    {
        MidiTrackState &trackState = m_trackState[tk];

        fprintf(out, "=======================Track %lu=======================\r\n", (unsigned long)tk);
        fprintf(out, "Device Mask: 0x%04X\r\n", (unsigned)trackState.deviceMask);
        fprintf(out, "\r\n");

        MidiTrackQueue::iterator it = m_trackBeginPosition.track[tk].pos;

        while(it != m_trackData[tk].end())
        {
            MidiTrackRow &row = *it;

            str2time(row.timeDelay, delayBuff, 100);
            str2time(row.time, timeBuff, 100);

            fprintf(out, "  Delay=%8lu (%10.6g sec., %s), Time: %6lu [%s] %10.3f sec.)\r\n",
                         (unsigned long)row.delay, row.timeDelay, delayBuff,
                         (unsigned long)row.absPos, timeBuff, row.time);

            for(size_t i = row.events_begin; i < row.events_end; ++i)
            {
                MidiEvent &e = m_eventBank[i];

                fprintf(out, "-CH=%02u [%02X] %s -- ",
                        (unsigned)e.channel,
                        (unsigned)e.type,
                        evtName((BW_MidiSequencer::MidiEvent::Types)e.type,
                                (BW_MidiSequencer::MidiEvent::SubTypes)e.subtype)
                        );

                if(e.data_loc_size)
                {
                    fprintf(out, "; loc[%u]: ", (unsigned)e.data_loc_size);
                    for(size_t j = 0; j < e.data_loc_size; ++j)
                        fprintf(out, " %02X", e.data_loc[j]);
                }

                if(e.data_block.size > 0)
                {
                    fprintf(out, "; block[%u]: ", (unsigned)e.data_block.size);
                    for(size_t j = e.data_block.offset; j < e.data_block.offset + e.data_block.size; ++j)
                        fprintf(out, " %02X", m_dataBank[j]);
                }

                fprintf(out, "\r\n");
            }

            fflush(out);
            ++it;
        }

        fprintf(out, "=======================Track %lu=END===================\r\n\r\n\r\n", (unsigned long)tk);
        fflush(out);
    }

    return true;
}

#endif /* BWMIDI_ENABLE_DEBUG_SONG_DUMP */

#endif /* BW_MIDISEQ_DEBUGDUMP_HPP */
