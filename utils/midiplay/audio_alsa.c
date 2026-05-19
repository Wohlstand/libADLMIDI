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

#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <alsa/asoundlib.h>
#ifndef __STDC_NO_ATOMICS__
#include <stdatomic.h>
#endif

#include <adlmidi.h>
#include "audio.h"

#define PCM_DEVICE "default"


struct Audio_t
{
    unsigned int tmp, dir, period_time;
    int seconds, pcm;
    unsigned int rate, channels;
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_uframes_t frames;
    int frame_size;
    unsigned char *buff;
    int buff_size, loops;
    AudioOutputCallback callback;

    _Atomic int running;
    pthread_t runner;
    pthread_mutex_t audio_lock;
};

static struct Audio_t s_audio;
static char s_audioError[1024] = "";

static int audio_next_format(int prev_format)
{
    if(prev_format == ADLMIDI_SampleType_F64)
        return ADLMIDI_SampleType_F32;
    else if(prev_format == ADLMIDI_SampleType_F32)
        return ADLMIDI_SampleType_S32;
    else if(prev_format == ADLMIDI_SampleType_U32)
        return ADLMIDI_SampleType_S32;
    else if(prev_format == ADLMIDI_SampleType_S32)
        return ADLMIDI_SampleType_S24;
    else if(prev_format == ADLMIDI_SampleType_U24)
        return ADLMIDI_SampleType_S24;
    else if(prev_format == ADLMIDI_SampleType_S24)
        return ADLMIDI_SampleType_S16;
    else if(prev_format == ADLMIDI_SampleType_U16)
        return ADLMIDI_SampleType_S16;
    else if(prev_format == ADLMIDI_SampleType_S16)
        return ADLMIDI_SampleType_U8;
    else if(prev_format == ADLMIDI_SampleType_S8)
        return ADLMIDI_SampleType_U8;

    return -1;
}

int audio_init(struct AudioOutputSpec *in_spec, struct AudioOutputSpec *out_obtained, AudioOutputCallback callback)
{
    snd_pcm_format_t out_format = SND_PCM_FORMAT_S16;
    int sample_size = 2, test_format;
    unsigned int periods;

    memset(&s_audio, 0, sizeof(s_audio));
    memcpy(out_obtained, in_spec, sizeof(struct AudioOutputSpec));

    s_audio.rate = in_spec->freq;
    s_audio.channels = in_spec->channels;
    s_audio.frames = in_spec->samples;
    s_audio.callback = callback;

#if __BYTE_ORDER == __LITTLE_ENDIAN
    out_obtained->is_msb = 0;
#else
    out_obtained->is_msb = 1;
#endif

    s_audio.pcm = snd_pcm_open(&s_audio.pcm_handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);

    if(s_audio.pcm < 0)
    {
        snprintf(s_audioError, 1024, "ALSA ERROR: Can't open \"%s\" PCM device. %s\n", PCM_DEVICE, snd_strerror(s_audio.pcm));
        return -1;
    }

    /* Allocate parameters object and fill it with default values*/
    snd_pcm_hw_params_alloca(&s_audio.params);

    snd_pcm_hw_params_any(s_audio.pcm_handle, s_audio.params);

    /* Set parameters */
    s_audio.pcm = snd_pcm_hw_params_set_access(s_audio.pcm_handle, s_audio.params, SND_PCM_ACCESS_RW_INTERLEAVED);

    if(s_audio.pcm < 0)
    {
        snprintf(s_audioError, 1024, "ALSA ERROR: Can't set interleaved mode. %s\n", snd_strerror(s_audio.pcm));
        audio_close();
        return -1;
    }


    for(test_format = in_spec->format; test_format >= 0; test_format = audio_next_format(test_format))
    {
        switch(test_format)
        {
        case ADLMIDI_SampleType_S8:
            out_format = SND_PCM_FORMAT_S8; sample_size = 1; break;
        case ADLMIDI_SampleType_U8:
            out_format = SND_PCM_FORMAT_U8; sample_size = 1; break;
        case ADLMIDI_SampleType_S16:
            out_format = SND_PCM_FORMAT_S16; sample_size = 2; break;
        case ADLMIDI_SampleType_U16:
            out_format = SND_PCM_FORMAT_U16; sample_size = 2; break;
        case ADLMIDI_SampleType_S24:
            out_format = SND_PCM_FORMAT_S24; sample_size = 3; break;
        case ADLMIDI_SampleType_U24:
            out_format = SND_PCM_FORMAT_U24; sample_size = 3; break;
        case ADLMIDI_SampleType_S32:
            out_format = SND_PCM_FORMAT_S32; sample_size = 4; break;
        case ADLMIDI_SampleType_U32:
            out_format = SND_PCM_FORMAT_U32; sample_size = 4; break;
        case ADLMIDI_SampleType_F32:
            out_format = SND_PCM_FORMAT_FLOAT; sample_size = 4; break;
        case ADLMIDI_SampleType_F64:
            out_format = SND_PCM_FORMAT_FLOAT64; sample_size = 4; break;
        }

        s_audio.pcm = snd_pcm_hw_params_set_format(s_audio.pcm_handle, s_audio.params, out_format);

        if(s_audio.pcm >= 0)
            break;
    }

    if(test_format < 0)
    {
        snprintf(s_audioError, 1024, "ALSA ERROR: Can't set format. %s\n", snd_strerror(s_audio.pcm));
        audio_close();
        return -1;
    }



    s_audio.pcm = snd_pcm_hw_params_set_channels_near(s_audio.pcm_handle, s_audio.params, &s_audio.channels);

    if(s_audio.pcm < 0)
    {
        snprintf(s_audioError, 1024, "ALSA ERROR: Can't set channels number. %s\n", snd_strerror(s_audio.pcm));
        audio_close();
        return -1;
    }

    out_obtained->channels = s_audio.channels;


    s_audio.pcm = snd_pcm_hw_params_set_rate_near(s_audio.pcm_handle, s_audio.params, &s_audio.rate, 0);

    if(s_audio.pcm < 0)
    {
        snprintf(s_audioError, 1024, "ALSA ERROR: Can't set rate. %s\n", snd_strerror(s_audio.pcm));
        audio_close();
        return -1;
    }

    out_obtained->freq = s_audio.rate;

    s_audio.pcm = snd_pcm_hw_params_set_period_size_near(s_audio.pcm_handle, s_audio.params, &s_audio.frames, 0);

    if(s_audio.pcm < 0)
    {
        snprintf(s_audioError, 1024, "ALSA ERROR: Can't set period size. %s\n", snd_strerror(s_audio.pcm));
        audio_close();
        return -1;
    }

    periods = 2;
    s_audio.pcm = snd_pcm_hw_params_set_periods_min(s_audio.pcm_handle, s_audio.params, &periods, NULL);
    if(s_audio.pcm < 0)
    {
        snprintf(s_audioError, 1024, "ALSA ERROR: Can't set minimum period size. %s\n", snd_strerror(s_audio.pcm));
        audio_close();
        return -1;
    }

    s_audio.pcm = snd_pcm_hw_params_set_periods_first(s_audio.pcm_handle, s_audio.params, &periods, NULL);
    if(s_audio.pcm < 0)
    {
        snprintf(s_audioError, 1024, "ALSA ERROR: Can't set first period size. %s\n", snd_strerror(s_audio.pcm));
        audio_close();
        return -1;
    }

    out_obtained->samples = s_audio.frames;

    s_audio.pcm = snd_pcm_hw_params(s_audio.pcm_handle, s_audio.params);

    if(s_audio.pcm < 0)
    {
        snprintf(s_audioError, 1024, "ALSA ERROR: Can't set harware parameters. %s\n", snd_strerror(s_audio.pcm));
        audio_close();
        return -1;
    }


    s_audio.buff_size = s_audio.frames * s_audio.channels * sample_size;
    s_audio.buff = (unsigned char *)malloc(s_audio.buff_size);
    s_audio.frame_size = sample_size * s_audio.channels;

    snd_pcm_hw_params_get_period_time(s_audio.params, &s_audio.period_time, NULL);

    atomic_init(&s_audio.running, 0);

    pthread_mutex_init(&s_audio.audio_lock, NULL);
    snd_pcm_nonblock(s_audio.pcm_handle, 0);

    return 0;
}

void audio_close(void)
{
    audio_stop();

    if(s_audio.pcm_handle)
    {
        snd_pcm_drain(s_audio.pcm_handle);
        snd_pcm_close(s_audio.pcm_handle);
    }

    if(s_audio.buff)
        free(s_audio.buff);

    memset(&s_audio, 0, sizeof(s_audio));
}

const char* audio_get_error(void)
{
    return s_audioError;
}

static void* audio_runner(void *nothing)
{
    snd_pcm_sframes_t rc;
    unsigned char *write_buff;
    int write_buff_len;
    (void)nothing;

    while(atomic_load(&s_audio.running))
    {
        s_audio.callback(NULL, s_audio.buff, s_audio.buff_size);

        write_buff = s_audio.buff;
        write_buff_len = s_audio.frames;

        while(write_buff_len > 0 && atomic_load(&s_audio.running))
        {
            s_audio.pcm = snd_pcm_writei(s_audio.pcm_handle, write_buff, write_buff_len);

            if(s_audio.pcm < 0)
            {
                if(s_audio.pcm == -EAGAIN)
                {
                    audio_delay(1);
                    continue;
                }

                s_audio.pcm = snd_pcm_recover(s_audio.pcm_handle, s_audio.pcm, 1);

                if(s_audio.pcm < 0)
                {
                    fprintf(stderr, "ALSA: write failed (unrecoverable): %s\n", snd_strerror(s_audio.pcm));
                    fflush(stderr);
                    atomic_store(&s_audio.running, 0);
                    break;
                }

                continue;
            }
            else if(s_audio.pcm == 0)
            {
                unsigned int delay = (write_buff_len / 2 * 1000) / s_audio.rate;
                audio_delay(delay);
                break;
            }

            write_buff += s_audio.pcm * s_audio.frame_size;
            write_buff_len -= s_audio.pcm;
        }

        while(atomic_load(&s_audio.running))
        {
            rc = snd_pcm_avail(s_audio.pcm_handle);

            if((rc < 0) && (rc != -EAGAIN))
            {
                fprintf(stderr, "ALSA: snd_pcm_avail failed (unrecoverable): %s\n", snd_strerror(rc));
                fflush(stderr);
                atomic_store(&s_audio.running, 0);
                break;
            }
            else if(rc < (snd_pcm_sframes_t)s_audio.frames)
            {
                unsigned int delay = ((s_audio.frames - (rc >= 0 ? rc : 0)) * 1000) / s_audio.rate;
                audio_delay(delay >= 10 ? delay : 10);
            }
            else
                break;
        }
    }

    return NULL;
}

void audio_start(void)
{
    if(s_audio.runner || atomic_load(&s_audio.running) == 1)
        return;

    snd_pcm_start(s_audio.pcm_handle);
    atomic_store(&s_audio.running, 1);
    pthread_create(&s_audio.runner, NULL, audio_runner, NULL);
}

void audio_stop(void)
{
    if(atomic_load(&s_audio.running) == 0)
        return;

    atomic_store(&s_audio.running, 0);
    pthread_join(s_audio.runner, NULL);
    snd_pcm_drop(s_audio.pcm_handle);
    s_audio.runner = 0;
}

void audio_lock(void)
{
    pthread_mutex_lock(&s_audio.audio_lock);
}

void audio_unlock(void)
{
    pthread_mutex_unlock(&s_audio.audio_lock);
}

void audio_delay(unsigned int ms)
{
    usleep(ms * 1000);
}

void* audio_mutex_create(void)
{
    pthread_mutex_t *mt = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(mt, NULL);
    return mt;
}

void  audio_mutex_destroy(void *m)
{
    pthread_mutex_t *mt = (pthread_mutex_t*)m;
    pthread_mutex_destroy(mt);
    free(mt);
}

void  audio_mutex_lock(void *m)
{
    pthread_mutex_t *mt = (pthread_mutex_t*)m;
    pthread_mutex_lock(mt);
}

void  audio_mutex_unlock(void *m)
{
    pthread_mutex_t *mt = (pthread_mutex_t*)m;
    pthread_mutex_unlock(mt);
}
