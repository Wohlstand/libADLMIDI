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
#include "playback.h"
#include "../dev_setup.h"
#include "../misc.h"
#include "../time_counter.h"
#include "dos_tman.h"

int s_curSong = 0;
static double s_tempo = 1.0;

static double s_extra_delay = 0.0;
static DosTaskman *s_taskman = NULL;
static bool s_pause = false;

static void s_midiLoop(DosTask *task)
{
    if(stop || s_pause)
        return;

    ADL_MIDIPlayer *player = reinterpret_cast<ADL_MIDIPlayer *>(DosTaskman::task_getData(task));
    const double mindelay = 1.0 / DosTaskman::task_getFreq(task);
    double tickDelay;

    if(DosTaskman::task_getCount(task) >= DosTaskman::task_getRate(task))
    {
        s_extra_delay += mindelay;
        return; // Skip this iteration
    }

    tickDelay = adl_tickEvents(player, mindelay + s_extra_delay, mindelay / 10.0);

    if(s_extra_delay > 0)
        s_extra_delay = 0;

    if(adl_atEnd(player) || tickDelay <= 0)
        stop = true;
}

static void nextSong(ADL_MIDIPlayer *myDevice)
{
    ++s_curSong;
    if(s_curSong >= adl_getSongsCount(myDevice))
        s_curSong = 0;

    adl_selectSongNum(myDevice, s_curSong);
}

static void prevSong(ADL_MIDIPlayer *myDevice)
{
    --s_curSong;
    if(s_curSong < 0)
        s_curSong = adl_getSongsCount(myDevice) - 1;

    adl_selectSongNum(myDevice, s_curSong);
}

static void dos_dpmi_tail() {}

void runDOSLoop(DosTaskman *taskMan, ADL_MIDIPlayer *myDevice)
{
    DosTask *midiTask;
    void (*c_lock_begin)(DosTask *task) = &s_midiLoop;
    void (*c_lock_end)() = &dos_dpmi_tail;
    adl_dpmi_lock_region((void*&)c_lock_begin, (void*&)c_lock_end);

    midiTask = taskMan->addTask(s_midiLoop, s_devSetup.clock_freq, 1, myDevice);
    s_taskman = taskMan;

    taskMan->dispatch();

    if(s_devSetup.songNumLoad >= 0)
        s_curSong = s_devSetup.songNumLoad;

    s_timeCounter.clearLineR();

    setCursorVisibility(false);

    while(!stop)
    {
#   ifndef DEBUG_TRACE_ALL_EVENTS
        s_timeCounter.printTime(adl_positionTell(myDevice));
#   endif

        if(kbhit())
        {   // Quit on ESC key!
            int c = getch();
            switch(c)
            {
            case 27:
                stop = true;
                break;
            case 'p':
            case 'P':
            {
                if(adl_getSongsCount(myDevice) <= 1)
                    break; // Nothing to do
                s_pause = true;
                prevSong(myDevice);
                s_timeCounter.clearLineR();
                s_fprintf(stdout, " - Selecting song %d / %d\n", s_curSong + 1, adl_getSongsCount(myDevice));
                flushout(stdout);
                s_pause = false;
                break;
            }
            case 'n':
            case 'N':
            {
                if(adl_getSongsCount(myDevice) <= 1)
                    break; // Nothing to do
                s_pause = true;
                nextSong(myDevice);
                s_timeCounter.clearLineR();
                s_fprintf(stdout, " - Selecting song %d / %d\n", s_curSong + 1, adl_getSongsCount(myDevice));
                flushout(stdout);
                s_pause = false;
                break;
            }
            case 'r':
            case 'R':
            {
                s_pause = true;
                adl_panic(myDevice);
                adl_positionRewind(myDevice);
                s_timeCounter.clearLineR();
                s_fprintf(stdout, " - Rewind song to begin...\n");
                flushout(stdout);
                s_pause = false;
                break;
            }
            case '+':
            {
                s_pause = true;
                if(s_tempo < 8.0)
                    s_tempo += 0.1;
                adl_setTempo(myDevice, s_tempo);
                s_pause = false;
                break;
            }
            case '-':
            {
                s_pause = true;
                if(s_tempo > 0.1)
                    s_tempo -= 0.1;
                adl_setTempo(myDevice, s_tempo);
                s_pause = false;
                break;
            }
            case '*':
            {
                s_pause = true;
                s_tempo = 1.0;
                adl_setTempo(myDevice, s_tempo);
                s_pause = false;
                break;
            }
            }

            if(c == 27)
                stop = true;
        }

        s_timeCounter.waitDosTimerTick();
    }

    setCursorVisibility(true);

    s_timeCounter.clearLine();

    adl_panic(myDevice); //Shut up all sustaining notes

    taskMan->terminate(midiTask);
    midiTask = NULL;
    adl_dpmi_unlock_region((void*&)c_lock_begin, (void*&)c_lock_end);
}
