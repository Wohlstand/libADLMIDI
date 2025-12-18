/*
 * ADLMIDI Player is a free MIDI player based on a libADLMIDI,
 * a Software MIDI synthesizer library with OPL3 emulation
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

#include <adlmidi.h>
#include "audio.h"
#define SDL_MAIN_HANDLED

#if defined(ADLMIDI_USE_SDL3)
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_timer.h>
// Workarounds
typedef SDL_Mutex SDL_mutex;
#define AUDIO_S8 SDL_AUDIO_S8
#define AUDIO_U8 SDL_AUDIO_U8
#define AUDIO_S16SYS SDL_AUDIO_S16
#define AUDIO_S32SYS SDL_AUDIO_S32
#define AUDIO_F32SYS SDL_AUDIO_F32
#define AUDIO_S16MSB SDL_AUDIO_S16BE
#define AUDIO_S32MSB SDL_AUDIO_S32BE
#define AUDIO_F32MSB SDL_AUDIO_F32BE
#define AUDIO_S16LSB SDL_AUDIO_S16LE
#define AUDIO_S32LSB SDL_AUDIO_S32LE
#define AUDIO_F32LSB SDL_AUDIO_F32LE
#elif defined(ADLMIDI_USE_SDL2)
#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_timer.h>
#endif

#if defined(ADLMIDI_USE_SDL3)
static SDL_AudioStream *s_stream = NULL;
static AudioOutputCallback s_callback = NULL;

static void SDLCALL audio_sdl3_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
    (void)total_amount;
    if(additional_amount > 0)
    {
        Uint8 *data = SDL_stack_alloc(Uint8, additional_amount);
        if(data)
        {
            s_callback(userdata, data, additional_amount);
            SDL_PutAudioStreamData(stream, data, additional_amount);
            SDL_stack_free(data);
        }
    }
}
#endif


int audio_init(struct AudioOutputSpec *in_spec, struct AudioOutputSpec *out_obtained, AudioOutputCallback callback)
{
    SDL_AudioSpec spec;
    SDL_AudioSpec obtained;
    int ret = 0;

    spec.format = AUDIO_S16SYS;
    spec.freq = (int)in_spec->freq;
#if !defined(ADLMIDI_USE_SDL3)
    spec.samples = in_spec->samples;
#endif
    spec.channels = in_spec->channels;
#if defined(ADLMIDI_USE_SDL3)
    s_callback = callback;
#else
    spec.callback = callback;
#endif

#if defined(ADLMIDI_USE_SDL3)
    if(!SDL_Init(SDL_INIT_AUDIO))
        return -1;
#endif

    switch(in_spec->format)
    {
    case ADLMIDI_SampleType_S8:
        spec.format = AUDIO_S8; break;
    case ADLMIDI_SampleType_U8:
        spec.format = AUDIO_U8; break;
    case ADLMIDI_SampleType_S16:
        spec.format = AUDIO_S16SYS;
        break;
    case ADLMIDI_SampleType_U16:
#if defined(ADLMIDI_USE_SDL3)
        return -1; // U16 is no longer supported!
#else
        spec.format = AUDIO_U16SYS;
        break;
#endif
    case ADLMIDI_SampleType_S32:
        spec.format = AUDIO_S32SYS;
        break;
    case ADLMIDI_SampleType_F32:
        spec.format = AUDIO_F32SYS;
        break;
    }

#if defined(ADLMIDI_USE_SDL3)
    s_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_sdl3_callback, NULL);
    if(!s_stream)
        return -1;
    SDL_memcpy(&obtained, &spec, sizeof(SDL_AudioSpec));
#else
    ret = SDL_OpenAudio(&spec, &obtained);
#endif

    out_obtained->channels = obtained.channels;
    out_obtained->freq = (unsigned int)obtained.freq;
#if defined(ADLMIDI_USE_SDL3)
    out_obtained->samples = in_spec->samples;
#else
    out_obtained->samples = obtained.samples;
#endif
    out_obtained->format = in_spec->format;
    out_obtained->is_msb = 0;

    switch(obtained.format)
    {
    case AUDIO_S8:
        out_obtained->format = ADLMIDI_SampleType_S8; break;
    case AUDIO_U8:
        out_obtained->format = ADLMIDI_SampleType_U8; break;
    case AUDIO_S16MSB:
        out_obtained->is_msb = 1;/* fallthrough */
    case AUDIO_S16LSB:
        out_obtained->format = ADLMIDI_SampleType_S16; break;
#if !defined(ADLMIDI_USE_SDL3)
    case AUDIO_U16MSB:
        out_obtained->is_msb = 1;/* fallthrough */
    case AUDIO_U16LSB:
        out_obtained->format = ADLMIDI_SampleType_U16; break;
#endif
    case AUDIO_S32MSB:
        out_obtained->is_msb = 1;/* fallthrough */
    case AUDIO_S32LSB:
        out_obtained->format = ADLMIDI_SampleType_S32; break;
    case AUDIO_F32MSB:
        out_obtained->is_msb = 1;/* fallthrough */
    case AUDIO_F32LSB:
        out_obtained->format = ADLMIDI_SampleType_F32; break;
    default:
        audio_close();
        return -1;
    }

    return ret;
}

void audio_close(void)
{
#if defined(ADLMIDI_USE_SDL3)
    SDL_DestroyAudioStream(s_stream);
    s_stream = NULL;
#else
    SDL_CloseAudio();
#endif
}

const char* audio_get_error(void)
{
    return SDL_GetError();
}

void audio_start(void)
{
#if defined(ADLMIDI_USE_SDL3)
    if(s_stream)
        SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(s_stream));
#else
    SDL_PauseAudio(0);
#endif
}

void audio_stop(void)
{
#if defined(ADLMIDI_USE_SDL3)
    if(s_stream)
        SDL_PauseAudioDevice(SDL_GetAudioStreamDevice(s_stream));
#else
    SDL_PauseAudio(1);
#endif
}

void audio_lock(void)
{
#if defined(ADLMIDI_USE_SDL3)
    if(s_stream)
        SDL_LockAudioStream(s_stream);
#else
    SDL_LockAudio();
#endif
}

void audio_unlock(void)
{
#if defined(ADLMIDI_USE_SDL3)
    if(s_stream)
        SDL_UnlockAudioStream(s_stream);
#else
    SDL_UnlockAudio();
#endif
}

void audio_delay(unsigned int ms)
{
    SDL_Delay(ms);
}

void* audio_mutex_create(void)
{
    return SDL_CreateMutex();
}

void  audio_mutex_destroy(void *m)
{
    SDL_mutex *mut = (SDL_mutex *)m;
    SDL_DestroyMutex(mut);
}

void  audio_mutex_lock(void *m)
{
    SDL_mutex *mut = (SDL_mutex *)m;
#if defined(ADLMIDI_USE_SDL3)
    SDL_LockMutex(mut);
#else
    SDL_mutexP(mut);
#endif
}

void  audio_mutex_unlock(void *m)
{
    SDL_mutex *mut = (SDL_mutex *)m;
#if defined(ADLMIDI_USE_SDL3)
    SDL_UnlockMutex(mut);
#else
    SDL_mutexV(mut);
#endif
}
