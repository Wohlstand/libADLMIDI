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
#include <deque>
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

typedef std::deque<uint8_t> AudioBuff;
static AudioBuff g_audioBuffer;
static MutexType g_audioBuffer_lock;

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


static void s_audioPlaybackCallback(void *, uint8_t *stream, int len)
{
    unsigned ate = static_cast<unsigned>(len); // number of bytes

    audio_lock();
    //short *target = (short *) stream;
    g_audioBuffer_lock.Lock();

    if(g_audioBuffer.empty()) // Just in a case, if queue is blank, just zero the output
    {
        switch(g_audioFormat.type)
        {
        case ADLMIDI_SampleType_S8:
            std::memset(stream, 0x7f, len);
            break;
        case ADLMIDI_SampleType_U16:
            for(size_t i = 0; i < (size_t)len; i += 2)
                *(uint32_t*)(stream + i) = 0x7FFF;
            break;
        default:
            std::memset(stream, 0, len);
        }
    }

    if(ate > g_audioBuffer.size())
        ate = (unsigned)g_audioBuffer.size();

    for(unsigned a = 0; a < ate; ++a)
        stream[a] = g_audioBuffer[a];

    applyGain(stream, len);

    g_audioBuffer.erase(g_audioBuffer.begin(), g_audioBuffer.begin() + ate);
    g_audioBuffer_lock.Unlock();
    audio_unlock();
}


int runAudioLoop(ADL_MIDIPlayer *myDevice, AudioOutputSpec &spec)
{
    //const unsigned MaxSamplesAtTime = 512; // 512=dbopl limitation
    // How long is SDL buffer, in seconds?
    // The smaller the value, the more often SDL_AudioCallBack()
    // is called.
    //const double AudioBufferLength = 0.08;

    // How much do WE buffer, in seconds? The smaller the value,
    // the more prone to sound chopping we are.
    const double OurHeadRoomLength = 0.1;
    // The lag between visual content and audio content equals
    // the sum of these two buffers.

    AudioOutputSpec obtained;

    // Set up Audio Output
    if(audio_init(&spec, &obtained, s_audioPlaybackCallback) < 0)
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

    size_t got;
    uint8_t buff[4096];
    bool isMono = obtained.channels == 1;
    size_t out_buffer_size = static_cast<size_t>(obtained.samples + (obtained.freq * (isMono ? g_audioFormat.sampleOffset >> 1 : g_audioFormat.sampleOffset)) * OurHeadRoomLength);

    audio_start();

    setCursorVisibility(false);

    s_timeCounter.clearLineR();

    while(!stop)
    {
        got = (size_t)adl_playFormat(myDevice, 1024,
                                      buff,
                                      buff + g_audioFormat.containerSize,
                                      &g_audioFormat) * g_audioFormat.containerSize;
        if(got <= 0)
            break;

        if(isMono)
            got = stereoToMono(buff, got);

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

        g_audioBuffer_lock.Lock();
#if defined(__GNUC__) && (__GNUC__ == 15) && (__GNUC_MINOR__ == 1)
        // Workaround for GCC 15.1.0 on faulty std::deque's resize() call when C++98 is set
        // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=120931
        for(size_t p = 0; p < got; ++p)
            g_audioBuffer.push_back(buff[p]);
#else
        size_t pos = g_audioBuffer.size();
        g_audioBuffer.resize(pos + got);
        for(size_t p = 0; p < got; ++p)
            g_audioBuffer[pos + p] = buff[p];
#endif
        g_audioBuffer_lock.Unlock();

        while(!stop && (g_audioBuffer.size() > out_buffer_size))
        {
            audio_delay(1);
        }

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
                    adl_selectSongNum(myDevice, s_devSetup.songNumLoad);
                    s_fprintf(stdout, "\rSwitching song to %d/%d...                               \r\n", s_devSetup.songNumLoad, songsCount);
                    flushout(stdout);
                    break;
                case 'D':
                    // code for arrow left
                    s_devSetup.songNumLoad--;
                    if(s_devSetup.songNumLoad < 0)
                        s_devSetup.songNumLoad = 0;
                    adl_selectSongNum(myDevice, s_devSetup.songNumLoad);
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
                    adl_setBank(myDevice, bankId);
                    s_fprintf(stdout, "\rSwitching bank to %d/%d...                               \r\n", bankId, adl_getBanksCount());
                    flushout(stdout);
                    break;
                case 'V':
                case 'v':
                    bankId--;
                    if(bankId < 0)
                        bankId = adl_getBanksCount() - 1;
                    adl_setBank(myDevice, bankId);
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
            adl_positionSeek(myDevice, seekTo);
        }
#       endif

#       ifdef DEBUG_SONG_CHANGE
        if(delayBeforeSongChange-- <= 0)
        {
            delayBeforeSongChange = rand() % 100;
            adl_selectSongNum(myDevice, rand() % 10);
        }
#       endif

#       ifdef DEBUG_SONG_CHANGE_BY_HOOK
        if(gotXmiTrigger)
        {
            gotXmiTrigger = false;
            adl_selectSongNum(myDevice, (rand() % 10) + 1);
        }
#       endif
    }

    setCursorVisibility(true);

    s_timeCounter.clearLine();

    audio_stop();
    audio_close();

    return 0;
}
