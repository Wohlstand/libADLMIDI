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

#include "playback.h"
#include "../misc.h"
#include "../time_counter.h"


void runHWSerialLoop(ADL_MIDIPlayer *myDevice)
{
    double tick_delay = 0.00000001;
    double tick_wait = 0.0;
    double timeBegL, timeEndL;
#if _WIN32
    const double minDelay = 0.050; // On Windows, the Serial bandwith is WAY SLOWER, so, bigger granuality.
#else
    const double minDelay = 0.005;
#endif
    double eat_delay;
    // bool tickSkip = true;

    setCursorVisibility(false);

    s_timeCounter.clearLineR();

    while(!stop)
    {
        timeBegL = s_getTime();
        tick_delay = adl_tickEvents(myDevice, tick_delay < minDelay ? tick_delay : minDelay, minDelay / 10.0);
        // adl_tickIterators(myDevice, minDelay);
#   ifndef DEBUG_TRACE_ALL_EVENTS
        s_timeCounter.printTime(adl_positionTell(myDevice));
#   endif
        timeEndL = s_getTime();

        eat_delay = timeEndL - timeBegL;

        if(tick_delay < minDelay)
            tick_wait = tick_delay - eat_delay;
        else
            tick_wait = minDelay - eat_delay;

        if(adl_atEnd(myDevice) && tick_delay <= 0)
            stop = 1;

        if(tick_wait > 0.0)
            s_sleepU(tick_wait);

#if 0
        timeBegL = s_getTime();

        tick_delay = adl_tickEventsOnly(myDevice, tick_delay, 0.000000001);
        adl_tickIterators(myDevice, tick_delay < minDelay ? tick_delay : minDelay);

        if(adl_atEnd(myDevice) && tick_delay <= 0)
            stop = true;

#   ifndef DEBUG_TRACE_ALL_EVENTS
        s_timeCounter.printTime(adl_positionTell(myDevice));
#   endif

        timeEndL = s_getTime();

        if(timeEndL < timeBegL)
            timeEndL = timeBegL;

        eat_delay = timeEndL - timeBegL;
        tick_wait += tick_delay - eat_delay;

        if(tick_wait > 0.0)
        {
            if(tick_wait < minDelay)
            {
                if(tick_wait > 0.0)
                    s_sleepU(tick_wait);
                tick_wait -= minDelay;
            }

            while(tick_wait > 0.0)
            {
                timeBegL = s_getTime();
                if(!tickSkip)
                    adl_tickIterators(myDevice, minDelay);
                else
                    tickSkip = false;
                timeEndL = s_getTime();

                if(timeEndL < timeBegL)
                    timeEndL = timeBegL;

                double microDelay = minDelay - (timeEndL - timeBegL);

                if(microDelay > 0.0)
                    s_sleepU(microDelay);

                tick_wait -= minDelay;
            }
        }
#endif
    }

    setCursorVisibility(true);

    s_timeCounter.clearLine();

    adl_panic(myDevice); //Shut up all sustaining notes
}
