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

#include <cstdio>
#include <vector>
#include <cstring>
#include "playback.h"
#include "../dev_setup.h"
#include "../time_counter.h"
#include "../misc.h"


class MutexType
{
    void *mut;
public:
    MutexType() : mut(audio_mutex_create()) { }
    ~MutexType()
    {
        audio_mutex_destroy(mut);
    }
    void Lock()
    {
        audio_mutex_lock(mut);
    }
    void Unlock()
    {
        audio_mutex_unlock(mut);
    }
};

typedef std::vector<uint8_t> AudioBuff;
static AudioBuff g_audioBuffer;
static MutexType g_audioBuffer_lock;

struct AudioPlaybackData
{
    ADL_MIDIPlayer *dev;
    bool isMono;
    bool bufferDone;
};

//#define DEBUG_SONG_CHANGE
//#define DEBUG_SONG_CHANGE_BY_HOOK

#ifdef DEBUG_SONG_CHANGE_BY_HOOK
static bool gotXmiTrigger = false;

static void xmiTriggerCallback(void *, unsigned trigger, size_t track)
{
    s_fprintf(stdout, " - Trigger hook: trigger %u, track: %u\n", trigger, (unsigned)track);
    flushout(stdout);
    gotXmiTrigger = true;
}

static void loopEndCallback(void *)
{
    s_fprintf(stdout, " - Loop End hook: trigger\n");
    flushout(stdout);
    gotXmiTrigger = true;
}
#endif


static void s_audioPlaybackCallback(void *p_data, uint8_t *stream, int len)
{
    AudioPlaybackData *data = (AudioPlaybackData*)p_data;
    int querySize = len / g_audioFormat.containerSize;
    int got;

    audio_lock();

    g_audioBuffer_lock.Lock();

    if(data->isMono)
        querySize *= 2;

    got = (size_t)adl_playFormat(data->dev, querySize,
                                 g_audioBuffer.data(),
                                 g_audioBuffer.data() + g_audioFormat.containerSize,
                                 &g_audioFormat) * g_audioFormat.containerSize;
    if(got <= 0)
    {
        data->bufferDone = true;
        g_audioBuffer_lock.Unlock();
        return;
    }

    if(data->isMono)
        got = stereoToMono(g_audioBuffer.data(), got);

    for(int a = 0; a < got; ++a)
        stream[a] = g_audioBuffer[a];

    applyGain(stream, len);

    g_audioBuffer_lock.Unlock();

    audio_unlock();
}


int runAudioLoop(ADL_MIDIPlayer *myDevice, AudioOutputSpec &spec)
{
    AudioOutputSpec obtained;
    AudioPlaybackData audioData;
    audioData.dev = myDevice;
    audioData.bufferDone = false;
    audioData.isMono = false;

    // Set up Audio Output
    if(audio_init(&spec, &obtained, s_audioPlaybackCallback, &audioData) < 0)
    {
        s_fprintf(stdout, "\nERROR: Couldn't open audio: %s\n\n", audio_get_error());
        return 1;
    }

    if(spec.samples != obtained.samples || spec.freq != obtained.freq || spec.format != obtained.format)
    {
        s_fprintf(stdout, " - Audio wanted (format=%s,samples=%u,rate=%u,channels=%u);\n"
                          " - Audio obtained (format=%s,samples=%u,rate=%u,channels=%u)\n",
                  audio_format_to_str(spec.format, spec.is_msb),         spec.samples,     spec.freq,     spec.channels,
                  audio_format_to_str(obtained.format, obtained.is_msb), obtained.samples, obtained.freq, obtained.channels);
    }

    fillAudioFormat(obtained);

#if defined(ENABLE_TERMINAL_HOTKEYS)
    int bankId = 59;
#endif

#if defined(DEBUG_SONG_SWITCHING)
    int songsCount = adl_getSongsCount(myDevice);
#endif

#   ifdef DEBUG_SONG_CHANGE
    int delayBeforeSongChange = 50;
    s_fprintf(stdout, "DEBUG: === Random song set test is active! ===\n");
    flushout(stdout);
#   endif

#   ifdef DEBUG_SEEKING_TEST
    int delayBeforeSeek = 50;
    s_fprintf(stdout, "DEBUG: === Random position set test is active! ===\n");
    flushout(stdout);
#   endif

#ifdef ADLMIDI_PLAY_ENABLE_NCURSES
    setenv("TERMINFO", "/usr/share/terminfo", 1);
    setenv("TERM", "linux", 1);
    initscr();

    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);
    scrollok(stdscr, TRUE);
#endif

    audioData.isMono = obtained.channels == 1;
    g_audioBuffer.resize(obtained.samples * g_audioFormat.containerSize * 2);

    audio_start();

    setCursorVisibility(false);

    s_timeCounter.clearLineR();

    while(!stop)
    {
        g_audioBuffer_lock.Lock();

        if(audioData.bufferDone)
        {
            g_audioBuffer_lock.Unlock();
            break;
        }

#   ifdef DEBUG_TRACE_ALL_CHANNELS
        enum { TerminalColumns = 80 };
        char channelText[TerminalColumns + 1];
        char channelAttr[TerminalColumns + 1];
        adl_describeChannels(myDevice, channelText, channelAttr, sizeof(channelText));
        s_fprintf(stdout, "%*s\r", TerminalColumns, "");  // erase the line
        s_fprintf(stdout, "%s\n", channelText);
#   endif

#   ifndef DEBUG_TRACE_ALL_EVENTS
        s_timeCounter.printTime(adl_positionTell(myDevice));
#   endif

        g_audioBuffer_lock.Unlock();

        audio_delay(5);

#       if defined(DEBUG_SONG_SWITCHING) || defined(ENABLE_TERMINAL_HOTKEYS)
        if(kbhit())
        {
            int code = getch();

            if(code == '\033' && kbhit())
            {
                getch();
                switch(getch())
                {
#           if defined(DEBUG_SONG_SWITCHING)
                case 'C':
                    // code for arrow right
                    s_devSetup.songNumLoad++;
                    if(s_devSetup.songNumLoad >= songsCount)
                        s_devSetup.songNumLoad = songsCount;
                    g_audioBuffer_lock.Lock();
                    adl_selectSongNum(myDevice, s_devSetup.songNumLoad);
                    g_audioBuffer_lock.Unlock();
                    s_fprintf(stdout, "\rSwitching song to %d/%d...                               \r\n", s_devSetup.songNumLoad, songsCount);
                    flushout(stdout);
                    break;
                case 'D':
                    // code for arrow left
                    s_devSetup.songNumLoad--;
                    if(s_devSetup.songNumLoad < 0)
                        s_devSetup.songNumLoad = 0;
                    g_audioBuffer_lock.Lock();
                    adl_selectSongNum(myDevice, s_devSetup.songNumLoad);
                    g_audioBuffer_lock.Unlock();
                    s_fprintf(stdout, "\rSwitching song to %d/%d...                               \r\n", s_devSetup.songNumLoad, songsCount);
                    flushout(stdout);
                    break;
#endif
                }
            }
            else
            {
                switch(code)
                {
                case 'F':
                case 'f':
                    bankId++;
                    if(bankId >= adl_getBanksCount())
                        bankId = 0;
                    g_audioBuffer_lock.Lock();
                    adl_setBank(myDevice, bankId);
                    g_audioBuffer_lock.Unlock();
                    s_fprintf(stdout, "\rSwitching bank to %d/%d...                               \r\n", bankId, adl_getBanksCount());
                    flushout(stdout);
                    break;
                case 'V':
                case 'v':
                    bankId--;
                    if(bankId < 0)
                        bankId = adl_getBanksCount() - 1;
                    g_audioBuffer_lock.Lock();
                    adl_setBank(myDevice, bankId);
                    g_audioBuffer_lock.Unlock();
                    s_fprintf(stdout, "\rSwitching bank to %d/%d...                               \r\n", bankId, adl_getBanksCount());
                    flushout(stdout);
                    break;
                case 27: // Quit by ESC key
                    stop = 1;
                    break;
                }
            }
        }
#       endif

#       ifdef DEBUG_SEEKING_TEST
        if(delayBeforeSeek-- <= 0)
        {
            delayBeforeSeek = rand() % 50;
            double seekTo = double((rand() % int(adl_totalTimeLength(myDevice)) - delayBeforeSeek - 1 ));
            g_audioBuffer_lock.Lock();
            adl_positionSeek(myDevice, seekTo);
            g_audioBuffer_lock.Unlock();
        }
#       endif

#       ifdef DEBUG_SONG_CHANGE
        if(delayBeforeSongChange-- <= 0)
        {
            delayBeforeSongChange = rand() % 100;
            g_audioBuffer_lock.Lock();
            adl_selectSongNum(myDevice, rand() % 10);
            g_audioBuffer_lock.Unlock();
        }
#       endif

#       ifdef DEBUG_SONG_CHANGE_BY_HOOK
        if(gotXmiTrigger)
        {
            gotXmiTrigger = false;
            g_audioBuffer_lock.Lock();
            adl_selectSongNum(myDevice, (rand() % 10) + 1);
            g_audioBuffer_lock.Unlock();
        }
#       endif
    }

    setCursorVisibility(true);

    s_timeCounter.clearLine();

    audio_stop();
    audio_close();

    return 0;
}
