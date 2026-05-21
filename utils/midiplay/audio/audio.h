/*
 * Simple cross-platform Audio Output wrapper
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
#ifndef MIDIPLAY_AUDIO_H
#define MIDIPLAY_AUDIO_H

#ifdef __cplusplus
extern "C"
{
#endif

typedef void (*AudioOutputCallback)(void *, unsigned char *stream, int len);

/**
 * @brief Sound output format
 *
 * It should match the ADLMIDI_SampleType and OPNMIDI_SampleType!
 */
enum MidiPlay_SampleType
{
    /*! signed PCM 16-bit */
    MidiPlay_SampleType_S16 = 0,
    /*! signed PCM 8-bit */
    MidiPlay_SampleType_S8,
    /*! float 32-bit */
    MidiPlay_SampleType_F32,
    /*! float 64-bit */
    MidiPlay_SampleType_F64,
    /*! signed PCM 24-bit */
    MidiPlay_SampleType_S24,
    /*! signed PCM 32-bit */
    MidiPlay_SampleType_S32,
    /*! unsigned PCM 8-bit */
    MidiPlay_SampleType_U8,
    /*! unsigned PCM 16-bit */
    MidiPlay_SampleType_U16,
    /*! unsigned PCM 24-bit */
    MidiPlay_SampleType_U24,
    /*! unsigned PCM 32-bit */
    MidiPlay_SampleType_U32,
    /*! Count of available sample format types */
    MidiPlay_SampleType_Count
};

struct AudioOutputSpec
{
    unsigned int freq;
    unsigned short format;
    unsigned short is_msb;
    unsigned short samples;
    unsigned char  channels;
};

extern int audio_init(struct AudioOutputSpec *in_spec, struct AudioOutputSpec *out_obtained, AudioOutputCallback callback);

extern int audio_is_big_endian(void);

extern void audio_close(void);

extern const char* audio_get_error(void);

extern void audio_start(void);

extern void audio_stop(void);

extern void audio_lock(void);

extern void audio_unlock(void);

extern void audio_delay(unsigned int ms);

extern void* audio_mutex_create(void);
extern void  audio_mutex_destroy(void *m);
extern void  audio_mutex_lock(void *m);
extern void  audio_mutex_unlock(void *m);

#ifdef __cplusplus
}
#endif

#endif /* MIDIPLAY_AUDIO_H */
