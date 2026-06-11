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

#include <stddef.h>
#include <cmath>
#include <cstring>
#include <cstdio>
#include "time_counter.h"

TimeCounter s_timeCounter;


static inline void secondsToHMSM(double seconds_full, char *hmsm_buffer, size_t hmsm_buffer_size)
{
    double seconds_integral;
    double seconds_fractional = std::modf(seconds_full, &seconds_integral);
    unsigned int milliseconds = static_cast<unsigned int>(seconds_fractional * 1000.0);
    unsigned int seconds = static_cast<unsigned int>(std::fmod(seconds_full, 60.0));
    unsigned int minutes = static_cast<unsigned int>(std::fmod(seconds_full / 60, 60.0));
    unsigned int hours   = static_cast<unsigned int>(seconds_full / 3600);
    std::memset(hmsm_buffer, 0, hmsm_buffer_size);
    if (hours > 0)
        snprintf(hmsm_buffer, hmsm_buffer_size, "%02u:%02u:%02u,%03u", hours, minutes, seconds, milliseconds);
    else
        snprintf(hmsm_buffer, hmsm_buffer_size, "%02u:%02u,%03u", minutes, seconds, milliseconds);
}


TimeCounter::TimeCounter()
{
    hasLoop = false;
    totalTime = 0.0;
    milliseconds_prev = ~0u;
    printsCounter = 0;
    complete_prev = -1;
    printsCounterPeriod = 1;
}

void TimeCounter::setTotal(double total)
{
    totalTime = total;
    secondsToHMSM(total, totalHMS, 25);
#ifdef HAS_S_GETTIME
    realTimeStart = s_getTime();
    secondsToHMSM(s_getTime() - realTimeStart, realHMS, 25);
#endif
}

void TimeCounter::setLoop(double loopStart, double loopEnd)
{
    hasLoop = false;

    if(loopStart >= 0.0 && loopEnd >= 0.0)
    {
        secondsToHMSM(loopStart, loopStartHMS, 25);
        secondsToHMSM(loopEnd, loopEndHMS, 25);
        hasLoop = true;
    }
}

#ifdef ADLMIDI_ENABLE_HW_DOS
void TimeCounter::waitDosTimerTick()
{
    volatile unsigned long timer = DosTaskman::getCurTicks();
    while(timer == DosTaskman::getCurTicks());
}

void TimeCounter::delay(int ticks)
{
    volatile unsigned long timer = DosTaskman::getCurTicks() + ticks;
    while(timer >= DosTaskman::getCurTicks());
}
#endif

void TimeCounter::initLineBuff()
{
    std::memset(linebuff, ' ', sizeof(linebuff));
    linebuff[80] = '\0';
    linebuff[79] = '\r';
}

void TimeCounter::clearLineR()
{
    s_fprintf(stdout,  "                                                                              \r");
    flushout(stdout);
}

void TimeCounter::printTime(double pos)
{
    uint64_t milliseconds = static_cast<uint64_t>(pos * 1000.0);
    initLineBuff();
    int len;

    if(milliseconds != milliseconds_prev)
    {
        if(printsCounter >= printsCounterPeriod)
        {
            printsCounter = -1;
            secondsToHMSM(pos, posHMS, 25);
#ifdef HAS_S_GETTIME
            secondsToHMSM(s_getTime() - realTimeStart, realHMS, 25);
#endif \
            // s_fprintf(stdout, "                                               \r");
#ifdef HAS_S_GETTIME
            len = snprintf(linebuff, 79, "Time position: %s / %s [Real time: %s]", posHMS, totalHMS, realHMS);
#else
            len = snprintf(linebuff, 79, "Time position: %s / %s", posHMS, totalHMS);
#endif
            if(len > 0)
                memset(linebuff + len, ' ',  79 - len);
            linebuff[79] = '\r';
            s_fprintf(stdout, "%s", linebuff);
            flushout(stdout);
            milliseconds_prev = milliseconds;
        }

        printsCounter++;
    }
}

void TimeCounter::printProgress(double pos)
{
    int complete = static_cast<int>(std::floor(100.0 * pos / totalTime));
    int len;

    if(complete_prev != complete)
    {
        len = snprintf(linebuff, 79, "Recording WAV... [%d%% completed]", complete);

        if(len > 0)
            memset(linebuff + len, ' ',  79 - len);
        linebuff[79] = '\r';
        s_fprintf(stdout, "%s", linebuff);
        flushout(stdout);
        complete_prev = complete;
    }
}

void TimeCounter::clearLine()
{
    s_fprintf(stdout, "                                                                              \n\n");
    flushout(stdout);
}
