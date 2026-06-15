/* Copyright (C) 2023 NY00123
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

#include <alsa/asoundlib.h>
#include <adlmidi.h>
#include <signal.h>
#include <stdlib.h>
#include "SDL.h"

typedef struct ALSA_Seq_Data
{
    snd_seq_t *handle;
    int port;
    unsigned n_connections;
} ALSA_Seq_Data;

typedef struct Audio_Device_Data
{
    SDL_AudioDeviceID id;
    int freq;
} Audio_Device_Data;

typedef struct SeqSettings_t
{
    int     bankId;
    char    bankPath[2048];

    float   gain;
    int     audioFreq;
    Uint8   audioChans;
    SDL_AudioFormat audioFormat;

    enum ADL_Emulator           chipEmu;
    enum ADLMIDI_VolumeModels   volumeModel;
    enum ADLMIDI_ChannelAlloc   chanAlloc;

    int     numChips;
    int     num4ops;

    int     flagDeepTremolo;
    int     flagDeepVibrato;
    int     flagFullPanStereo;
    int     flagScalableModulators;
    int     flagAutoArpeggio;

    int     hwSerialEnable;
    char    hwSerialDevice[256];
    int     hwSerialBaud;
    enum ADL_SerialProtocol hwSerialProto;
} SeqSettings;

/* ALSA sequencer */

static SeqSettings g_settings;

struct ADLMIDI_AudioFormat g_audioFormat;

void fillAudioFormat(const SDL_AudioSpec *spec)
{
    switch(spec->format)
    {
    case AUDIO_S8:
        g_audioFormat.type = ADLMIDI_SampleType_S8;
        g_audioFormat.containerSize = sizeof(int8_t);
        g_audioFormat.sampleOffset = sizeof(int8_t) * 2;
        break;
    case AUDIO_U8:
        g_audioFormat.type = ADLMIDI_SampleType_U8;
        g_audioFormat.containerSize = sizeof(uint8_t);
        g_audioFormat.sampleOffset = sizeof(uint8_t) * 2;
        break;
    case AUDIO_S16LSB:
    case AUDIO_S16MSB:
        g_audioFormat.type = ADLMIDI_SampleType_S16;
        g_audioFormat.containerSize = sizeof(int16_t);
        g_audioFormat.sampleOffset = sizeof(int16_t) * 2;
        break;
    case AUDIO_U16LSB:
    case AUDIO_U16MSB:
        g_audioFormat.type = ADLMIDI_SampleType_U16;
        g_audioFormat.containerSize = sizeof(uint16_t);
        g_audioFormat.sampleOffset = sizeof(uint16_t) * 2;
        break;
    case AUDIO_S32LSB:
    case AUDIO_S32MSB:
        g_audioFormat.type = ADLMIDI_SampleType_S32;
        g_audioFormat.containerSize = sizeof(int32_t);
        g_audioFormat.sampleOffset = sizeof(int32_t) * 2;
        break;
    case AUDIO_F32LSB:
    case AUDIO_F32MSB:
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
        int8_t *buf = (int8_t *)(buffer);
        size_t samples = bufferSize;
        for(i = 0; i < samples; ++i)
            *(buf++) *= g_settings.gain;
        break;
    }
    case ADLMIDI_SampleType_U8:
    {
        uint8_t *buf = buffer;
        size_t samples = bufferSize;
        for(i = 0; i < samples; ++i)
        {
            int8_t s = (int8_t)((int32_t)*buf + (-0x7f - 1)) * g_settings.gain;
            *(buf++) = (uint8_t)((int32_t)s - (-0x7f - 1));
        }
        break;
    }
    case ADLMIDI_SampleType_S16:
    {
        int16_t *buf = (int16_t *)buffer;
        size_t samples = bufferSize / g_audioFormat.containerSize;
        for(i = 0; i < samples; ++i)
            *(buf++) *= g_settings.gain;
        break;
    }
    case ADLMIDI_SampleType_U16:
    {
        uint16_t *buf = (uint16_t *)buffer;
        size_t samples = bufferSize / g_audioFormat.containerSize;
        for(i = 0; i < samples; ++i)
        {
            int16_t s = (int16_t)((int32_t)*buf + (-0x7fff - 1)) * g_settings.gain;
            *(buf++) = (uint16_t)((int32_t)s - (-0x7fff - 1));
        }
        break;
    }
    case ADLMIDI_SampleType_S32:
    {
        int32_t *buf = (int32_t *)buffer;
        size_t samples = bufferSize / g_audioFormat.containerSize;
        for(i = 0; i < samples; ++i)
            *(buf++) *= g_settings.gain;
        break;
    }
    case ADLMIDI_SampleType_F32:
    {
        float *buf = (float *)buffer;
        size_t samples = bufferSize / g_audioFormat.containerSize;
        for(i = 0; i < samples; ++i)
            *(buf++) *= g_settings.gain;
        break;
    }
    default:
        break;
    }
}

static int check_seq_error(int ret, const char *msg)
{
    if(ret < 0)
        puts(msg);

    return ret;
}

static int midi_open(ALSA_Seq_Data *data)
{
    if(check_seq_error(snd_seq_open(&data->handle, "default", SND_SEQ_OPEN_INPUT, 0), "Could not open sequencer") < 0)
        return -1;

    if(check_seq_error(snd_seq_set_client_name(data->handle, "libADLMIDI"), "Could not set client name") < 0)
        return -1;

    data->port = snd_seq_create_simple_port(
                  data->handle, "libADLMIDI port",
                  SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
                  SND_SEQ_PORT_TYPE_APPLICATION|SND_SEQ_PORT_TYPE_MIDI_GENERIC);

    if(check_seq_error(data->port, "Could not create sequencer port") < 0)
        return -1;

    data->n_connections = 0;

    printf("Opened sequencer on %d:%d\n", snd_seq_client_id(data->handle), data->port);

    return 0;
}

static void midi_close(const ALSA_Seq_Data *data)
{
    check_seq_error(snd_seq_delete_simple_port(data->handle, data->port), "Could not delete sequencer port");
    check_seq_error(snd_seq_close(data->handle), "Could not close sequencer");
}

/* ADLMIDI player */

static int check_adl_error(int ret, struct ADL_MIDIPlayer *player, const char *msg)
{
    if(ret < 0)
        printf("%s: %s\n", msg, adl_errorInfo(player));

    return ret;
}

static struct ADL_MIDIPlayer *player_open(void)
{
    struct ADL_MIDIPlayer *player = adl_init(g_settings.audioFreq);
    if (!player)
    {
        printf("ADLMIDI player initialization failed: %s\n", adl_errorString());
        return NULL;
    }

    if(check_adl_error(adl_switchEmulator(player, ADLMIDI_EMU_NUKED), player, "Switching ADLMIDI emulator failed") < 0)
    {
        adl_close(player);
        return NULL;
    }

    if(g_settings.bankPath[0])
    {
        if(check_adl_error(adl_openBankFile(player, g_settings.bankPath), player, "Loading external ADLMIDI bank failed") < 0)
        {
            adl_close(player);
            return NULL;
        }
    }
    else if(check_adl_error(adl_setBank(player, g_settings.bankId), player, "Setting ADLMIDI bank failed") < 0)
    {
        adl_close(player);
        return NULL;
    }

    adl_setNumFourOpsChn(player, g_settings.num4ops);
    adl_setNumChips(player, g_settings.numChips);
    adl_setVolumeRangeModel(player, g_settings.volumeModel);
    adl_setChannelAllocMode(player, g_settings.chanAlloc);
    adl_setAutoArpeggio(player, g_settings.flagAutoArpeggio);
    adl_setHTremolo(player, g_settings.flagDeepTremolo);
    adl_setHVibrato(player, g_settings.flagDeepVibrato);
    adl_setScaleModulators(player, g_settings.flagScalableModulators);
    adl_setSoftPanEnabled(player, g_settings.flagFullPanStereo);

    adl_rt_resetState(player);

    return player;
}

static void player_close(struct ADL_MIDIPlayer *player)
{
    adl_close(player);
}

/* SDL audio device */

static int digi_open(Audio_Device_Data *data, struct ADL_MIDIPlayer **player_ptr, SDL_AudioCallback callback)
{
    SDL_AudioSpec desired_spec, obtained_spec;

    if(SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        printf("SDL_Init failed, %s\n", SDL_GetError());
        exit(3);
    }

    desired_spec.format = g_settings.audioFormat;
    desired_spec.channels = g_settings.audioChans;
    desired_spec.freq = g_settings.audioFreq;
    desired_spec.samples = 1024;
    desired_spec.callback = callback;
    desired_spec.userdata = player_ptr;
    data->id = SDL_OpenAudioDevice(NULL, 0, &desired_spec, &obtained_spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);

    if (data->id <= 0)
    {
        printf("SDL_OpenAudioDevice failed, %s\n", SDL_GetError());
        return -1;
    }

    printf("SDL audio device opened, requested rate %d, got %d\n", desired_spec.freq, obtained_spec.freq);
    g_settings.audioFreq = data->freq = obtained_spec.freq;
    fillAudioFormat(&obtained_spec);

    return 0;
}

static void digi_close(const Audio_Device_Data *data)
{
    SDL_CloseAudioDevice(data->id);
    SDL_Quit();
}

/* Making it all work */

void midi_process(struct ADL_MIDIPlayer *player, ALSA_Seq_Data *seq_data, SDL_AudioDeviceID audio_device)
{
    snd_seq_event_t *ev = NULL;
    snd_seq_event_input(seq_data->handle, &ev);

    if(!ev)
        return;

    if((ev->type == SND_SEQ_EVENT_PORT_SUBSCRIBED) || (ev->type == SND_SEQ_EVENT_PORT_UNSUBSCRIBED))
    {
        printf("%s, sender %u:%u, dst %u:%u\n",
               ev->type == SND_SEQ_EVENT_PORT_SUBSCRIBED ? "Subscribed" : "Unsubscribed",
               ev->data.connect.sender.client, ev->data.connect.sender.port,
               ev->data.connect.dest.client, ev->data.connect.dest.port);

        if (ev->type == SND_SEQ_EVENT_PORT_SUBSCRIBED)
        {
            if (!(seq_data->n_connections++))
                SDL_PauseAudioDevice(audio_device, 0);
        }
        else
        {
            if (!(--seq_data->n_connections))
                SDL_PauseAudioDevice(audio_device, 1);
        }
    }

    SDL_LockAudioDevice(audio_device);

    switch (ev->type)
    {
    case SND_SEQ_EVENT_NOTEON:
        adl_rt_noteOn(player, ev->data.note.channel, ev->data.note.note, ev->data.note.velocity);
        break;
    case SND_SEQ_EVENT_NOTEOFF:
        adl_rt_noteOff(player, ev->data.note.channel, ev->data.note.note);
        break;
    case SND_SEQ_EVENT_KEYPRESS:
        adl_rt_noteAfterTouch(player, ev->data.note.channel, ev->data.note.note, ev->data.note.velocity);
        break;
    case SND_SEQ_EVENT_CONTROLLER:
        adl_rt_controllerChange(player, ev->data.control.channel, ev->data.control.param, ev->data.control.value);
        break;
    case SND_SEQ_EVENT_PGMCHANGE:
        adl_rt_patchChange(player, ev->data.control.channel, ev->data.control.value);
        break;
    case SND_SEQ_EVENT_CHANPRESS:
        adl_rt_channelAfterTouch(player, ev->data.control.channel, ev->data.control.value);
        break;
    case SND_SEQ_EVENT_PITCHBEND:
        adl_rt_pitchBend(player, ev->data.control.channel, ev->data.control.value + 8192);
        break;
    case SND_SEQ_EVENT_SYSEX:
        adl_rt_systemExclusive(player, (ADL_UInt8 *)ev->data.ext.ptr, ev->data.ext.len);
        break;

    case SND_SEQ_EVENT_PORT_SUBSCRIBED:
    case SND_SEQ_EVENT_PORT_UNSUBSCRIBED:
        adl_reset(player);
        break;
    }

    SDL_UnlockAudioDevice(audio_device);
}

static void sound_callback(void *player_ptr, Uint8 *stream, int len)
{
    adl_generateFormat(*(struct ADL_MIDIPlayer **)player_ptr,
                       len / g_audioFormat.containerSize,
                       stream,
                       stream + g_audioFormat.containerSize,
                       &g_audioFormat);

    applyGain(stream, len);
}

// The rest of the code

static volatile sig_atomic_t keep_running = 1;

static void signal_callback(int signum)
{
    if (signum == SIGINT)
        keep_running = 0;
}

static void printUsage()
{
    int i, n_banks = adl_getBanksCount();
    const char *const *bank_names = adl_getBankNames();

    puts("=============================\n"
         "  libADLMIDI ALSA sequencer  \n"
         "=============================\n"
         "\n"
         "Usage: adlalsaseq <bank>\n"
         "\n"
         "    Available embedded banks by number:\n");

    for (i = 0; i < n_banks; i++)
        printf("          %2d = %s\n", i, bank_names[i]);

    puts("");
}

static int is_number(const char *s)
{
    if(s == NULL || *s == '\0')
        return 0;

    while(*s != '\0' && isdigit(*s))
        ++s;

    return *s == '\0';
}

static void argShift(int *m_argc, char ***m_argv)
{
    --(*m_argc);
    ++(*m_argv);
}

static int check_args(int argc, char **argv, int *out_bank_id)
{
    char *cur;

    memset(&g_settings, 0, sizeof(SeqSettings));

    /* Default Setup */
    g_settings.flagDeepTremolo = -1;
    g_settings.flagDeepVibrato = -1;
    g_settings.flagFullPanStereo = 0;
    g_settings.flagScalableModulators = -1;
    g_settings.flagAutoArpeggio = 0;
    g_settings.chipEmu = ADLMIDI_EMU_NUKED_FAST;
    g_settings.numChips = 2;
    g_settings.num4ops = -1;
    g_settings.gain = 2.0f;
    g_settings.hwSerialProto = ADLMIDI_SerialProtocol_RetroWaveOPL3;

    g_settings.audioChans = 2;
    g_settings.audioFormat = AUDIO_S16SYS;
    g_settings.audioFreq = 49716;

    g_audioFormat.type = ADLMIDI_SampleType_S16;
    g_audioFormat.containerSize = sizeof(Sint16);
    g_audioFormat.sampleOffset = 2 * g_audioFormat.containerSize;

    if(argc < 2 || !strcmp(argv[1], "--help"))
    {
        printUsage();
        return -1;
    }

    argShift(&argc, &argv);

    while(argc > 0)
    {
        cur = *argv;
        if(!SDL_strcmp(cur, "-bank"))
        {
            argShift(&argc, &argv);
            if(argc == 0 || !*argv)
                break;

            if(is_number(*argv))
                g_settings.bankId = atoi(*argv);
            else
                strncpy(g_settings.bankPath, *argv, sizeof(g_settings.bankPath));
        }
        else if(!SDL_strcmp(cur, "-chips"))
        {
            argShift(&argc, &argv);
            if(argc == 0)
                break;

            g_settings.numChips = atoi(*argv);
        }
        else if(!SDL_strcmp(cur, "-4ops"))
        {
            argShift(&argc, &argv);
            if(argc == 0)
                break;

            g_settings.num4ops = atoi(*argv);
        }
        else if(!SDL_strcmp(cur, "-vm"))
        {
            argShift(&argc, &argv);
            if(argc == 0)
                break;

            g_settings.volumeModel = atoi(*argv);
        }
        else if(!SDL_strcmp(cur, "-ca"))
        {
            argShift(&argc, &argv);
            if(argc == 0)
                break;

            g_settings.chanAlloc = atoi(*argv);
        }
        else if(!SDL_strcmp(cur, "-gain"))
        {
            argShift(&argc, &argv);
            if(argc == 0)
                break;

            g_settings.gain = atof(*argv);
        }
        else if(!SDL_strcmp(cur, "-dt"))
            g_settings.flagDeepTremolo = 1;
        else if(!SDL_strcmp(cur, "-dv"))
            g_settings.flagDeepVibrato = 1;
        else if(!SDL_strcmp(cur, "-sm"))
            g_settings.flagScalableModulators = 1;
        else if(!SDL_strcmp(cur, "-fp"))
            g_settings.flagFullPanStereo = 1;
        else if(!SDL_strcmp(cur, "-aa"))
            g_settings.flagAutoArpeggio = 1;
        else if(!SDL_strcmp(cur, "-f32"))
            g_settings.audioFormat = AUDIO_F32SYS;
        else if(!SDL_strcmp(cur, "-s32"))
            g_settings.audioFormat = AUDIO_S32SYS;
        else if(!SDL_strcmp(cur, "-s16"))
            g_settings.audioFormat = AUDIO_S16SYS;
        else if(!SDL_strcmp(cur, "-u8"))
            g_settings.audioFormat = AUDIO_U8;
        else if(!SDL_strcmp(cur, "-s8"))
            g_settings.audioFormat = AUDIO_S8;

        argShift(&argc, &argv);
    }

    *out_bank_id = g_settings.bankId;

    return 0;
}

int main(int argc, char **argv)
{
    int bank_id = 0;
    ALSA_Seq_Data seq_data;
    Audio_Device_Data device_data;
    struct ADL_MIDIPlayer *player = 0;
    struct sigaction action;

    if(check_args(argc, argv, &bank_id) < 0)
        return 1;

    if(midi_open(&seq_data) < 0)
    {
        return 2;
    }

    if(digi_open(&device_data, &player, sound_callback) < 0)
    {
        return 3;
    }

    memset(&action, 0, sizeof(action));
    action.sa_handler = signal_callback;

    if(sigaction(SIGINT, &action, NULL) < 0)
    {
        perror("Setting signal handler failed: ");
        return 1;
    }

    player = player_open();

    if(!player)
    {
        digi_close(&device_data);
        midi_close(&seq_data);
        return 4;
    }


    while(keep_running)
        midi_process(player, &seq_data, device_data.id);

    digi_close(&device_data);
    player_close(player);
    midi_close(&seq_data);
    puts("\nDone");

    return 0;
}
