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
#ifndef BW_MIDISEQ_READ_XMI_IMPL_HPP
#define BW_MIDISEQ_READ_XMI_IMPL_HPP

#include <cstring>

#include "../midi_sequencer.hpp"

#ifndef BWMIDI_DISABLE_XMI_SUPPORT
#include "cvt_xmi2mid.hpp"


bool BW_MidiSequencer::parseXMI(FileAndMemReader &fr)
{
    const size_t headerSize = 14;
    char headerBuf[headerSize] = "";
    size_t fsize = 0;
//    BufferGuard<uint8_t> cvt_buf;
    std::vector<std::vector<uint8_t > > song_buf;
    bool ret;

    (void)Convert_xmi2midi; /* Shut up the warning */

    fsize = fr.read(headerBuf, 1, headerSize);
    if(fsize < headerSize)
    {
        m_errorString.set("Unexpected end of file at header!\n");
        return false;
    }

    if(std::memcmp(headerBuf, "FORM", 4) != 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Invalid format, FORM signature is not found!\n");
        return false;
    }

    if(std::memcmp(headerBuf + 8, "XDIR", 4) != 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Invalid format\n");
        return false;
    }

    size_t mus_len = fr.fileSize();
    fr.seek(0, FileAndMemReader::SET);

    uint8_t *mus = (uint8_t*)std::malloc(mus_len + 20);
    if(!mus)
    {
        m_errorString.set("Out of memory!");
        return false;
    }

    std::memset(mus, 0, mus_len + 20);

    fsize = fr.read(mus, 1, mus_len);
    if(fsize < mus_len)
    {
        m_errorString.set("Failed to read XMI file data!\n");
        return false;
    }

    // Close source stream
    fr.close();

//    uint8_t *mid = NULL;
//    uint32_t mid_len = 0;
    int m2mret = Convert_xmi2midi_multi(mus, static_cast<uint32_t>(mus_len + 20),
                                        song_buf, XMIDI_CONVERT_NOCONVERSION);
    if(mus)
        free(mus);

    if(m2mret < 0)
    {
        song_buf.clear();
        m_errorString.set("Invalid XMI data format!");
        return false;
    }

    if(m_loadTrackNumber >= (int)song_buf.size())
        m_loadTrackNumber = song_buf.size() - 1;

    for(size_t i = 0; i < song_buf.size(); ++i)
    {
        m_rawSongsData.push_back(song_buf[i]);
    }

    song_buf.clear();

    // cvt_buf.set(mid);
    // Open converted MIDI file
    fr.openData(m_rawSongsData[m_loadTrackNumber].data(),
                m_rawSongsData[m_loadTrackNumber].size());
    // Set format as XMIDI
    m_format = Format_XMIDI;

    ret = parseSMF(fr);

    return ret;
}
#endif /* BWMIDI_DISABLE_XMI_SUPPORT */

#endif /* BW_MIDISEQ_READ_KLM_IMPL_HPP */
