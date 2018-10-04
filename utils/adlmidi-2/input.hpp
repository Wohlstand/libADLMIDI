/*
 *  ADLMIDI Player - a console MIDI player with OPL3 emulation
 *
 *  Original code: Copyright (c) 2010-2018 Joel Yliluoma <bisqwit@iki.fi>
 *  Copyright (C) 2015-2018 Vitaly Novichkov <admin@wohlnet.ru>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef _WIN32
# include <cctype>
# define WIN32_LEAN_AND_MEAN
# ifndef NOMINMAX
#   define NOMINMAX //To don't damage std::min and std::max
# endif
# include <windows.h>
#endif

#ifdef __DJGPP__
#include <conio.h>
#include <pc.h>
#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>
#include <sys/exceptn.h>
#include <dos.h>
#include <stdlib.h>
#define BIOStimer _farpeekl(_dos_ds, 0x46C)
static const unsigned NewTimerFreq = 209;
#elif !defined(_WIN32) || defined(__CYGWIN__)
# include <termios.h>
# include <fcntl.h>
# include <sys/ioctl.h>
# include <csignal>
#endif

class xInput
{
    #ifdef _WIN32
    void *inhandle;
    #endif
    #if (!defined(_WIN32) || defined(__CYGWIN__)) && !defined(__DJGPP__) && !defined(__APPLE__)
    struct termio back;
    #endif
public:
    xInput();
    ~xInput();

    char PeekInput();
};

extern xInput Input;
