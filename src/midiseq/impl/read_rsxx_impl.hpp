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
#ifndef BW_MIDISEQ_READ_RSXX_IMPL_HPP
#define BW_MIDISEQ_READ_RSXX_IMPL_HPP

#ifdef BWMIDI_ENABLE_OPL_MUSIC_SUPPORT

#include <cstring>
#include "../midi_sequencer.hpp"


bool BW_MidiSequencer::parseRSXX(FileAndMemReader &fr)
{
    const size_t headerSize = 14;
    char headerBuf[headerSize] = "";
    size_t fsize = 0, deltaTicks = 192;
    size_t trackLength, trackPos, totalGotten;
    LoopPointParseState loopState;
    char start;

    std::vector<TempoEvent> temposList;

    std::memset(&loopState, 0, sizeof(loopState));


    fsize = fr.read(headerBuf, 1, headerSize);
    if(fsize < headerSize)
    {
        m_errorString.set("Unexpected end of file at header!\n");
        return false;
    }

    // Try to identify RSXX format
    start = headerBuf[0];
    if(start < 0x5D)
    {
        m_errorString.set("RSXX song too short!\n");
        return false;
    }
    else
    {
        fr.seek(start - 0x10, FileAndMemReader::SET);
        fr.read(headerBuf, 1, 6);
        if(std::memcmp(headerBuf, "rsxx}u", 6) == 0)
        {
            m_format = Format_RSXX;
            fr.seek(start, FileAndMemReader::SET);
            deltaTicks = 60;
        }
        else
        {
            m_errorString.set("Invalid RSXX header!\n");
            return false;
        }
    }

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

    //Finalize raw track data with a zero
    // rawTrackData[0].push_back(0);

    if(totalGotten == 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Empty track data");
        return false;
    }

    m_smfFormat = 0;
    m_loop.stackLevel   = -1;

    buildSmfSetupReset(1);

    // Build new MIDI events table
    if(!smf_buildOneTrack(fr, 0, trackLength, temposList, loopState))
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": MIDI data parsing error has occouped!\n");
        m_errorString.append(m_parsingErrorsString.c_str());
        return false;
    }

    initTracksBegin(0);
    installLoop(loopState);
    buildTimeLine(temposList, loopState.loopStartTicks, loopState.loopEndTicks);

    return true;
}

#endif /* BWMIDI_ENABLE_OPL_MUSIC_SUPPORT */

#endif /* BW_MIDISEQ_READ_RSXX_IMPL_HPP */
