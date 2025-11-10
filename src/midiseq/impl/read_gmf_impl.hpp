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
#ifndef BW_MIDISEQ_READ_GMF_IMPL_HPP
#define BW_MIDISEQ_READ_GMF_IMPL_HPP

#include <cstring>

#include "../midi_sequencer.hpp"

bool BW_MidiSequencer::parseGMF(FileAndMemReader &fr)
{
    const size_t headerSize = 14;
    char headerBuf[headerSize] = "";
    size_t fsize = 0, deltaTicks = 192, totalGotten, trackLength, trackPos;
    LoopPointParseState loopState;

    std::vector<TempoEvent> temposList;

    std::memset(&loopState, 0, sizeof(loopState));

    m_smfFormat = 0;

    fsize = fr.read(headerBuf, 1, headerSize);
    if(fsize < headerSize)
    {
        m_errorString.set("Unexpected end of file at header!\n");
        return false;
    }

    if(std::memcmp(headerBuf, "GMF\x1", 4) != 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Invalid format, GMF\\x1 signature is not found!\n");
        return false;
    }

    fr.seek(7 - static_cast<long>(headerSize), FileAndMemReader::CUR);

    m_invDeltaTicks.nom = 1;
    m_invDeltaTicks.denom = 1000000l * deltaTicks;
    m_tempo.nom = 1;
    m_tempo.denom = deltaTicks * 2;

    // static const unsigned char EndTag[4] = {0xFF, 0x2F, 0x00, 0x00};
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

#endif /* BW_MIDISEQ_READ_GMF_IMPL_HPP */
