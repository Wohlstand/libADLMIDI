/*
 * ADLMIDI Player is a free MIDI player based on a libADLMIDI,
 * a Software MIDI synthesizer library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2026 Vitaly Novichkov <admin@wohlnet.ru>
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

#include <stdint.h>
#include "playback.h"
#include "../dev_setup.h"


int stop = 0;

#if !defined(ADLMIDI_ENABLE_HW_DOS)
const char* audio_format_to_str(int format, int is_msb)
{
    switch(format)
    {
    case ADLMIDI_SampleType_S8:
        return "S8";
    case ADLMIDI_SampleType_U8:
        return "U8";
    case ADLMIDI_SampleType_S16:
        return is_msb ? "S16MSB" : "S16";
    case ADLMIDI_SampleType_U16:
        return is_msb ? "U16MSB" : "U16";
    case ADLMIDI_SampleType_S32:
        return is_msb ? "S32MSB" : "S32";
    case ADLMIDI_SampleType_F32:
        return is_msb ? "F32MSB" : "F32";
    }
    return "UNK";
}


void fillAudioFormat(const AudioOutputSpec &spec)
{
    switch(spec.format)
    {
    case ADLMIDI_SampleType_S8:
        g_audioFormat.type = ADLMIDI_SampleType_S8;
        g_audioFormat.containerSize = sizeof(int8_t);
        g_audioFormat.sampleOffset = sizeof(int8_t) * 2;
        break;
    case ADLMIDI_SampleType_U8:
        g_audioFormat.type = ADLMIDI_SampleType_U8;
        g_audioFormat.containerSize = sizeof(uint8_t);
        g_audioFormat.sampleOffset = sizeof(uint8_t) * 2;
        break;
    case ADLMIDI_SampleType_S16:
        g_audioFormat.type = ADLMIDI_SampleType_S16;
        g_audioFormat.containerSize = sizeof(int16_t);
        g_audioFormat.sampleOffset = sizeof(int16_t) * 2;
        break;
    case ADLMIDI_SampleType_U16:
        g_audioFormat.type = ADLMIDI_SampleType_U16;
        g_audioFormat.containerSize = sizeof(uint16_t);
        g_audioFormat.sampleOffset = sizeof(uint16_t) * 2;
        break;
    case ADLMIDI_SampleType_S32:
        g_audioFormat.type = ADLMIDI_SampleType_S32;
        g_audioFormat.containerSize = sizeof(int32_t);
        g_audioFormat.sampleOffset = sizeof(int32_t) * 2;
        break;
    case ADLMIDI_SampleType_F32:
        g_audioFormat.type = ADLMIDI_SampleType_F32;
        g_audioFormat.containerSize = sizeof(float);
        g_audioFormat.sampleOffset = sizeof(float) * 2;
        break;
    }
}


void applyGain(uint8_t *buffer, size_t bufferSize)
{
    size_t i;

    switch(g_audioFormat.type)
    {
    case ADLMIDI_SampleType_S8:
    {
        int8_t *buf = reinterpret_cast<int8_t *>(buffer);
        size_t samples = bufferSize;
        for(i = 0; i < samples; ++i)
            *(buf++) *= g_gaining;
        break;
    }
    case ADLMIDI_SampleType_U8:
    {
        uint8_t *buf = buffer;
        size_t samples = bufferSize;
        for(i = 0; i < samples; ++i)
        {
            int8_t s = static_cast<int8_t>(static_cast<int32_t>(*buf) + (-0x7f - 1)) * g_gaining;
            *(buf++) = static_cast<uint8_t>(static_cast<int32_t>(s) - (-0x7f - 1));
        }
        break;
    }
    case ADLMIDI_SampleType_S16:
    {
        int16_t *buf = reinterpret_cast<int16_t *>(buffer);
        size_t samples = bufferSize / g_audioFormat.containerSize;
        for(i = 0; i < samples; ++i)
            *(buf++) *= g_gaining;
        break;
    }
    case ADLMIDI_SampleType_U16:
    {
        uint16_t *buf = reinterpret_cast<uint16_t *>(buffer);
        size_t samples = bufferSize / g_audioFormat.containerSize;
        for(i = 0; i < samples; ++i)
        {
            int16_t s = static_cast<int16_t>(static_cast<int32_t>(*buf) + (-0x7fff - 1)) * g_gaining;
            *(buf++) = static_cast<uint16_t>(static_cast<int32_t>(s) - (-0x7fff - 1));
        }
        break;
    }
    case ADLMIDI_SampleType_S32:
    {
        int32_t *buf = reinterpret_cast<int32_t *>(buffer);
        size_t samples = bufferSize / g_audioFormat.containerSize;
        for(i = 0; i < samples; ++i)
            *(buf++) *= g_gaining;
        break;
    }
    case ADLMIDI_SampleType_F32:
    {
        float *buf = reinterpret_cast<float *>(buffer);
        size_t samples = bufferSize / g_audioFormat.containerSize;
        for(i = 0; i < samples; ++i)
            *(buf++) *= g_gaining;
        break;
    }
    default:
        break;
    }
}
#endif
