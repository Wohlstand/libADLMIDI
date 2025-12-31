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
#ifndef BW_MIDISEQ_COMMON_HPP
#define BW_MIDISEQ_COMMON_HPP

#include <cstddef>
#include <stdint.h>

#include "../file_reader.hpp"

#if !defined(__SIZEOF_POINTER__) // Workaround for MSVC
#   if defined(_WIN32)
#       if defined(_WIN64)
#           define __SIZEOF_POINTER__ 8
#       else
#           define __SIZEOF_POINTER__ 4
#       endif
#   else
#       warning UNKNOWN SIZE OF POINTER! The value 4 is set as a workaround
#       define __SIZEOF_POINTER__ 4
#   endif
#endif

/**
 * @brief Utility function to read Big-Endian integer from raw binary data
 * @param buffer Pointer to raw binary buffer
 * @param nbytes Count of bytes to parse integer
 * @return Extracted unsigned integer
 */
static uint64_t readBEint(const void *buffer, size_t nbytes)
{
    uint64_t result = 0;
    const uint8_t *data = reinterpret_cast<const uint8_t *>(buffer);

    for(size_t n = 0; n < nbytes; ++n)
    {
        result <<= 8;
        result |= data[n];
    }

    return result;
}

/**
 * @brief Utility function to read Little-Endian integer from raw binary data
 * @param buffer Pointer to raw binary buffer
 * @param nbytes Count of bytes to parse integer
 * @return Extracted unsigned integer
 */
static uint64_t readLEint(const void *buffer, size_t nbytes)
{
    uint64_t result = 0;
    const uint8_t *data = reinterpret_cast<const uint8_t *>(buffer);

    for(size_t n = 0; n < nbytes; ++n)
        result += static_cast<uint64_t>(data[n] << (n * 8));

    return result;
}

static uint16_t readLEint16(const void *buffer, size_t nbytes)
{
    uint16_t result = 0;
    const uint8_t *data = reinterpret_cast<const uint8_t *>(buffer);

    for(size_t n = 0; n < nbytes; ++n)
        result += static_cast<uint16_t>(data[n] << (n * 8));

    return result;
}

static uint32_t readLEint32(const void *buffer, size_t nbytes)
{
    uint32_t result = 0;
    const uint8_t *data = reinterpret_cast<const uint8_t *>(buffer);

    for(size_t n = 0; n < nbytes; ++n)
        result += static_cast<uint32_t>(data[n] << (n * 8));

    return result;
}

static bool readUInt32LE_SZ(size_t &out, FileAndMemReader &fr)
{
    uint8_t buf[4];

    if(fr.read(buf, 1, 4) != 4)
        return false;

    out = readLEint32(buf, 4);

    return true;
}

static bool readUInt32LE(uint32_t &out, FileAndMemReader &fr)
{
    uint8_t buf[4];

    if(fr.read(buf, 1, 4) != 4)
        return false;

    out = readLEint32(buf, 4);

    return true;
}

static bool readUInt16LE(size_t &out, FileAndMemReader &fr)
{
    uint8_t buf[2];

    if(fr.read(buf, 1, 2) != 2)
        return false;

    out = readLEint16(buf, 2);

    return true;
}

static bool readUInt16LE(uint16_t &out, FileAndMemReader &fr)
{
    uint8_t buf[2];

    if(fr.read(buf, 1, 2) != 2)
        return false;

    out = readLEint16(buf, 2);

    return true;
}


#if 0
/**
 * @brief Secure Standard MIDI Variable-Length numeric value parser with anti-out-of-range protection
 * @param [_inout] ptr Pointer to memory block that contains begin of variable-length value, will be iterated forward
 * @param [_in end Pointer to end of memory block where variable-length value is stored (after end of track)
 * @param [_out] ok Reference to boolean which takes result of variable-length value parsing
 * @return Unsigned integer that conains parsed variable-length value
 */
static uint64_t readVarLenEx(const uint8_t **ptr, const uint8_t *end, bool &ok)
{
    uint64_t result = 0;
    ok = false;

    for(;;)
    {
        if(*ptr >= end)
            return 2;
        unsigned char byte = *((*ptr)++);
        result = (result << 7) + (byte & 0x7F);
        if(!(byte & 0x80))
            break;
    }

    ok = true;
    return result;
}
#endif

static uint64_t readVarLenEx(FileAndMemReader &fr, const size_t end, bool &ok)
{
    uint64_t result = 0;
    uint8_t byte;

    ok = false;

    for(;;)
    {
        if(fr.tell() >= end || fr.read(&byte, 1, 1) != 1)
            return 2;

        result = (result << 7) + (byte & 0x7F);

        if((byte & 0x80) == 0)
            break;
    }

    ok = true;

    return result;
}

static uint64_t readHMPVarLenEx(FileAndMemReader &fr, const size_t end, bool &ok)
{
    uint64_t result = 0;
    uint8_t byte;
    int offset = 0;

    ok = false;

    for(;;)
    {
        if(fr.tell() >= end || fr.read(&byte, 1, 1) != 1)
            return 2;

        result |= (byte & 0x7F) << offset;
        offset += 7;

        if((byte & 0x80) != 0)
            break;
    }

    ok = true;

    return result;
}

static bool strEqual(const char *in_str, size_t length, const char *needle)
{
    const char *it_i = in_str, *it_n = needle;
    size_t i = 0;

    for( ; i < length && *it_n != 0; ++i, ++it_i, ++it_n)
    {
        if(*it_i <= 'Z' && *it_i >= 'A')
        {
            if(*it_i - ('Z' - 'z') != *it_n)
                return false; // Mismatch!
        }
        else if(*it_i != *it_n)
            return false; // Mismatch!
    }

    return i == length && *it_n == 0; // Length is same
}

#endif /* BW_MIDISEQ_COMMON_HPP */
