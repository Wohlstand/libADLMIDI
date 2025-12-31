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
#ifndef BW_MIDISEQ_LOAD_MUSIC_IMPL_HPP
#define BW_MIDISEQ_LOAD_MUSIC_IMPL_HPP

#include <cstring>
#include <string>
#include <assert.h>
#ifndef _WIN32
#   include <errno.h>
#endif

#include "../midi_sequencer.hpp"
#include "common.hpp"



#ifdef BWMIDI_ENABLE_OPL_MUSIC_SUPPORT
/**
 * @brief Detect the EA-MUS file format
 * @param head Header part
 * @param fr Context with opened file data
 * @return true if given file was identified as EA-MUS
 */
static bool detectRSXX(const char *head, FileAndMemReader &fr)
{
    char headerBuf[7] = "";
    bool ret = false;

    // Try to identify RSXX format
    if(head[0] >= 0x5D && fr.fileSize() > reinterpret_cast<const uint8_t*>(head)[0])
    {
        fr.seek(head[0] - 0x10, FileAndMemReader::SET);
        fr.read(headerBuf, 1, 6);
        if(std::memcmp(headerBuf, "rsxx}u", 6) == 0)
            ret = true;
    }

    fr.seek(0, FileAndMemReader::SET);
    return ret;
}

/**
 * @brief Detect the Id-software Music File format
 * @param head Header part
 * @param fr Context with opened file data
 * @return true if given file was identified as IMF
 */
static bool detectIMF(const char *head, FileAndMemReader &fr)
{
    uint8_t raw[4];
    size_t end = static_cast<size_t>(head[0]) + 256 * static_cast<size_t>(head[1]);

    if(end & 3)
        return false;

    size_t backup_pos = fr.tell();
    int64_t sum1 = 0, sum2 = 0;
    fr.seek((end > 0 ? 2 : 0), FileAndMemReader::SET);

    for(size_t n = 0; n < 16383; ++n)
    {
        if(fr.read(raw, 1, 4) != 4)
            break;
        int64_t value1 = raw[0];
        value1 += raw[1] << 8;
        sum1 += value1;
        int64_t value2 = raw[2];
        value2 += raw[3] << 8;
        sum2 += value2;
    }

    fr.seek(static_cast<long>(backup_pos), FileAndMemReader::SET);

    return (sum1 > sum2);
}

static bool detectKLM(const char *head, FileAndMemReader &fr)
{
    uint16_t song_off = static_cast<uint64_t>(readLEint(head + 3, 2));

    if(head[2] != 0x01)
        return false;

    if(song_off > fr.fileSize())
        return false;

    if((song_off - 5) % 11 != 0) // Invalid offset!
        return false;

    return true;
}
#endif


bool BW_MidiSequencer::loadMIDI(const std::string &filename)
{
    FileAndMemReader file;
    file.openFile(filename.c_str());

    if(!loadMIDI(file))
        return false;

    return true;
}

bool BW_MidiSequencer::loadMIDI(const void *data, size_t size)
{
    FileAndMemReader file;
    file.openData(data, size);
    return loadMIDI(file);
}



// template<class T>
// class BufferGuard
// {
//     T *m_ptr;
// public:
//     BufferGuard() : m_ptr(NULL)
//     {}

//     ~BufferGuard()
//     {
//         set();
//     }

//     void set(T *p = NULL)
//     {
//         if(m_ptr)
//             free(m_ptr);
//         m_ptr = p;
//     }
// };


bool BW_MidiSequencer::loadMIDI(FileAndMemReader &fr)
{
    size_t  fsize = 0;
    BW_MidiSequencer_UNUSED(fsize);
    m_parsingErrorsString.clear();

    assert(m_interface); // MIDI output interface must be defined!

    if(!fr.isValid())
    {
        m_errorString.set("Invalid data stream!\n");
#ifndef _WIN32
        m_errorString.append(std::strerror(errno));
#endif
        return false;
    }

    m_atEnd            = false;
    m_loop.fullReset();
    m_loop.caughtStart = true;

    m_deviceMaskAvailable = Device_ANY;

    m_format = Format_MIDI;
    m_smfFormat = 0;

    m_cmfInstruments.clear();
    m_rawSongsData.clear();

    const size_t headerSize = 4 + 4 + 2 + 2 + 2; // 14
    char headerBuf[headerSize] = "";

    fsize = fr.read(headerBuf, 1, headerSize);
    if(fsize < headerSize)
    {
        m_errorString.set("Unexpected end of file at header!\n");
        return false;
    }


    if(std::memcmp(headerBuf, "MThd\0\0\0\6", 8) == 0)
    {
        fr.seek(0, FileAndMemReader::SET);
        return parseSMF(fr);
    }

    if(std::memcmp(headerBuf, "RIFF", 4) == 0)
    {
        fr.seek(0, FileAndMemReader::SET);
        return parseRMI(fr);
    }

    if(std::memcmp(headerBuf, "GMF\x1", 4) == 0)
    {
        fr.seek(0, FileAndMemReader::SET);
        return parseGMF(fr);
    }

    if(std::memcmp(headerBuf, "MUS\x1A", 4) == 0)
    {
        fr.seek(0, FileAndMemReader::SET);
        return parseMUS(fr);
    }

    if(std::memcmp(headerBuf, "HMI-MIDISONG06", 14) == 0 || std::memcmp(headerBuf, "HMIMIDIP", 8) == 0)
    {
        fr.seek(0, FileAndMemReader::SET);
        return parseHMI(fr);
    }

#ifndef BWMIDI_DISABLE_XMI_SUPPORT
    if((std::memcmp(headerBuf, "FORM", 4) == 0) && (std::memcmp(headerBuf + 8, "XDIR", 4) == 0))
    {
        fr.seek(0, FileAndMemReader::SET);
        return parseXMI(fr);
    }
#endif

#ifdef BWMIDI_ENABLE_OPL_MUSIC_SUPPORT
    if(std::memcmp(headerBuf, "CTMF", 4) == 0)
    {
        fr.seek(0, FileAndMemReader::SET);
        return parseCMF(fr);
    }

    if(detectRSXX(headerBuf, fr))
    {
        fr.seek(0, FileAndMemReader::SET);
        return parseRSXX(fr);
    }

    if(detectKLM(headerBuf, fr))
    {
        fr.seek(0, FileAndMemReader::SET);
        return parseKLM(fr);
    }

    // This file type should be parsed last!
    if(detectIMF(headerBuf, fr))
    {
        fr.seek(0, FileAndMemReader::SET);
        return parseIMF(fr);
    }
#endif

    m_errorString.set("Unknown or unsupported file format");
    return false;
}


#endif /* BW_MIDISEQ_LOAD_MUSIC_IMPL_HPP */
