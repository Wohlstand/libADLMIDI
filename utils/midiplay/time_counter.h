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


#pragma once
#ifndef ADLMIDI_TIME_COUNTER_H
#define ADLMIDI_TIME_COUNTER_H

#include <stdint.h>
#include "misc.h" // IWYU pragma: keep

struct TimeCounter
{
    char posHMS[25];
    char totalHMS[25];
    char loopStartHMS[25];
    char loopEndHMS[25];
    char linebuff[81];
#ifdef HAS_S_GETTIME
    char realHMS[25];
#endif

    bool hasLoop;
    uint64_t milliseconds_prev;
    int printsCounter;
    int printsCounterPeriod;
    int complete_prev;
    double totalTime;

#ifdef HAS_S_GETTIME
    double realTimeStart;
#endif

    TimeCounter();

    void setTotal(double total);

    void setLoop(double loopStart, double loopEnd);

#ifdef ADLMIDI_ENABLE_HW_DOS
    void waitDosTimerTick();
    void delay(int ticks);
#endif

    void initLineBuff();

    void clearLineR();

    void printTime(double pos);

    void printProgress(double pos);

    void clearLine();

};

extern TimeCounter s_timeCounter;


#endif /* ADLMIDI_TIME_COUNTER_H */
