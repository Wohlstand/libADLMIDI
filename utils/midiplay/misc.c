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

#if (defined(_MSC_VER) && _MSC_VER < 1900) || defined(__DJGPP__)
#   include <stdarg.h>
#endif

#ifdef _WIN32
#   include <windows.h> // for Windows-specific setCursorVisibility implementation
#endif

#include "misc.h"

#if defined(ADLMIDI_ENABLE_HW_SERIAL) && !defined(OUTPUT_WAVE_ONLY)
#   if defined(ADLMIDI_USE_SDL2) || defined(ADLMIDI_USE_SDL3)
#       ifdef ADLMIDI_USE_SDL3
#           include <SDL3/SDL_timer.h>
#       elif defined(ADLMIDI_USE_SDL2)
#           include <SDL2/SDL_timer.h>
#       endif

#       ifdef __APPLE__
#           include <unistd.h>
#       endif
#   else
#       include <time.h>
#       include <unistd.h>
#       include <assert.h>
#   endif
#endif



#if defined(ADLMIDI_ENABLE_HW_SERIAL) && !defined(OUTPUT_WAVE_ONLY)
#   if defined(ADLMIDI_USE_SDL2) || defined(ADLMIDI_USE_SDL3)
double s_getTime(void)
{
#       ifdef ADLMIDI_USE_SDL3
    return SDL_GetTicks() / 1000.0;
#       else
    return SDL_GetTicks64() / 1000.0;
#       endif
}

void s_sleepU(double s)
{
#       ifdef __APPLE__
    /* For unknown reasons, any sleep functions to way WAY LONGER than requested */
    /* So, implementing an own one. */
    static double debt = 0.0;
    double target = s_getTime() + s - debt;

    while(s_getTime() < target)
        usleep(1000);

    debt = s_getTime() - target;
#       else
    SDL_Delay((Uint32)(s * 1000));
#       endif
}

#   else /* Non-SDL implementation */

double s_getTime(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (t.tv_nsec + (t.tv_sec * 1000000000)) / 1000000000.0;
}

void s_sleepU(double s)
{
    static double debt = 0.0;
    double target = s_getTime() + s - debt;

    while(s_getTime() < target)
        usleep(1000);

    debt = s_getTime() - target;
}
#   endif
#endif






#if defined(DEBUG_SONG_SWITCHING) || defined(ENABLE_TERMINAL_HOTKEYS)
#   include <unistd.h>
#   include <sys/select.h>
#   ifndef TERMINAL_USE_NCURSES
#       include <termios.h>
#   else
#       include <ncurses.h>
#   endif

#   ifndef TERMINAL_USE_NCURSES
struct termios orig_termios;
#   endif

#   ifndef TERMINAL_USE_NCURSES
void reset_terminal_mode(void)
{
    tcsetattr(0, TCSANOW, &orig_termios);
}
#   endif

void set_conio_terminal_mode(void)
{
#   ifndef TERMINAL_USE_NCURSES
    struct termios new_termios;

    /* take two copies - one for now, one for later */
    tcgetattr(0, &orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));

    /* register cleanup handler, and set the new terminal mode */
    atexit(reset_terminal_mode);
    cfmakeraw(&new_termios);
    tcsetattr(0, TCSANOW, &new_termios);
#   else
    setenv("TERMINFO", "/usr/share/terminfo", 1);
    setenv("TERM", "linux", 1);
    initscr();

    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);
    scrollok(stdscr, TRUE);
#   endif
}

int kbhit(void)
{
#   ifndef TERMINAL_USE_NCURSES
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv) > 0;
#   else
    int ch = getch();
    if(ch != ERR)
    {
        ungetch(ch);
        return 1;
    }
    else
        return 0;
#   endif
}

#   ifndef TERMINAL_USE_NCURSES
int getch(void)
{
    int r;
    unsigned char c;
    if ((r = read(0, &c, sizeof(c))) < 0)
        return r;
    else
        return c;
}
#   endif
#endif




#if defined(_MSC_VER) && _MSC_VER < 1900
int c99_vsnprintf(char *outBuf, size_t size, const char *format, va_list ap)
{
    int count = -1;

    if (size != 0)
        count = _vsnprintf_s(outBuf, size, _TRUNCATE, format, ap);
    if (count == -1)
        count = _vscprintf(format, ap);

    return count;
}

int c99_snprintf(char *outBuf, size_t size, const char *format, ...)
{
    int count;
    va_list ap;

    va_start(ap, format);
    count = c99_vsnprintf(outBuf, size, format, ap);
    va_end(ap);

    return count;
}
#endif



#ifdef __DJGPP__
int s_fprintf(FILE *stream, const char *format, ...)
{
    int ret;
    va_list args;
    dos_taskman_suspend();
    va_start(args, format);
    if(dos_taskman_is_inside_interrupt())
        ret = dos_taskman_reserve_fprintf(stream, format, args);
    else
        ret = vfprintf(stream, format, args);
    va_end(args);
    dos_taskman_resume();
    return ret;
}
#endif /* __DJGPP__ */





#ifdef ADLMIDI_ENABLE_HW_DOS
void keyWait()
{
    s_fprintf(stdout, "<press any key to continue...>");
    flushout(stdout);
    getch();
    s_fprintf(stdout, "\r                              \r");
}
#endif





#ifdef ADLMIDI_ENABLE_HW_DOS

void setCursorVisibility(int visible)
{
    union REGS regs;

    regs.w.ax = 0x0100;
    regs.w.cx = visible ? 0x0708 : 0x2000;
#   if defined(__FLAT__) || defined(__DJGPP__)
    int386(0x10, &regs, &regs);
#   else
    int86(0x10, &regs, &regs);
#   endif
}

#else /* ADLMIDI_ENABLE_HW_DOS */

void setCursorVisibility(int visible)
{
#   ifdef _WIN32
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    info.dwSize = 100;
    info.bVisible = visible ? TRUE : FALSE;
    SetConsoleCursorInfo(consoleHandle, &info);
#   else
    if(visible)
        printf("\e[?25h");
    else
        printf("\e[?25l");
#   endif
}

#endif  /* ADLMIDI_ENABLE_HW_DOS */
