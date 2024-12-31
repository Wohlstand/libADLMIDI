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

#include <string>
#include <cstdio>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <deque>
#include <vector>
#include <algorithm>
#include <signal.h>
#include <stdint.h>
#include "utf8main.h"

#if defined(ADLMIDI_ENABLE_HW_SERIAL) && !defined(OUTPUT_WAVE_ONLY)
#   ifdef ADLMIDI_USE_SDL2
#   include <SDL2/SDL_timer.h>
#ifdef __APPLE__
#   include <unistd.h>
#endif

#   define HAS_S_GETTIME
static inline double s_getTime()
{
    return SDL_GetTicks64() / 1000.0;
}

static inline void s_sleepU(double s)
{
#ifdef __APPLE__
    // For unknown reasons, any sleep functions to way WAY LONGER than requested
    // So, implementing an own one.
    static double debt = 0.0;
    double target = s_getTime() + s - debt;

    while(s_getTime() < target)
        usleep(1000);

    debt = s_getTime() - target;
#else
    SDL_Delay((Uint32)(s * 1000));
#endif
}
#   else

#   include <time.h>
#   include <unistd.h>
#   include <assert.h>
static inline double s_getTime()
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (t.tv_nsec + (t.tv_sec * 1000000000)) / 1000000000.0;
}

static inline void s_sleepU(double s)
{
    static double debt = 0.0;
    double target = s_getTime() + s - debt;

    while(s_getTime() < target)
        usleep(1000);

    debt = s_getTime() - target;
}
#   endif
#endif

#ifdef DEBUG_SONG_SWITCHING
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>

struct termios orig_termios;

void reset_terminal_mode()
{
    tcsetattr(0, TCSANOW, &orig_termios);
}

void set_conio_terminal_mode()
{
    struct termios new_termios;

    /* take two copies - one for now, one for later */
    tcgetattr(0, &orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));

    /* register cleanup handler, and set the new terminal mode */
    atexit(reset_terminal_mode);
    cfmakeraw(&new_termios);
    tcsetattr(0, TCSANOW, &new_termios);
}

int kbhit()
{
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv) > 0;
}

int getch()
{
    int r;
    unsigned char c;
    if ((r = read(0, &c, sizeof(c))) < 0)
        return r;
    else
        return c;
}
#endif

#if defined(_MSC_VER) && _MSC_VER < 1900

#define snprintf c99_snprintf
#define vsnprintf c99_vsnprintf

__inline int c99_vsnprintf(char *outBuf, size_t size, const char *format, va_list ap)
{
    int count = -1;

    if (size != 0)
        count = _vsnprintf_s(outBuf, size, _TRUNCATE, format, ap);
    if (count == -1)
        count = _vscprintf(format, ap);

    return count;
}

__inline int c99_snprintf(char *outBuf, size_t size, const char *format, ...)
{
    int count;
    va_list ap;

    va_start(ap, format);
    count = c99_vsnprintf(outBuf, size, format, ap);
    va_end(ap);

    return count;
}
#endif

#if defined(__WATCOMC__)
#include <stdio.h> // snprintf is here!
#define flushout(stream)
#else
#define flushout(stream) std::fflush(stream)
#endif

#if defined(__DJGPP__) || (defined(__WATCOMC__) && (defined(__DOS__) || defined(__DOS4G__) || defined(__DOS4GNZ__)))
#define HW_OPL_MSDOS
#include <conio.h>
#include <dos.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef __DJGPP__
#include <pc.h>
#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>
#include <sys/exceptn.h>
#define BIOStimer _farpeekl(_dos_ds, 0x46C)
#endif//__DJGPP__

#ifdef __WATCOMC__
//#include <wdpmi.h>
#include <i86.h>
#include <bios.h>
#include <time.h>

unsigned long biostime(unsigned cmd, unsigned long lon)
{
    long tval = (long)lon;
    _bios_timeofday(cmd, &tval);
    return (unsigned long)tval;
}

#define BIOStimer   biostime(0, 0l)
#define BIOSTICK    55                  /* biostime() increases one tick about
                                   every 55 msec */

void mch_delay(int32_t msec)
{
    /*
     * We busy-wait here.  Unfortunately, delay() and usleep() have been
     * reported to give problems with the original Windows 95.  This is
     * fixed in service pack 1, but not everybody installed that.
     */
    long starttime = biostime(0, 0L);
    while(biostime(0, 0L) < starttime + msec / BIOSTICK);
}

//#define BIOStimer _farpeekl(_dos_ds, 0x46C)
//#define DOSMEM(s,o,t) *(t _far *)(SS_DOSMEM + (DWORD)((o)|(s)<<4))
//#define BIOStimer DOSMEM(0x46, 0x6C, WORD);
//#define VSYNC_STATUS    Ox3da
//#define VSYNC_MASK      Ox08
/* #define SYNC { while(inp(VSYNCSTATUS) & VSYNCMASK);\
      while (!(inp(VSYNCSTATUS) & VSYNCMASK)); } */
#endif//__WATCOMC__

#endif

#include <adlmidi.h>

#ifndef HARDWARE_OPL3

#ifndef OUTPUT_WAVE_ONLY
#   include "audio.h"
#endif

#include "wave_writer.h"

#   ifndef OUTPUT_WAVE_ONLY
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
static ADLMIDI_AudioFormat g_audioFormat;
static float g_gaining = 2.0f;

static void applyGain(uint8_t *buffer, size_t bufferSize)
{
    size_t i;

    switch(g_audioFormat.type)
    {
    case ADLMIDI_SampleType_S8:
    {
        int8_t *buf = reinterpret_cast<int8_t *>(buffer);
        size_t samples = bufferSize;
        for(i = 0; i < samples; ++i)
            *(buf++) *= g_gaining;
        break;
    }
    case ADLMIDI_SampleType_U8:
    {
        uint8_t *buf = buffer;
        size_t samples = bufferSize;
        for(i = 0; i < samples; ++i)
        {
            int8_t s = static_cast<int8_t>(static_cast<int32_t>(*buf) + (-0x7f - 1)) * g_gaining;
            *(buf++) = static_cast<uint8_t>(static_cast<int32_t>(s) - (-0x7f - 1));
        }
        break;
    }
    case ADLMIDI_SampleType_S16:
    {
        int16_t *buf = reinterpret_cast<int16_t *>(buffer);
        size_t samples = bufferSize / g_audioFormat.containerSize;
        for(i = 0; i < samples; ++i)
            *(buf++) *= g_gaining;
        break;
    }
    case ADLMIDI_SampleType_U16:
    {
        uint16_t *buf = reinterpret_cast<uint16_t *>(buffer);
        size_t samples = bufferSize / g_audioFormat.containerSize;
        for(i = 0; i < samples; ++i)
        {
            int16_t s = static_cast<int16_t>(static_cast<int32_t>(*buf) + (-0x7fff - 1)) * g_gaining;
            *(buf++) = static_cast<uint16_t>(static_cast<int32_t>(s) - (-0x7fff - 1));
        }
        break;
    }
    case ADLMIDI_SampleType_S32:
    {
        int32_t *buf = reinterpret_cast<int32_t *>(buffer);
        size_t samples = bufferSize / g_audioFormat.containerSize;
        for(i = 0; i < samples; ++i)
            *(buf++) *= g_gaining;
        break;
    }
    case ADLMIDI_SampleType_F32:
    {
        float *buf = reinterpret_cast<float *>(buffer);
        size_t samples = bufferSize / g_audioFormat.containerSize;
        for(i = 0; i < samples; ++i)
            *(buf++) *= g_gaining;
        break;
    }
    default:
        break;
    }
}

static void SDL_AudioCallbackX(void *, uint8_t *stream, int len)
{
    unsigned ate = static_cast<unsigned>(len); // number of bytes

    audio_lock();
    //short *target = (short *) stream;
    g_audioBuffer_lock.Lock();    

    if(ate > g_audioBuffer.size())
        ate = (unsigned)g_audioBuffer.size();

    for(unsigned a = 0; a < ate; ++a)
        stream[a] = g_audioBuffer[a];

    applyGain(stream, len);

    g_audioBuffer.erase(g_audioBuffer.begin(), g_audioBuffer.begin() + ate);
    g_audioBuffer_lock.Unlock();
    audio_unlock();
}
#   endif//OUTPUT_WAVE_ONLY

const char* audio_format_to_str(int format, int is_msb)
{
    switch(format)
    {
    case ADLMIDI_SampleType_S8:
        return "S8";
    case ADLMIDI_SampleType_U8:
        return "U8";
    case ADLMIDI_SampleType_S16:
        return is_msb ? "S16MSB" : "S16";
    case ADLMIDI_SampleType_U16:
        return is_msb ? "U16MSB" : "U16";
    case ADLMIDI_SampleType_S32:
        return is_msb ? "S32MSB" : "S32";
    case ADLMIDI_SampleType_F32:
        return is_msb ? "F32MSB" : "F32";
    }
    return "UNK";
}

#endif //HARDWARE_OPL3

const char* volume_model_to_str(int vm)
{
    switch(vm)
    {
    default:
    case ADLMIDI_VolumeModel_Generic:
        return "Generic";
    case ADLMIDI_VolumeModel_NativeOPL3:
        return "Native OPL3";
    case ADLMIDI_VolumeModel_DMX:
        return "DMX";
    case ADLMIDI_VolumeModel_APOGEE:
        return "Apogee Sound System";
    case ADLMIDI_VolumeModel_9X:
        return "9X (SB16)";
    case ADLMIDI_VolumeModel_DMX_Fixed:
        return "DMX (fixed AM voices)";
    case ADLMIDI_VolumeModel_APOGEE_Fixed:
        return "Apogee Sound System (fixed AM voices)";
    case ADLMIDI_VolumeModel_AIL:
        return "Audio Interface Library (AIL)";
    case ADLMIDI_VolumeModel_9X_GENERIC_FM:
        return "9X (Generic FM)";
    case ADLMIDI_VolumeModel_HMI:
        return "HMI Sound Operating System";
    case ADLMIDI_VolumeModel_HMI_OLD:
        return "HMI Sound Operating System (Old)";
    }
}

const char* chanalloc_to_str(int vm)
{
    switch(vm)
    {
    default:
    case ADLMIDI_ChanAlloc_AUTO:
        return "<auto>";

    case ADLMIDI_ChanAlloc_OffDelay:
        return "Off Delay";

    case ADLMIDI_ChanAlloc_SameInst:
        return "Same instrument";

    case ADLMIDI_ChanAlloc_AnyReleased:
        return "Any released";
    }
}


static bool is_number(const std::string &s)
{
    std::string::const_iterator it = s.begin();
    while(it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

static void printError(const char *err, const char *what = NULL)
{
    if(what)
        std::fprintf(stderr, "\nERROR (%s): %s\n\n", what, err);
    else
        std::fprintf(stderr, "\nERROR: %s\n\n", err);
    flushout(stderr);
}

static int stop = 0;
#ifndef HARDWARE_OPL3
static void sighandler(int dum)
{
    switch(dum)
    {
    case SIGINT:
    case SIGTERM:
#ifndef _WIN32
    case SIGHUP:
#endif
        stop = 1;
        break;
    default:
        break;
    }
}
#endif

//#define DEBUG_SONG_CHANGE
//#define DEBUG_SONG_CHANGE_BY_HOOK

#ifdef DEBUG_SONG_CHANGE_BY_HOOK
static bool gotXmiTrigger = false;

static void xmiTriggerCallback(void *, unsigned trigger, size_t track)
{
    std::fprintf(stdout, " - Trigger hook: trigger %u, track: %u\n", trigger, (unsigned)track);
    flushout(stdout);
    gotXmiTrigger = true;
}

static void loopEndCallback(void *)
{
    std::fprintf(stdout, " - Loop End hook: trigger\n");
    flushout(stdout);
    gotXmiTrigger = true;
}
#endif

static void debugPrint(void * /*userdata*/, const char *fmt, ...)
{
    char buffer[4096];
    std::va_list args;
    va_start(args, fmt);
    int rc = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    if(rc > 0)
    {
        std::fprintf(stdout, " - Debug: %s\n", buffer);
        flushout(stdout);
    }
}

#ifdef HARDWARE_OPL3
static inline void keyWait()
{
    std::printf("\n<press any key to continue...>");
    getch();
    std::printf("\r                              \n");
}
#endif

static void printBanks()
{
    // Get count of embedded banks (no initialization needed)
    int banksCount = adl_getBanksCount();
    //Get pointer to list of embedded bank names
    const char *const *banknames = adl_getBankNames();

    if(banksCount > 0)
    {
        std::printf("    Available embedded banks by number:\n\n");

        for(int a = 0; a < banksCount; ++a)
        {
            std::printf("%10s%2u = %s\n", a ? "" : "Banks:", a, banknames[a]);
#ifdef HARDWARE_OPL3
            if(((a - 15) % 23 == 0 && a != 0))
                keyWait();
#endif
        }

        std::printf(
            "\n"
            "     Use banks 2-5 to play Descent \"q\" soundtracks.\n"
            "     Look up the relevant bank number from descent.sng.\n"
            "\n"
            "     The fourth parameter can be used to specify the number\n"
            "     of four-op channels to use. Each four-op channel eats\n"
            "     the room of two regular channels. Use as many as required.\n"
            "     The Doom & Hexen sets require one or two, while\n"
            "     Miles four-op set requires the maximum of numcards*6.\n"
            "\n"
        );
    }
    else
    {
        std::printf("    This build of libADLMIDI has no embedded banks!\n\n");
    }

    flushout(stdout);
}


#ifdef DEBUG_TRACE_ALL_EVENTS
static void debugPrintEvent(void * /*userdata*/, ADL_UInt8 type, ADL_UInt8 subtype, ADL_UInt8 channel, const ADL_UInt8 * /*data*/, size_t len)
{
    std::fprintf(stdout, " - E: 0x%02X 0x%02X %02d (%d)\r\n", type, subtype, channel, (int)len);
    flushout(stdout);
}
#endif

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


static struct TimeCounter
{
    char posHMS[25];
    char totalHMS[25];
    char loopStartHMS[25];
    char loopEndHMS[25];
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

#ifdef HARDWARE_OPL3
    unsigned newTimerFreq;
    unsigned timerPeriod;
    int haveYield;
    int haveDosIdle;
    unsigned int ring;
    unsigned long BIOStimer_begin;

    unsigned long timerNext;

    enum wmethod
    {
        WM_NONE,
        WM_YIELD,
        WM_IDLE,
        WM_HLT
    } idleMethod;

#endif

    TimeCounter()
    {
        hasLoop = false;
        totalTime = 0.0;
        milliseconds_prev = ~0u;
        printsCounter = 0;
        complete_prev = -1;

#ifndef HARDWARE_OPL3
        printsCounterPeriod = 1;
#else
        printsCounterPeriod = 20;
        setDosTimerHZ(209);
        haveYield = 0;
        haveDosIdle = 0;
        ring = 0;
        idleMethod = WM_NONE;

        timerNext = 0;
#endif
    }

#ifdef HARDWARE_OPL3
    void initDosTimer()
    {
#   ifdef __DJGPP__
        /* determine protection ring */
        __asm__ ("mov %%cs, %0\n\t"
                 "and $3, %0" : "=r" (ring));

        errno = 0;
        __dpmi_yield();
        haveYield = errno ? 0 : 1;

        if(!haveYield)
        {
            __dpmi_regs regs;
            regs.x.ax = 0x1680;
            __dpmi_int(0x28, &regs);
            haveDosIdle = regs.h.al ? 0 : 1;

            if(haveDosIdle)
                idleMethod = WM_IDLE;
            else if(ring == 0)
                idleMethod = WM_HLT;
            else
                idleMethod = WM_NONE;
        }
        else
        {
            idleMethod = WM_YIELD;
        }

        const char *method;
        switch(idleMethod)
        {
        default:
        case WM_NONE:
            method = "none";
            break;
        case WM_YIELD:
            method = "yield";
            break;
        case WM_IDLE:
            method = "idle";
            break;
        case WM_HLT:
            method = "hlt";
            break;
        }

        std::fprintf(stdout, " - [DOS] Using idle method: %s\n", method);
#   endif
    }

    void setDosTimerHZ(unsigned timer)
    {
        newTimerFreq = timer;
        timerPeriod = 0x1234DDul / newTimerFreq;
    }

    void flushDosTimer()
    {
#   ifdef __DJGPP__
        outportb(0x43, 0x34);
        outportb(0x40, timerPeriod & 0xFF);
        outportb(0x40, timerPeriod >>   8);
#   endif

#   ifdef __WATCOMC__
        outp(0x43, 0x34);
        outp(0x40, TimerPeriod & 0xFF);
        outp(0x40, TimerPeriod >>   8);
#   endif

        BIOStimer_begin = BIOStimer;

        std::fprintf(stdout, " - [DOS] Running clock with %d hz\n", newTimerFreq);
    }

    void restoreDosTimer()
    {
#   ifdef __DJGPP__
        // Fix the skewed clock and reset BIOS tick rate
        _farpokel(_dos_ds, 0x46C, BIOStimer_begin + (BIOStimer - BIOStimer_begin) * (0x1234DD / 65536.0) / newTimerFreq);

        //disable();
        outportb(0x43, 0x34);
        outportb(0x40, 0);
        outportb(0x40, 0);
        //enable();
#   endif

#   ifdef __WATCOMC__
        outp(0x43, 0x34);
        outp(0x40, 0);
        outp(0x40, 0);
#   endif
    }

    void waitDosTimer()
    {
//__asm__ volatile("sti\nhlt");
//usleep(10000);
#       ifdef __DJGPP__
        switch(idleMethod)
        {
        default:
        case WM_NONE:
            if(timerNext != 0)
                while(BIOStimer < timerNext);
            timerNext = BIOStimer + 1;
            break;

        case WM_YIELD:
            __dpmi_yield();
            break;

        case WM_IDLE:
        {
            __dpmi_regs regs;

            /* the DOS Idle call is documented to return immediately if no other
             * program is ready to run, therefore do one HLT if we can */
            if(ring == 0)
                __asm__ volatile ("hlt");

            regs.x.ax = 0x1680;
            __dpmi_int(0x28, &regs);
            if (regs.h.al)
                errno = ENOSYS;
            break;
        }

        case WM_HLT:
            __asm__ volatile("hlt");
            break;
        }
#       endif
#       ifdef __WATCOMC__
        //dpmi_dos_yield();
        mch_delay((unsigned int)(tick_delay * 1000.0));
#       endif
    }
#endif

    void setTotal(double total)
    {
        totalTime = total;
        secondsToHMSM(total, totalHMS, 25);
#ifdef HAS_S_GETTIME
        realTimeStart = s_getTime();
        secondsToHMSM(s_getTime() - realTimeStart, realHMS, 25);
#endif
    }

    void setLoop(double loopStart, double loopEnd)
    {
        hasLoop = false;

        if(loopStart >= 0.0 && loopEnd >= 0.0)
        {
            secondsToHMSM(loopStart, loopStartHMS, 25);
            secondsToHMSM(loopEnd, loopEndHMS, 25);
            hasLoop = true;
        }
    }

    void clearLineR()
    {
        std::fprintf(stdout, "                                               \r");
        flushout(stdout);
    }

    void printTime(double pos)
    {
        uint64_t milliseconds = static_cast<uint64_t>(pos * 1000.0);

        if(milliseconds != milliseconds_prev)
        {
            if(printsCounter >= printsCounterPeriod)
            {
                printsCounter = -1;
                secondsToHMSM(pos, posHMS, 25);
#ifdef HAS_S_GETTIME
                secondsToHMSM(s_getTime() - realTimeStart, realHMS, 25);
#endif
                std::fprintf(stdout, "                                               \r");
#ifdef HAS_S_GETTIME
                std::fprintf(stdout, "Time position: %s / %s [Real time: %s]\r", posHMS, totalHMS, realHMS);
#else
                std::fprintf(stdout, "Time position: %s / %s\r", posHMS, totalHMS);
#endif
                flushout(stdout);
                milliseconds_prev = milliseconds;
            }
            printsCounter++;
        }
    }

    void printProgress(double pos)
    {
        int complete = static_cast<int>(std::floor(100.0 * pos / totalTime));

        if(complete_prev != complete)
        {
            std::fprintf(stdout, "                                               \r");
            std::fprintf(stdout, "Recording WAV... [%d%% completed]\r", complete);
            flushout(stdout);
            complete_prev = complete;
        }
    }

    void clearLine()
    {
        std::fprintf(stdout, "                                               \n\n");
        flushout(stdout);
    }

} s_timeCounter;



#if defined(ADLMIDI_ENABLE_HW_SERIAL) && !defined(OUTPUT_WAVE_ONLY)
static void runHWSerialLoop(ADL_MIDIPlayer *myDevice)
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
            stop = true;

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

    s_timeCounter.clearLine();

    adl_panic(myDevice); //Shut up all sustaining notes
}
#endif // ADLMIDI_ENABLE_HW_SERIAL


#ifndef HARDWARE_OPL3
#   ifndef OUTPUT_WAVE_ONLY
static int runAudioLoop(ADL_MIDIPlayer *myDevice, AudioOutputSpec &spec)
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

    // Set up SDL
    if(audio_init(&spec, &obtained, SDL_AudioCallbackX) < 0)
    {
        std::fprintf(stderr, "\nERROR: Couldn't open audio: %s\n\n", audio_get_error());
        return 1;
    }

    if(spec.samples != obtained.samples)
    {
        std::fprintf(stderr, " - Audio wanted (format=%s,samples=%u,rate=%u,channels=%u);\n"
                             " - Audio obtained (format=%s,samples=%u,rate=%u,channels=%u)\n",
                     audio_format_to_str(spec.format, spec.is_msb),         spec.samples,     spec.freq,     spec.channels,
                     audio_format_to_str(obtained.format, obtained.is_msb), obtained.samples, obtained.freq, obtained.channels);
    }

    switch(obtained.format)
    {
    case ADLMIDI_SampleType_S8:
        g_audioFormat.type = ADLMIDI_SampleType_S8;
        g_audioFormat.containerSize = sizeof(int8_t);
        g_audioFormat.sampleOffset = sizeof(int8_t) * 2;
        break;
    case ADLMIDI_SampleType_U8:
        g_audioFormat.type = ADLMIDI_SampleType_U8;
        g_audioFormat.containerSize = sizeof(uint8_t);
        g_audioFormat.sampleOffset = sizeof(uint8_t) * 2;
        break;
    case ADLMIDI_SampleType_S16:
        g_audioFormat.type = ADLMIDI_SampleType_S16;
        g_audioFormat.containerSize = sizeof(int16_t);
        g_audioFormat.sampleOffset = sizeof(int16_t) * 2;
        break;
    case ADLMIDI_SampleType_U16:
        g_audioFormat.type = ADLMIDI_SampleType_U16;
        g_audioFormat.containerSize = sizeof(uint16_t);
        g_audioFormat.sampleOffset = sizeof(uint16_t) * 2;
        break;
    case ADLMIDI_SampleType_S32:
        g_audioFormat.type = ADLMIDI_SampleType_S32;
        g_audioFormat.containerSize = sizeof(int32_t);
        g_audioFormat.sampleOffset = sizeof(int32_t) * 2;
        break;
    case ADLMIDI_SampleType_F32:
        g_audioFormat.type = ADLMIDI_SampleType_F32;
        g_audioFormat.containerSize = sizeof(float);
        g_audioFormat.sampleOffset = sizeof(float) * 2;
        break;
    }


#   ifdef DEBUG_SONG_CHANGE
    int delayBeforeSongChange = 50;
    std::fprintf(stdout, "DEBUG: === Random song set test is active! ===\n");
    flushout(stdout);
#   endif

#   ifdef DEBUG_SEEKING_TEST
    int delayBeforeSeek = 50;
    std::fprintf(stdout, "DEBUG: === Random position set test is active! ===\n");
    flushout(stdout);
#   endif

    size_t got;
    uint8_t buff[16384];

    audio_start();

    s_timeCounter.clearLineR();

    while(!stop)
    {
        got = (size_t)adl_playFormat(myDevice, 4096,
                                     buff,
                                     buff + g_audioFormat.containerSize,
                                     &g_audioFormat) * g_audioFormat.containerSize;
        if(got <= 0)
            break;

#   ifdef DEBUG_TRACE_ALL_CHANNELS
        enum { TerminalColumns = 80 };
        char channelText[TerminalColumns + 1];
        char channelAttr[TerminalColumns + 1];
        adl_describeChannels(myDevice, channelText, channelAttr, sizeof(channelText));
        std::fprintf(stdout, "%*s\r", TerminalColumns, "");  // erase the line
        std::fprintf(stdout, "%s\n", channelText);
#   endif

#   ifndef DEBUG_TRACE_ALL_EVENTS
        s_timeCounter.printTime(adl_positionTell(myDevice));
#   endif

        g_audioBuffer_lock.Lock();
        size_t pos = g_audioBuffer.size();
        g_audioBuffer.resize(pos + got);
        for(size_t p = 0; p < got; ++p)
            g_audioBuffer[pos + p] = buff[p];
        g_audioBuffer_lock.Unlock();

        const AudioOutputSpec &spec = obtained;
        while(!stop && (g_audioBuffer.size() > static_cast<size_t>(spec.samples + (spec.freq * g_audioFormat.sampleOffset) * OurHeadRoomLength)))
        {
            audio_delay(1);
        }

#       ifdef DEBUG_SONG_SWITCHING
        if(kbhit())
        {
            int code = getch();
            if(code == '\033' && kbhit())
            {
                getch();
                switch(getch())
                {
                case 'C':
                    // code for arrow right
                    songNumLoad++;
                    if(songNumLoad >= songsCount)
                        songNumLoad = songsCount;
                    adl_selectSongNum(myDevice, songNumLoad);
                    std::fprintf(stdout, "\rSwitching song to %d/%d...                               \r\n", songNumLoad, songsCount);
                    flushout(stdout);
                    break;
                case 'D':
                    // code for arrow left
                    songNumLoad--;
                    if(songNumLoad < 0)
                        songNumLoad = 0;
                    adl_selectSongNum(myDevice, songNumLoad);
                    std::fprintf(stdout, "\rSwitching song to %d/%d...                               \r\n", songNumLoad, songsCount);
                    flushout(stdout);
                    break;
                }
            }
            else if(code == 27) // Quit by ESC key
                stop = 1;
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

    s_timeCounter.clearLine();

    audio_stop();
    audio_close();

    return 0;
}
#   endif // OUTPUT_WAVE_ONLY

static int runWaveOutLoopLoop(ADL_MIDIPlayer *myDevice, const std::string &musPath, unsigned sampleRate)
{
    std::string wave_out = musPath + ".wav";
    std::fprintf(stdout, " - Recording WAV file %s...\n", wave_out.c_str());
    std::fprintf(stdout, "\n==========================================\n");
    flushout(stdout);

    if(wave_open(static_cast<long>(sampleRate), wave_out.c_str()) == 0)
    {
        wave_enable_stereo();
        short buff[4096];

        while(!stop)
        {
            size_t got = static_cast<size_t>(adl_play(myDevice, 4096, buff));
            if(got <= 0)
                break;
            wave_write(buff, static_cast<long>(got));

            s_timeCounter.printProgress(adl_positionTell(myDevice));
        }

        wave_close();
        s_timeCounter.clearLine();

        if(stop)
            std::fprintf(stdout, "Interrupted! Recorded WAV is incomplete, but playable!\n");
        else
            std::fprintf(stdout, "Completed!\n");
        flushout(stdout);
    }
    else
    {
        adl_close(myDevice);
        return 1;
    }

    return 0;
}

#else // HARDWARE_OPL3
static void runDOSLoop(ADL_MIDIPlayer *myDevice)
{
    double tick_delay = 0.0;

    s_timeCounter.clearLineR();

    while(!stop)
    {
        const double mindelay = 1.0 / s_timeCounter.newTimerFreq;

#   ifndef DEBUG_TRACE_ALL_EVENTS
        s_timeCounter.printTime(adl_positionTell(myDevice));
#   endif

        s_timeCounter.waitDosTimer();

        static unsigned long PrevTimer = BIOStimer;
        const unsigned long CurTimer = BIOStimer;
        const double eat_delay = (CurTimer - PrevTimer) / (double)s_timeCounter.newTimerFreq;
        PrevTimer = CurTimer;

        tick_delay = adl_tickEvents(myDevice, eat_delay, mindelay);

        if(adl_atEnd(myDevice) && tick_delay <= 0)
            stop = true;

        if(kbhit())
        {   // Quit on ESC key!
            int c = getch();
            if(c == 27)
                stop = true;
        }
    }

    s_timeCounter.clearLine();

    adl_panic(myDevice); //Shut up all sustaining notes
}
#endif // HARDWARE_OPL3


int main(int argc, char **argv)
{
    std::fprintf(stdout, "==========================================\n"
                        #ifdef HARDWARE_OPL3
                         "      libADLMIDI demo utility (HW OPL)\n"
                        #else
                         "         libADLMIDI demo utility\n"
                        #endif
                         "==========================================\n\n");
    flushout(stdout);

    if(argc >= 2 && std::string(argv[1]) == "--list-banks")
    {
        printBanks();
        return 0;
    }

    if(argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")
    {
        std::printf(
            "Usage: adlmidi <midifilename> [ <options> ] \n"
            "                              [ <bank> [ <numchips> [ <numfourops>] ] ]\n"
            // " -p Enables adlib percussion instrument mode\n"
            " -t Enables tremolo amplification mode\n"
            " -v Enables vibrato amplification mode\n"
            " -s Enables scaling of modulator volumes\n"
            " -vm <num> Chooses one of volume models: \n"
            "    0 auto (default)\n"
            "    1 Generic\n"
            "    2 Native OPL3\n"
            "    3 DMX\n"
            "    4 Apogee Sound System\n"
            "    5 9x SB16\n"
            "    6 DMX (Fixed AM voices)\n"
            "    7 Apogee Sound System (Fixed AM voices)\n"
            "    8 Audio Interface Library (AIL)\n"
            "    9 9x Generic FM\n"
            "   10 HMI Sound Operating System\n"
        );
#ifdef HARDWARE_OPL3
        keyWait();
#endif
        std::printf(
            " -frb Enables full-ranged CC74 XG Brightness controller\n"
            " -nl Quit without looping\n"
            " -w Write WAV file rather than playing\n"
            " -mb  Run the test of multibank over embedded. 62, 14, 68, and 74'th banks\n"
            "      will be combined into one\n"
            " --solo <track>             Selects a solo track to play\n"
            " --only <track1,...,trackN> Selects a subset of tracks to play\n"
            " --song <song ID 0...N-1>   Selects a song to play (if XMI)\n"
            " -ea Enable the auto-arpeggio\n"
#ifndef HARDWARE_OPL3
            " -fp Enables full-panning stereo support\n"
            " --gain <value> Set the gaining factor (default 2.0)\n"
            " --emu-nuked  Uses Nuked OPL3 v 1.8 emulator\n"
            " --emu-nuked7 Uses Nuked OPL3 v 1.7.4 emulator\n"
            " --emu-dosbox Uses DosBox 0.74 OPL3 emulator\n"
            " --emu-opal   Uses Opal OPL3 emulator\n"
            " --emu-java   Uses Java OPL3 emulator\n"
#endif
#ifdef HARDWARE_OPL3
            "\n"
            " --time-freq <hz>  Uses a different time value, DEFAULT 209\n"
            " --list-banks  Print a lost of all built-in FM banks\n"
#endif
            "\n"
            "Where <bank> - number of embeeded bank or filepath to custom WOPL bank file\n"
            "\n"
            "Note: To create WOPL bank files use OPL Bank Editor you can get here: \n"
            "https://github.com/Wohlstand/OPL3BankEditor\n"
            "\n"
        );

#ifndef HARDWARE_OPL3
        printBanks();
#endif
        return 0;
    }

    unsigned int sampleRate = 44100;
#if !defined(HARDWARE_OPL3) && !defined(OUTPUT_WAVE_ONLY)
    //const unsigned MaxSamplesAtTime = 512; // 512=dbopl limitation
    // How long is SDL buffer, in seconds?
    // The smaller the value, the more often SDL_AudioCallBack()
    // is called.
    const double AudioBufferLength = 0.08;

    AudioOutputSpec spec;
    spec.freq     = sampleRate;
    spec.format   = ADLMIDI_SampleType_S16;
    spec.is_msb   = 0;
    spec.channels = 2;
    spec.samples  = uint16_t((double)spec.freq * AudioBufferLength);
#endif // !HARDWARE_OPL3 && !OUTPUT_WAVE_ONLY

#if defined(ADLMIDI_ENABLE_HW_SERIAL) && !defined(OUTPUT_WAVE_ONLY)
    bool hwSerial = false;
    std::string serialName;
    unsigned serialBaud = 115200;
    unsigned serialProto = ADLMIDI_SerialProtocol_RetroWaveOPL3;
#endif

    ADL_MIDIPlayer *myDevice;

    //Initialize libADLMIDI and create the instance (you can initialize multiple of them!)
    myDevice = adl_init(sampleRate);
    if(myDevice == NULL)
    {
        printError("Failed to init MIDI device!\n");
        return 1;
    }

    //Set internal debug messages hook to print all libADLMIDI's internal debug messages
    adl_setDebugMessageHook(myDevice, debugPrint, NULL);

    /*
     * Set library options by parsing of command line arguments
     */
    bool multibankFromEnbededTest = false;

#ifndef OUTPUT_WAVE_ONLY
    bool recordWave = false;
    int loopEnabled = 1;
#endif
    int autoArpeggioEnabled = 0;
    int chanAlloc = ADLMIDI_ChanAlloc_AUTO;

#ifndef HARDWARE_OPL3
    int emulator = ADLMIDI_EMU_NUKED;
#endif

    int volumeModel = ADLMIDI_VolumeModel_AUTO;
    size_t soloTrack = ~(size_t)0;
    int songNumLoad = -1;
    std::vector<size_t> onlyTracks;

#if !defined(HARDWARE_OPL3) && !defined(OUTPUT_WAVE_ONLY)
    g_audioFormat.type = ADLMIDI_SampleType_S16;
    g_audioFormat.containerSize = sizeof(int16_t);
    g_audioFormat.sampleOffset = sizeof(int16_t) * 2;
#endif

    while(argc > 2)
    {
        bool had_option = false;

        if(!std::strcmp("-p", argv[2]))
            fprintf(stderr, "Warning: -p argument is deprecated and useless!\n"); //adl_setPercMode(myDevice, 1);//Turn on AdLib percussion mode
        else if(!std::strcmp("-v", argv[2]))
            adl_setHVibrato(myDevice, 1);//Force turn on deep vibrato

#if !defined(OUTPUT_WAVE_ONLY) && !defined(HARDWARE_OPL3)
        else if(!std::strcmp("-w", argv[2]))
        {
            //Current Wave output implementation allows only SINT16 output
            g_audioFormat.type = ADLMIDI_SampleType_S16;
            g_audioFormat.containerSize = sizeof(int16_t);
            g_audioFormat.sampleOffset = sizeof(int16_t) * 2;
            recordWave = true;//Record library output into WAV file
        }
        else if(!std::strcmp("-s8", argv[2]) && !recordWave)
            spec.format = ADLMIDI_SampleType_S8;
        else if(!std::strcmp("-u8", argv[2]) && !recordWave)
            spec.format = ADLMIDI_SampleType_U8;
        else if(!std::strcmp("-s16", argv[2]) && !recordWave)
            spec.format = ADLMIDI_SampleType_S16;
        else if(!std::strcmp("-u16", argv[2]) && !recordWave)
            spec.format = ADLMIDI_SampleType_U16;
        else if(!std::strcmp("-s32", argv[2]) && !recordWave)
            spec.format = ADLMIDI_SampleType_S32;
        else if(!std::strcmp("-f32", argv[2]) && !recordWave)
            spec.format = ADLMIDI_SampleType_F32;
#endif

        else if(!std::strcmp("-t", argv[2]))
            adl_setHTremolo(myDevice, 1);//Force turn on deep tremolo

        else if(!std::strcmp("-frb", argv[2]))
            adl_setFullRangeBrightness(myDevice, 1);//Turn on a full-ranged XG CC74 Brightness

#ifndef OUTPUT_WAVE_ONLY
        else if(!std::strcmp("-nl", argv[2]))
            loopEnabled = 0; //Enable loop
#endif
        else if(!std::strcmp("-na", argv[2])) // Deprecated
            autoArpeggioEnabled = 0; //Enable auto-arpeggio
        else if(!std::strcmp("-ea", argv[2]))
            autoArpeggioEnabled = 1; //Enable auto-arpeggio

#ifndef HARDWARE_OPL3
        else if(!std::strcmp("--emu-nuked", argv[2]))
            emulator = ADLMIDI_EMU_NUKED;
        else if(!std::strcmp("--emu-nuked7", argv[2]))
            emulator = ADLMIDI_EMU_NUKED_174;
        else if(!std::strcmp("--emu-dosbox", argv[2]))
            emulator = ADLMIDI_EMU_DOSBOX;
        else if(!std::strcmp("--emu-opal", argv[2]))
            emulator = ADLMIDI_EMU_OPAL;
        else if(!std::strcmp("--emu-java", argv[2]))
            emulator = ADLMIDI_EMU_JAVA;
#endif
#if defined(ADLMIDI_ENABLE_HW_SERIAL) && !defined(OUTPUT_WAVE_ONLY)
        else if(!std::strcmp("--serial", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option --serial requires an argument!\n");
                return 1;
            }
            had_option = true;
            hwSerial = true;
            serialName = argv[3];
        }
        else if(!std::strcmp("--serial-baud", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option --serial-baud requires an argument!\n");
                return 1;
            }
            had_option = true;
            serialBaud = std::strtol(argv[3], NULL, 10);
        }
        else if(!std::strcmp("--serial-proto", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option --serial-proto requires an argument!\n");
                return 1;
            }
            had_option = true;
            serialProto = std::strtol(argv[3], NULL, 10);
        }
#endif
        else if(!std::strcmp("-fp", argv[2]))
            adl_setSoftPanEnabled(myDevice, 1);
        else if(!std::strcmp("-mb", argv[2]))
            multibankFromEnbededTest = true;
        else if(!std::strcmp("-s", argv[2]))
            adl_setScaleModulators(myDevice, 1);//Turn on modulators scaling by volume
#ifndef ADLMIDI_HW_OPL
        else if(!std::strcmp("--gain", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option --gain requires an argument!\n");
                return 1;
            }
            had_option = true;
            g_gaining = std::atof(argv[3]);
        }
#endif // HARDWARE_OPL3

#ifdef HARDWARE_OPL3
        else if(!std::strcmp("--time-freq", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option --time-freq requires an argument!\n");
                return 1;
            }

            unsigned timerFreq = std::strtoul(argv[3], NULL, 0);
            if(timerFreq == 0)
            {
                printError("The option --time-freq requires a non-zero integer argument!\n");
                return 1;
            }

            s_timeCounter.setDosTimerHZ(timerFreq);

            had_option = true;
        }
#endif
        else if(!std::strcmp("-vm", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option -vm requires an argument!\n");
                return 1;
            }
            volumeModel = std::strtol(argv[3], NULL, 10);
            had_option = true;
        }
        else if(!std::strcmp("-ca", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option -carequires an argument!\n");
                return 1;
            }
            chanAlloc = std::strtol(argv[3], NULL, 10);
            had_option = true;
        }
        else if(!std::strcmp("--solo", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option --solo requires an argument!\n");
                return 1;
            }
            soloTrack = std::strtoul(argv[3], NULL, 10);
            had_option = true;
        }
        else if(!std::strcmp("--song", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option --song requires an argument!\n");
                return 1;
            }
            songNumLoad = std::strtol(argv[3], NULL, 10);
            had_option = true;
        }
        else if(!std::strcmp("--only", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option --only requires an argument!\n");
                return 1;
            }

            const char *strp = argv[3];
            unsigned long value;
            unsigned size;
            bool err = std::sscanf(strp, "%lu%n", &value, &size) != 1;
            while(!err && *(strp += size))
            {
                onlyTracks.push_back(value);
                err = std::sscanf(strp, ",%lu%n", &value, &size) != 1;
            }
            if(err)
            {
                printError("Invalid argument to --only!\n");
                return 1;
            }
            onlyTracks.push_back(value);
            had_option = true;
        }

        else break;

        std::copy(argv + (had_option ? 4 : 3),
                  argv + argc,
                  argv + 2);
        argc -= (had_option ? 2 : 1);
    }

#ifndef OUTPUT_WAVE_ONLY
    //Turn loop on/off (for WAV recording loop must be disabled!)
    adl_setLoopEnabled(myDevice, recordWave ? 0 : loopEnabled);
#endif

    adl_setAutoArpeggio(myDevice, autoArpeggioEnabled);
    adl_setChannelAllocMode(myDevice, chanAlloc);

#ifdef DEBUG_TRACE_ALL_EVENTS
    //Hook all MIDI events are ticking while generating an output buffer
    if(!recordWave)
        adl_setRawEventHook(myDevice, debugPrintEvent, NULL);
#endif

#if defined(ADLMIDI_ENABLE_HW_SERIAL) && !defined(OUTPUT_WAVE_ONLY)
    if(hwSerial)
        adl_switchSerialHW(myDevice, serialName.c_str(), serialBaud, serialProto);
    else
#endif
#ifndef HARDWARE_OPL3
    adl_switchEmulator(myDevice, emulator);
#endif

    std::fprintf(stdout, " - Library version %s\n", adl_linkedLibraryVersion());
#ifdef HARDWARE_OPL3
    std::fprintf(stdout, " - Hardware OPL3 chip in use\n");
#elif defined(ADLMIDI_ENABLE_HW_SERIAL) && !defined(OUTPUT_WAVE_ONLY)
    if(hwSerial)
        std::fprintf(stdout, " - %s [device %s] in use\n", adl_chipEmulatorName(myDevice), serialName.c_str());
    else
        std::fprintf(stdout, " - %s Emulator in use\n", adl_chipEmulatorName(myDevice));
#else
    std::fprintf(stdout, " - %s Emulator in use\n", adl_chipEmulatorName(myDevice));
#endif

    if(argc >= 3)
    {
        if(is_number(argv[2]))
        {
            int bankno = std::atoi(argv[2]);
            //Choose one of embedded banks
            if(adl_setBank(myDevice, bankno) != 0)
            {
                printError(adl_errorInfo(myDevice), "Can't set an embedded bank");
                adl_close(myDevice);
                return 1;
            }
            std::fprintf(stdout, " - Use embedded bank #%d [%s]\n", bankno, adl_getBankNames()[bankno]);
        }
        else
        {
            std::string bankPath = argv[2];
            std::fprintf(stdout, " - Use custom bank [%s]...", bankPath.c_str());
            flushout(stdout);
            //Open external bank file (WOPL format is supported)
            //to create or edit them, use OPL3 Bank Editor you can take here https://github.com/Wohlstand/OPL3BankEditor
            if(adl_openBankFile(myDevice, bankPath.c_str()) != 0)
            {
                std::fprintf(stdout, "FAILED!\n");
                flushout(stdout);
                printError(adl_errorInfo(myDevice), "Can't open a custom bank file");
                adl_close(myDevice);
                return 1;
            }
            std::fprintf(stdout, "OK!\n");
        }
    }

    if(multibankFromEnbededTest)
    {
        ADL_BankId id[] =
        {
            {0, 0, 0}, /*62*/ // isPercussion, MIDI bank MSB, LSB
            {0, 8, 0}, /*14*/ // Use as MSB-8
            {1, 0, 0}, /*68*/
            {1, 0, 25} /*74*/
        };
        int banks[] =
        {
            62, 14, 68, 74
        };

        for(size_t i = 0; i < 4; i++)
        {
            ADL_Bank bank;
            if(adl_getBank(myDevice, &id[i], ADLMIDI_Bank_Create, &bank) < 0)
            {
                printError(adl_errorInfo(myDevice), "Can't get an embedded bank");
                adl_close(myDevice);
                return 1;
            }

            if(adl_loadEmbeddedBank(myDevice, &bank, banks[i]) < 0)
            {
                printError(adl_errorInfo(myDevice), "Can't load an embedded bank");
                adl_close(myDevice);
                return 1;
            }
        }

        std::fprintf(stdout, " - Ran a test of multibank over embedded\n");
    }

#ifndef HARDWARE_OPL3
    int numOfChips = 4;
    if(argc >= 4)
        numOfChips = std::atoi(argv[3]);

#if defined(ADLMIDI_ENABLE_HW_SERIAL) && !defined(OUTPUT_WAVE_ONLY)
    if(hwSerial)
        numOfChips = 1;
#endif

    //Set count of concurrent emulated chips count to excite channels limit of one chip
    if(adl_setNumChips(myDevice, numOfChips) != 0)
    {
        printError(adl_errorInfo(myDevice), "Can't set number of chips");
        adl_close(myDevice);
        return 1;
    }
#else
    int numOfChips = 1;
    adl_setNumChips(myDevice, numOfChips);
#endif

    if(volumeModel != ADLMIDI_VolumeModel_AUTO)
        adl_setVolumeRangeModel(myDevice, volumeModel);

    if(argc >= 5)
    {
        //Set total count of 4-operator channels between all emulated chips
        if(adl_setNumFourOpsChn(myDevice, std::atoi(argv[4])) != 0)
        {
            printError(adl_errorInfo(myDevice), "Can't set number of 4-op channels");
            adl_close(myDevice);
            return 1;
        }
    }

#if defined(DEBUG_SONG_CHANGE_BY_HOOK)
    adl_setTriggerHandler(myDevice, xmiTriggerCallback, NULL);
    adl_setLoopEndHook(myDevice, loopEndCallback, NULL);
    adl_setLoopHooksOnly(myDevice, 1);
#endif

    if(songNumLoad >= 0)
        adl_selectSongNum(myDevice, songNumLoad);

#ifdef DEBUG_SONG_SWITCHING
    set_conio_terminal_mode();
    if(songNumLoad < 0)
        songNumLoad = 0;
#endif

    std::string musPath = argv[1];
    //Open MIDI file to play
    if(adl_openFile(myDevice, musPath.c_str()) != 0)
    {
        printError(adl_errorInfo(myDevice), "Can't open MIDI file");
        adl_close(myDevice);
        return 2;
    }

    std::fprintf(stdout, " - Number of chips %d\n", adl_getNumChipsObtained(myDevice));
    std::fprintf(stdout, " - Number of four-ops %d\n", adl_getNumFourOpsChnObtained(myDevice));
    std::fprintf(stdout, " - Track count: %lu\n", static_cast<unsigned long>(adl_trackCount(myDevice)));
    std::fprintf(stdout, " - Volume model: %s\n", volume_model_to_str(adl_getVolumeRangeModel(myDevice)));
    std::fprintf(stdout, " - Channel allocation mode: %s\n", chanalloc_to_str(adl_getChannelAllocMode(myDevice)));

#ifndef HARDWARE_OPL3
#   ifdef ADLMIDI_ENABLE_HW_SERIAL
    if(!hwSerial)
#   endif
    {
        std::fprintf(stdout, " - Gain level: %g\n", g_gaining);
    }
#endif

    int songsCount = adl_getSongsCount(myDevice);
    if(songNumLoad >= 0)
        std::fprintf(stdout, " - Attempt to load song number: %d / %d\n", songNumLoad, songsCount);
    else if(songsCount > 0)
        std::fprintf(stdout, " - File contains %d song(s)\n", songsCount);

    if(soloTrack != ~static_cast<size_t>(0))
    {
        std::fprintf(stdout, " - Solo track: %lu\n", static_cast<unsigned long>(soloTrack));
        adl_setTrackOptions(myDevice, soloTrack, ADLMIDI_TrackOption_Solo);
    }

    if(!onlyTracks.empty())
    {
        size_t count = adl_trackCount(myDevice);
        for(size_t track = 0; track < count; ++track)
            adl_setTrackOptions(myDevice, track, ADLMIDI_TrackOption_Off);
        std::fprintf(stdout, " - Only tracks:");
        for(size_t i = 0, n = onlyTracks.size(); i < n; ++i)
        {
            size_t track = onlyTracks[i];
            adl_setTrackOptions(myDevice, track, ADLMIDI_TrackOption_On);
            std::fprintf(stdout, " %lu", static_cast<unsigned long>(track));
        }
        std::fprintf(stdout, "\n");
    }

    std::fprintf(stdout, " - Automatic arpeggio is turned %s\n", adl_getAutoArpeggio(myDevice) ? "ON" : "OFF");

    std::fprintf(stdout, " - File [%s] opened!\n", musPath.c_str());
    flushout(stdout);

#ifndef HARDWARE_OPL3
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
#   if !defined(_WIN32) && !defined(__WATCOMC__)
    signal(SIGHUP, sighandler);
#   endif

#else // HARDWARE_OPL3
    //disable();
    s_timeCounter.initDosTimer();
    s_timeCounter.flushDosTimer();
    //enable();
#endif//HARDWARE_OPL3

    s_timeCounter.setTotal(adl_totalTimeLength(myDevice));

#ifndef OUTPUT_WAVE_ONLY
    s_timeCounter.setLoop(adl_loopStartTime(myDevice), adl_loopEndTime(myDevice));

#   ifndef HARDWARE_OPL3
    if(!recordWave)
#   endif
    {
        std::fprintf(stdout, " - Loop is turned %s\n", loopEnabled ? "ON" : "OFF");
        if(s_timeCounter.hasLoop)
            std::fprintf(stdout, " - Has loop points: %s ... %s\n", s_timeCounter.loopStartHMS, s_timeCounter.loopEndHMS);
        std::fprintf(stdout, "\n==========================================\n");
        flushout(stdout);

#   ifndef HARDWARE_OPL3
#       ifdef ADLMIDI_ENABLE_HW_SERIAL
        if(hwSerial)
            runHWSerialLoop(myDevice);
        else
#       endif
        {
            int ret = runAudioLoop(myDevice, spec);
            if (ret != 0)
            {
                adl_close(myDevice);
                return ret;
            }
        }
#   else
        runDOSLoop(myDevice);
#   endif

        s_timeCounter.clearLine();
    }
#endif //OUTPUT_WAVE_ONLY

#ifndef HARDWARE_OPL3

#   ifndef OUTPUT_WAVE_ONLY
    else
#   endif //OUTPUT_WAVE_ONLY
    {
        int ret = runWaveOutLoopLoop(myDevice, musPath, sampleRate);
        if(ret != 0)
        {
            adl_close(myDevice);
            return ret;
        }
    }
#endif //HARDWARE_OPL3

#ifdef HARDWARE_OPL3
    s_timeCounter.restoreDosTimer();
#endif

    adl_close(myDevice);

    return 0;
}
