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
#ifndef ADLMIDI_DEV_SETUP_H
#define ADLMIDI_DEV_SETUP_H

#include <string>
#include <vector>
#include "audio.h"
#include "adlmidi.h"

struct Args
{
    int     setHwVibrato;
    int     setHwTremolo;
    int     setScaleMods;

    int         setBankNo;
    std::string setBankFile;
    int         setNum4op;
    int         setNumChips;

    std::string musPath;

    int     setFullRangeBright;

    int     enableFullPanning;

#ifndef OUTPUT_WAVE_ONLY
    bool    recordWave;
    int     loopEnabled;
#endif

    unsigned int    sampleRate;

#if !defined(ADLMIDI_ENABLE_HW_DOS)
    //const unsigned MaxSamplesAtTime = 512; // 512=dbopl limitation
    // How long is SDL buffer, in seconds?
    // The smaller the value, the more often SDL_AudioCallBack()
    // is called.
    // const double    AudioBufferLength;

    AudioOutputSpec spec;
#endif

#ifdef ADLMIDI_ENABLE_HW_DOS
    ADL_UInt16  setHwAddress;
    int         setChipType;
    unsigned    clock_freq;
#endif

#if defined(ADLMIDI_ENABLE_HW_SERIAL) && !defined(OUTPUT_WAVE_ONLY)
    bool            hwSerial;
    std::string     serialName;
    unsigned        serialBaud;
    unsigned        serialProto;
#endif

    /*
     * Set library options by parsing of command line arguments
     */
    bool    multibankFromEnbededTest;

    int     autoArpeggioEnabled;
    int     chanAlloc;

#ifndef ADLMIDI_ENABLE_HW_DOS
    int     emulator;
#endif
    int     volumeModel;

    size_t              soloTrack;
    int                 songNumLoad;
    std::vector<size_t> onlyTracks;


    Args();

    int parseArgs(int argc, char **argv_arr, bool *quit);
};


void printError(const char *err, const char *what = NULL);

extern ADLMIDI_AudioFormat g_audioFormat;
extern float g_gaining;
extern Args s_devSetup;

#endif
