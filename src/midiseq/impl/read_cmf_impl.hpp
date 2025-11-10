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
#ifndef BW_MIDISEQ_READ_CMF_IMPL_HPP
#define BW_MIDISEQ_READ_CMF_IMPL_HPP

#ifdef BWMIDI_ENABLE_OPL_MUSIC_SUPPORT

#include <cstring>

#include "../midi_sequencer.hpp"
#include "common.hpp"

#define CMF_OFFSET_VER_MAJOR    4
#define CMF_OFFSET_VER_MINOR    5
#define CMF_OFFSET_INS_START    6
#define CMF_OFFSET_MUS_START    8
#define CMF_OFFSET_TICKS_PER_Q  10
#define CMF_OFFSET_TICKS_PER_S  12

bool BW_MidiSequencer::parseCMF(FileAndMemReader &fr)
{
    const size_t headerSize = 14;
    char headerBuf[headerSize] = "";
    size_t fsize = 0, ver_maj, ver_min, deltaTicks = 192, totalGotten = 0,
           trackPos, trackLength, tempoNew;
    size_t ins_start, mus_start, ticks, ins_count;
    LoopPointParseState loopState;

    std::vector<TempoEvent> temposList;

    std::memset(&loopState, 0, sizeof(loopState));


    fsize = fr.read(headerBuf, 1, headerSize);
    if(fsize < headerSize)
    {
        m_errorString.set("Unexpected end of file at header!\n");
        return false;
    }

    if(std::memcmp(headerBuf, "CTMF", 4) != 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Invalid format, CTMF signature is not found!\n");
        return false;
    }

    m_format = Format_CMF;
    m_smfFormat = 0;

    ver_maj = headerBuf[CMF_OFFSET_VER_MAJOR];
    ver_min = headerBuf[CMF_OFFSET_VER_MINOR];

    if(ver_maj != 0x01 || (ver_min != 0x01 && ver_min != 0x00))
    {
        m_errorString.setFmt("%s: Unsupported version of CMF: %u.%u\n", fr.fileName().c_str(), (unsigned)ver_maj, (unsigned)ver_min);
        return false;
    }

    ins_start = readLEint(headerBuf + CMF_OFFSET_INS_START, 2);
    mus_start = readLEint(headerBuf + CMF_OFFSET_MUS_START, 2);
    //unsigned deltas    = readLEint(HeaderBuf + CMF_OFFSET_TICKS_PER_Q, 2);
    ticks     = readLEint(headerBuf + CMF_OFFSET_TICKS_PER_S, 2);

    // Read title, author, remarks start offsets in file
    fsize = fr.read(headerBuf, 1, 6);
    if(fsize < 6)
    {
        fr.close();
        m_errorString.set("Unexpected file ending on attempt to read CTMF header!");
        return false;
    }

    //uint64_t notes_starts[3] = {readLEint(headerBuf + 0, 2), readLEint(headerBuf + 0, 4), readLEint(headerBuf + 0, 6)};
    fr.seek(16, FileAndMemReader::CUR); // Skip the channels-in-use table

    if(ver_min == 0x00)
    {
        if(fr.read(headerBuf, 1, 1) != 1)
        {
            fr.close();
            m_errorString.set("Unexpected file ending on attempt to read CMF instruments block header!");
            return false;
        }

        ins_count = headerBuf[0];
    }
    else
    {
        if(!readUInt16LE(ins_count, fr))
        {
            fr.close();
            m_errorString.set("Unexpected file ending on attempt to read CMF instruments block header!");
            return false;
        }

        if(!readUInt16LE(tempoNew, fr))
        {
            fr.close();
            m_errorString.set("Unexpected file ending on attempt to read CMF tempo value!");
            return false;
        }
    }

    fr.seek(static_cast<long>(ins_start), FileAndMemReader::SET);

    m_cmfInstruments.reserve(static_cast<size_t>(ins_count));
    for(uint64_t i = 0; i < ins_count; ++i)
    {
        CmfInstrument inst;
        fsize = fr.read(inst.data, 1, 16);
        if(fsize < 16)
        {
            fr.close();
            m_errorString.set("Unexpected file ending on attempt to read CMF instruments raw data!");
            return false;
        }
        m_cmfInstruments.push_back(inst);
    }

    fr.seeku(mus_start, FileAndMemReader::SET);
    deltaTicks = (size_t)ticks;

    m_invDeltaTicks.nom = 1;
    m_invDeltaTicks.denom = 1000000l * deltaTicks;
    m_tempo.nom = 1;
    m_tempo.denom = deltaTicks;

    totalGotten = 0;

    // Read track header
    trackPos = fr.tell();
    fr.seek(0, FileAndMemReader::END);
    trackLength = fr.tell() - trackPos;
    fr.seek(static_cast<long>(trackPos), FileAndMemReader::SET);
    totalGotten += trackLength;

    if(totalGotten == 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Empty track data");
        return false;
    }

    buildSmfSetupReset(1);

    // Build new MIDI events table
    if(!smf_buildOneTrack(fr, 0, trackLength, temposList, loopState))
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": MIDI data parsing error has occouped!\n");
        m_errorString.append(m_parsingErrorsString.c_str());
        return false;
    }

    installLoop(loopState);
    buildTimeLine(temposList, loopState.loopStartTicks, loopState.loopEndTicks);

    return true;
}
#endif /* BWMIDI_ENABLE_OPL_MUSIC_SUPPORT */

#endif /* BW_MIDISEQ_READ_CMF_IMPL_HPP */
