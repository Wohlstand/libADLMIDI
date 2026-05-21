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
#ifndef ADLMIDI_MISC_H
#define ADLMIDI_MISC_H

#include <stdio.h> // IWYU pragma: keep

#if defined(__DJGPP__)
#   ifdef __cplusplus
#       include "playback/dos_tman.h" // IWYU pragma: keep
#   else
#       include "playback/dos_tman_c.h" // IWYU pragma: keep
#   endif
#endif


#if defined(__DJGPP__) || (defined(__WATCOMC__) && (defined(__DOS__) || defined(__DOS4G__) || defined(__DOS4GNZ__)))
#   define HW_OPL_MSDOS
#   include <conio.h>
#   include <dos.h>
#   include <sys/nearptr.h>

#   ifdef __WATCOMC__
/*#       include <wdpmi.h>*/
#       include <i86.h>
#       include <bios.h>
#       include <time.h>
#   endif /* __WATCOMC__ */
#endif




#ifdef __cplusplus
extern "C"
{
#endif

#if defined(ADLMIDI_ENABLE_HW_SERIAL) && !defined(OUTPUT_WAVE_ONLY)
#   if defined(ADLMIDI_USE_SDL2) || defined(ADLMIDI_USE_SDL3)
#       define HAS_S_GETTIME
#   endif
#endif

#if defined(DEBUG_SONG_SWITCHING) || defined(ENABLE_TERMINAL_HOTKEYS)
#   ifndef TERMINAL_USE_NCURSES
#       include <termios.h>
#   else
#       include <ncurses.h>
#   endif
#endif





double s_getTime(void);
void s_sleepU(double s);



#if defined(DEBUG_SONG_SWITCHING) || defined(ENABLE_TERMINAL_HOTKEYS)
void set_conio_terminal_mode(void);

#   ifndef TERMINAL_USE_NCURSES
void reset_terminal_mode(void);
int kbhit(void);
int getch(void);
#   endif
#endif





#if defined(_MSC_VER) && _MSC_VER < 1900
#   define snprintf c99_snprintf
#   define vsnprintf c99_vsnprintf

int c99_vsnprintf(char *outBuf, size_t size, const char *format, va_list ap);
int c99_snprintf(char *outBuf, size_t size, const char *format, ...);
#endif




#if defined(__WATCOMC__)
/* Use normal fprintf, but don't call fflush since this ruins the runtime */
#   define s_fprintf        std::fprintf
#   define flushout(stream)
#elif defined(__DJGPP__)
int s_fprintf(FILE *stream, const char *format, ...);
/* Use printf and flush with interrupt triggering suspension to avoid execution of interrupt just in a middle of the fprintf and fflush execution */
#   ifdef __cplusplus
#       define flushout(stream) \
            {\
                DosTaskman::suspend();\
                DosTaskman::reserve_flush(stream);\
                std::fflush(stream);\
                DosTaskman::resume();\
            }
#   else
#       define flushout(stream) \
            {\
                    dos_taskman_suspend();\
                    dos_taskman_reserve_flush(stream);\
                    fflush(stream);\
                    dos_taskman_resume();\
            }
#   endif
#else
/* Use normal fprintf and fflush */
#   ifdef __cplusplus
#       define s_fprintf        std::fprintf
#       define flushout(stream) std::fflush(stream)
#   else
#       define s_fprintf        fprintf
#       define flushout(stream) fflush(stream)
#   endif
#endif


#ifdef ADLMIDI_ENABLE_HW_DOS
void keyWait();
#endif



void setCursorVisibility(int visible);


#ifdef __cplusplus
}
#endif

#endif /*ADLMIDI_MISC_H*/
