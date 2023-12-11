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

// ALSA sequencer

static void check_seq_error(int ret, const char *msg)
{
    if (ret < 0)
    {
        puts(msg);
        exit(1);
    }
}

static ALSA_Seq_Data midi_open(void)
{
    ALSA_Seq_Data data;

    check_seq_error(snd_seq_open(&data.handle, "default", SND_SEQ_OPEN_INPUT, 0),
                    "Could not open sequencer");

    check_seq_error(snd_seq_set_client_name(data.handle, "libADLMIDI"),
                    "Could not set client name");

    data.port = snd_seq_create_simple_port(
                  data.handle, "libADLMIDI port",
                  SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
                  SND_SEQ_PORT_TYPE_APPLICATION);
    check_seq_error(data.port, "Could not create sequencer port");

    data.n_connections = 0;
    printf("Opened sequencer on %d:%d\n",
           snd_seq_client_id(data.handle), data.port);


    return data;
}

static void midi_close(const ALSA_Seq_Data *data)
{
    check_seq_error(snd_seq_delete_simple_port(data->handle, data->port),
                    "Could not delete sequencer port");
    check_seq_error(snd_seq_close(data->handle), "Could not close sequencer");
}

// ADLMIDI player

static void check_adl_error(int ret, struct ADL_MIDIPlayer *player,
                            const char *msg)
{
    if (ret < 0)
    {
        printf("%s: %s\n", msg, adl_errorInfo(player));
        exit(1);
    }
}

static struct ADL_MIDIPlayer *player_open(int bank_id, int freq)
{
    struct ADL_MIDIPlayer *player = adl_init(freq);
    if (!player)
    {
        printf("ADLMIDI player initialization failed: %s\n", adl_errorString());
        exit(1);
    }

    check_adl_error(adl_switchEmulator(player, ADLMIDI_EMU_NUKED),
                    player, "Switching ADLMIDI emulator failed");
    check_adl_error(adl_setBank(player, bank_id),
                    player, "Setting ADLMIDI bank failed");

    adl_rt_resetState(player);
    return player;
}

static void player_close(struct ADL_MIDIPlayer *player) { adl_close (player); }

// SDL audio device

static Audio_Device_Data digi_open(struct ADL_MIDIPlayer **player_ptr,
                                   SDL_AudioCallback callback)
{
    SDL_AudioSpec desired_spec, obtained_spec;
    Audio_Device_Data data;
    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        printf("SDL_Init failed, %s\n", SDL_GetError());
        exit(3);
    }
    desired_spec.format = AUDIO_S16SYS;
    desired_spec.channels = 2;
    desired_spec.freq = 49716;
    desired_spec.samples = 1024;
    desired_spec.callback = callback;
    desired_spec.userdata = player_ptr;
    data.id = SDL_OpenAudioDevice(NULL, 0, &desired_spec, &obtained_spec,
                                  SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (data.id <= 0)
    {
        printf("SDL_OpenAudioDevice failed, %s\n", SDL_GetError());
        exit(3);
    }

    printf("SDL audio device opened, requested rate %d, got %d\n",
           desired_spec.freq, obtained_spec.freq);
    data.freq = obtained_spec.freq;

    return data;
}

static void digi_close(const Audio_Device_Data *data)
{
    SDL_CloseAudioDevice(data->id);
    SDL_Quit();
}

// Making it all work

void midi_process(struct ADL_MIDIPlayer *player, ALSA_Seq_Data *seq_data,
                  SDL_AudioDeviceID audio_device)
{
    snd_seq_event_t *ev = NULL;
    snd_seq_event_input(seq_data->handle, &ev);

    if (!ev)
        return;

    if ((ev->type == SND_SEQ_EVENT_PORT_SUBSCRIBED) ||
        (ev->type == SND_SEQ_EVENT_PORT_UNSUBSCRIBED))
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
    adl_generate(*(struct ADL_MIDIPlayer **)player_ptr,
                 len / 2, (short *)stream);
}

// The rest of the code

static volatile sig_atomic_t keep_running = 1;

static void signal_callback(int signum)
{
    if (signum == SIGINT)
        keep_running = 0;
}

static void check_args(int argc, char **argv, int *out_bank_id)
{
    int i, n_banks = adl_getBanksCount();
    const char *const *bank_names = adl_getBankNames();

    int bank_id = (argc == 2) ? atoi(argv[1]) : -1;

    if ((bank_id < 0) || (bank_id >= n_banks))
    {
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
        exit(0);
    }
    *out_bank_id = bank_id;
}

int main(int argc, char **argv)
{
    int bank_id = 0;
    check_args(argc, argv, &bank_id);

    ALSA_Seq_Data seq_data = midi_open();
    struct ADL_MIDIPlayer *player = 0;
    Audio_Device_Data device_data = digi_open(&player, sound_callback);

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = signal_callback;
    if (sigaction(SIGINT, &action, NULL) < 0)
    {
        perror("Setting signal handler failed: ");
        exit(1);
    }

    player = player_open(bank_id, device_data.freq);

    while (keep_running)
        midi_process(player, &seq_data, device_data.id);

    digi_close(&device_data);
    player_close(player);
    midi_close(&seq_data);
    puts("\nDone");

    return 0;
}
