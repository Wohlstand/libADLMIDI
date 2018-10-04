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

#include "input.hpp"

xInput::xInput()
{
#ifdef _WIN32
    inhandle = GetStdHandle(STD_INPUT_HANDLE);
#endif
#if (!defined(_WIN32) || defined(__CYGWIN__)) && !defined(__DJGPP__) && !defined(__APPLE__)
    ioctl(0, TCGETA, &back);
    struct termio term = back;
    term.c_lflag &= ~(ICANON | ECHO);
    term.c_cc[VMIN] = 0; // 0=no block, 1=do block
    if(ioctl(0, TCSETA, &term) < 0)
        fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
#endif
}

xInput::~xInput()
{
#if (!defined(_WIN32) || defined(__CYGWIN__)) && !defined(__DJGPP__) && !defined(__APPLE__)
    if(ioctl(0, TCSETA, &back) < 0)
        fcntl(0, F_SETFL, fcntl(0, F_GETFL) & ~ O_NONBLOCK);
#endif
}

char xInput::PeekInput()
{
#ifdef __DJGPP__
    if(kbhit())
    {
        int c = getch();
        return c ? c : getch();
    }
#endif
#ifdef _WIN32
    DWORD nread = 0;
    INPUT_RECORD inbuf[1];
    while(PeekConsoleInput(inhandle, inbuf, sizeof(inbuf) / sizeof(*inbuf), &nread) && nread)
    {
        ReadConsoleInput(inhandle, inbuf, sizeof(inbuf) / sizeof(*inbuf), &nread);
        if(inbuf[0].EventType == KEY_EVENT
                && inbuf[0].Event.KeyEvent.bKeyDown)
        {
            char c = inbuf[0].Event.KeyEvent.uChar.AsciiChar;
            unsigned s = inbuf[0].Event.KeyEvent.wVirtualScanCode;
            if(c == 0) c = s;
            return c;
        }
    }
#endif
#if (!defined(_WIN32) || defined(__CYGWIN__)) && !defined(__DJGPP__)
    char c = 0;
    if(read(0, &c, 1) == 1) return c;
#endif
    return '\0';
}
