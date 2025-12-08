/*
 * libADLMIDI is a free Software MIDI synthesizer library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2025 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Library is based on the ADLMIDI, a MIDI player for Linux and Windows with OPL3 emulation:
 * http://iki.fi/bisqwit/source/adlmidi.html
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

inline uint16_t toUint16BE(const uint8_t *arr)
{
    uint16_t num = arr[1];
    num |= ((arr[0] << 8) & 0xFF00);
    return num;
}

inline int16_t toSint16BE(const uint8_t *arr)
{
    int16_t num = *reinterpret_cast<const int8_t *>(&arr[0]);
    num *= 1 << 8;
    num |= arr[1];
    return num;
}

inline int16_t toSint16LE(const uint8_t *arr)
{
    int16_t num = *reinterpret_cast<const int8_t *>(&arr[1]);
    num *= 1 << 8;
    num |= static_cast<int16_t>(arr[0]) & 0x00FF;
    return num;
}

inline uint16_t toUint16LE(const uint8_t *arr)
{
    uint16_t num = arr[0];
    num |= ((arr[1] << 8) & 0xFF00);
    return num;
}

inline uint32_t toUint32LE(const uint8_t *arr)
{
    uint32_t num = arr[0];
    num |= (static_cast<uint32_t>(arr[1] << 8)  & 0x0000FF00);
    num |= (static_cast<uint32_t>(arr[2] << 16) & 0x00FF0000);
    num |= (static_cast<uint32_t>(arr[3] << 24) & 0xFF000000);
    return num;
}

inline int32_t toSint32LE(const uint8_t *arr)
{
    int32_t num = *reinterpret_cast<const int8_t *>(&arr[3]);
    num *= 1 << 24;
    num |= (static_cast<int32_t>(arr[2]) << 16) & 0x00FF0000;
    num |= (static_cast<int32_t>(arr[1]) << 8) & 0x0000FF00;
    num |= static_cast<int32_t>(arr[0]) & 0x000000FF;
    return num;
}

#endif // COMMON_H
