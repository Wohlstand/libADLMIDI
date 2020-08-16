/*
 * libADLMIDI is a free Software MIDI synthesizer library with OPL3 emulation
 *
 * ADLMIDI Library API:   Copyright (c) 2015-2019 Vitaly Novichkov <admin@wohlnet.ru>
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

#ifndef OPLMIDI_MIDIPLAY_SETUP_HPP
#define OPLMIDI_MIDIPLAY_SETUP_HPP

#include <cstdio>

struct MIDIsetup
{
    int          emulator;
    bool         runAtPcmRate;
    unsigned int bankId;
    int          numFourOps;
    unsigned int numChips;
    int     deepTremoloMode;
    int     deepVibratoMode;
    int     rhythmMode;
    bool    logarithmicVolumes;
    int     volumeScaleModel;
    //unsigned int SkipForward;
    int     scaleModulators;
    bool    fullRangeBrightnessCC74;

    double delay;
    double carry;

    /* The lag between visual content and audio content equals */
    /* the sum of these two buffers. */
    double mindelay;
    double maxdelay;

    /* For internal usage */
    ssize_t tick_skip_samples_delay; /* Skip tick processing after samples count. */
    /* For internal usage */

    unsigned long PCM_RATE;
};

#endif // OPLMIDI_MIDIPLAY_SETUP_HPP
