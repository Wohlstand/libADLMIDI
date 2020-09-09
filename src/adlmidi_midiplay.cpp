/*
 * libADLMIDI is a free Software MIDI synthesizer library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2020 Vitaly Novichkov <admin@wohlnet.ru>
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

#include "adlmidi_midiplay.hpp"
#include "adlmidi_opl3.hpp"
#include "adlmidi_private.hpp"
#include "midi_sequencer.hpp"

// Minimum life time of percussion notes
static const double s_drum_note_min_time = 0.03;


// Standard frequency formula
static inline double s_commonFreq(double note, double bend)
{
    return BEND_COEFFICIENT * std::exp(0.057762265 * (note + bend));
}

// DMX volumes table
static const int_fast32_t s_dmx_freq_table[] =
{
    0x0133, 0x0133, 0x0134, 0x0134, 0x0135, 0x0136, 0x0136, 0x0137,
    0x0137, 0x0138, 0x0138, 0x0139, 0x0139, 0x013A, 0x013B, 0x013B,
    0x013C, 0x013C, 0x013D, 0x013D, 0x013E, 0x013F, 0x013F, 0x0140,
    0x0140, 0x0141, 0x0142, 0x0142, 0x0143, 0x0143, 0x0144, 0x0144,

    0x0145, 0x0146, 0x0146, 0x0147, 0x0147, 0x0148, 0x0149, 0x0149,
    0x014A, 0x014A, 0x014B, 0x014C, 0x014C, 0x014D, 0x014D, 0x014E,
    0x014F, 0x014F, 0x0150, 0x0150, 0x0151, 0x0152, 0x0152, 0x0153,
    0x0153, 0x0154, 0x0155, 0x0155, 0x0156, 0x0157, 0x0157, 0x0158,

    0x0158, 0x0159, 0x015A, 0x015A, 0x015B, 0x015B, 0x015C, 0x015D,
    0x015D, 0x015E, 0x015F, 0x015F, 0x0160, 0x0161, 0x0161, 0x0162,
    0x0162, 0x0163, 0x0164, 0x0164, 0x0165, 0x0166, 0x0166, 0x0167,
    0x0168, 0x0168, 0x0169, 0x016A, 0x016A, 0x016B, 0x016C, 0x016C,

    0x016D, 0x016E, 0x016E, 0x016F, 0x0170, 0x0170, 0x0171, 0x0172,
    0x0172, 0x0173, 0x0174, 0x0174, 0x0175, 0x0176, 0x0176, 0x0177,
    0x0178, 0x0178, 0x0179, 0x017A, 0x017A, 0x017B, 0x017C, 0x017C,
    0x017D, 0x017E, 0x017E, 0x017F, 0x0180, 0x0181, 0x0181, 0x0182,

    0x0183, 0x0183, 0x0184, 0x0185, 0x0185, 0x0186, 0x0187, 0x0188,
    0x0188, 0x0189, 0x018A, 0x018A, 0x018B, 0x018C, 0x018D, 0x018D,
    0x018E, 0x018F, 0x018F, 0x0190, 0x0191, 0x0192, 0x0192, 0x0193,
    0x0194, 0x0194, 0x0195, 0x0196, 0x0197, 0x0197, 0x0198, 0x0199,

    0x019A, 0x019A, 0x019B, 0x019C, 0x019D, 0x019D, 0x019E, 0x019F,
    0x01A0, 0x01A0, 0x01A1, 0x01A2, 0x01A3, 0x01A3, 0x01A4, 0x01A5,
    0x01A6, 0x01A6, 0x01A7, 0x01A8, 0x01A9, 0x01A9, 0x01AA, 0x01AB,
    0x01AC, 0x01AD, 0x01AD, 0x01AE, 0x01AF, 0x01B0, 0x01B0, 0x01B1,

    0x01B2, 0x01B3, 0x01B4, 0x01B4, 0x01B5, 0x01B6, 0x01B7, 0x01B8,
    0x01B8, 0x01B9, 0x01BA, 0x01BB, 0x01BC, 0x01BC, 0x01BD, 0x01BE,
    0x01BF, 0x01C0, 0x01C0, 0x01C1, 0x01C2, 0x01C3, 0x01C4, 0x01C4,
    0x01C5, 0x01C6, 0x01C7, 0x01C8, 0x01C9, 0x01C9, 0x01CA, 0x01CB,

    0x01CC, 0x01CD, 0x01CE, 0x01CE, 0x01CF, 0x01D0, 0x01D1, 0x01D2,
    0x01D3, 0x01D3, 0x01D4, 0x01D5, 0x01D6, 0x01D7, 0x01D8, 0x01D8,
    0x01D9, 0x01DA, 0x01DB, 0x01DC, 0x01DD, 0x01DE, 0x01DE, 0x01DF,
    0x01E0, 0x01E1, 0x01E2, 0x01E3, 0x01E4, 0x01E5, 0x01E5, 0x01E6,

    0x01E7, 0x01E8, 0x01E9, 0x01EA, 0x01EB, 0x01EC, 0x01ED, 0x01ED,
    0x01EE, 0x01EF, 0x01F0, 0x01F1, 0x01F2, 0x01F3, 0x01F4, 0x01F5,
    0x01F6, 0x01F6, 0x01F7, 0x01F8, 0x01F9, 0x01FA, 0x01FB, 0x01FC,
    0x01FD, 0x01FE, 0x01FF,

    0x0200, 0x0201, 0x0201, 0x0202, 0x0203, 0x0204, 0x0205, 0x0206,
    0x0207, 0x0208, 0x0209, 0x020A, 0x020B, 0x020C, 0x020D, 0x020E,
    0x020F, 0x0210, 0x0210, 0x0211, 0x0212, 0x0213, 0x0214, 0x0215,
    0x0216, 0x0217, 0x0218, 0x0219, 0x021A, 0x021B, 0x021C, 0x021D,

    0x021E, 0x021F, 0x0220, 0x0221, 0x0222, 0x0223, 0x0224, 0x0225,
    0x0226, 0x0227, 0x0228, 0x0229, 0x022A, 0x022B, 0x022C, 0x022D,
    0x022E, 0x022F, 0x0230, 0x0231, 0x0232, 0x0233, 0x0234, 0x0235,
    0x0236, 0x0237, 0x0238, 0x0239, 0x023A, 0x023B, 0x023C, 0x023D,

    0x023E, 0x023F, 0x0240, 0x0241, 0x0242, 0x0244, 0x0245, 0x0246,
    0x0247, 0x0248, 0x0249, 0x024A, 0x024B, 0x024C, 0x024D, 0x024E,
    0x024F, 0x0250, 0x0251, 0x0252, 0x0253, 0x0254, 0x0256, 0x0257,
    0x0258, 0x0259, 0x025A, 0x025B, 0x025C, 0x025D, 0x025E, 0x025F,

    0x0260, 0x0262, 0x0263, 0x0264, 0x0265, 0x0266, 0x0267, 0x0268,
    0x0269, 0x026A, 0x026C, 0x026D, 0x026E, 0x026F, 0x0270, 0x0271,
    0x0272, 0x0273, 0x0275, 0x0276, 0x0277, 0x0278, 0x0279, 0x027A,
    0x027B, 0x027D, 0x027E, 0x027F, 0x0280, 0x0281, 0x0282, 0x0284,

    0x0285, 0x0286, 0x0287, 0x0288, 0x0289, 0x028B, 0x028C, 0x028D,
    0x028E, 0x028F, 0x0290, 0x0292, 0x0293, 0x0294, 0x0295, 0x0296,
    0x0298, 0x0299, 0x029A, 0x029B, 0x029C, 0x029E, 0x029F, 0x02A0,
    0x02A1, 0x02A2, 0x02A4, 0x02A5, 0x02A6, 0x02A7, 0x02A9, 0x02AA,

    0x02AB, 0x02AC, 0x02AE, 0x02AF, 0x02B0, 0x02B1, 0x02B2, 0x02B4,
    0x02B5, 0x02B6, 0x02B7, 0x02B9, 0x02BA, 0x02BB, 0x02BD, 0x02BE,
    0x02BF, 0x02C0, 0x02C2, 0x02C3, 0x02C4, 0x02C5, 0x02C7, 0x02C8,
    0x02C9, 0x02CB, 0x02CC, 0x02CD, 0x02CE, 0x02D0, 0x02D1, 0x02D2,

    0x02D4, 0x02D5, 0x02D6, 0x02D8, 0x02D9, 0x02DA, 0x02DC, 0x02DD,
    0x02DE, 0x02E0, 0x02E1, 0x02E2, 0x02E4, 0x02E5, 0x02E6, 0x02E8,
    0x02E9, 0x02EA, 0x02EC, 0x02ED, 0x02EE, 0x02F0, 0x02F1, 0x02F2,
    0x02F4, 0x02F5, 0x02F6, 0x02F8, 0x02F9, 0x02FB, 0x02FC, 0x02FD,

    0x02FF, 0x0300, 0x0302, 0x0303, 0x0304, 0x0306, 0x0307, 0x0309,
    0x030A, 0x030B, 0x030D, 0x030E, 0x0310, 0x0311, 0x0312, 0x0314,
    0x0315, 0x0317, 0x0318, 0x031A, 0x031B, 0x031C, 0x031E, 0x031F,
    0x0321, 0x0322, 0x0324, 0x0325, 0x0327, 0x0328, 0x0329, 0x032B,

    0x032C, 0x032E, 0x032F, 0x0331, 0x0332, 0x0334, 0x0335, 0x0337,
    0x0338, 0x033A, 0x033B, 0x033D, 0x033E, 0x0340, 0x0341, 0x0343,
    0x0344, 0x0346, 0x0347, 0x0349, 0x034A, 0x034C, 0x034D, 0x034F,
    0x0350, 0x0352, 0x0353, 0x0355, 0x0357, 0x0358, 0x035A, 0x035B,

    0x035D, 0x035E, 0x0360, 0x0361, 0x0363, 0x0365, 0x0366, 0x0368,
    0x0369, 0x036B, 0x036C, 0x036E, 0x0370, 0x0371, 0x0373, 0x0374,
    0x0376, 0x0378, 0x0379, 0x037B, 0x037C, 0x037E, 0x0380, 0x0381,
    0x0383, 0x0384, 0x0386, 0x0388, 0x0389, 0x038B, 0x038D, 0x038E,

    0x0390, 0x0392, 0x0393, 0x0395, 0x0397, 0x0398, 0x039A, 0x039C,
    0x039D, 0x039F, 0x03A1, 0x03A2, 0x03A4, 0x03A6, 0x03A7, 0x03A9,
    0x03AB, 0x03AC, 0x03AE, 0x03B0, 0x03B1, 0x03B3, 0x03B5, 0x03B7,
    0x03B8, 0x03BA, 0x03BC, 0x03BD, 0x03BF, 0x03C1, 0x03C3, 0x03C4,

    0x03C6, 0x03C8, 0x03CA, 0x03CB, 0x03CD, 0x03CF, 0x03D1, 0x03D2,
    0x03D4, 0x03D6, 0x03D8, 0x03DA, 0x03DB, 0x03DD, 0x03DF, 0x03E1,
    0x03E3, 0x03E4, 0x03E6, 0x03E8, 0x03EA, 0x03EC, 0x03ED, 0x03EF,
    0x03F1, 0x03F3, 0x03F5, 0x03F6, 0x03F8, 0x03FA, 0x03FC, 0x03FE,

    0x036C
};

static inline double s_dmxFreq(double note, double bend)
{
    uint_fast32_t noteI = (uint_fast32_t)(note + 0.5);
    int_fast32_t bendI = 0;
    int_fast32_t outHz = 0.0;
    double bendDec = bend - (int)bend;

    noteI += (int)bend;

    bendI = (int_fast32_t)((bendDec * 128.0) / 2.0) + 128;
    bendI = bendI >> 1;

    int_fast32_t oct = 0;
    int_fast32_t freqIndex = (noteI << 5) + bendI;

    if(freqIndex < 0)
        freqIndex = 0;
    else if(freqIndex >= 284)
    {
        freqIndex -= 284;
        oct = freqIndex / 384;
        freqIndex = (freqIndex % 384) + 284;
    }

    outHz = s_dmx_freq_table[freqIndex];

    while(oct > 1)
    {
        outHz *= 2;
        oct -= 1;
    }

    return (double)outHz;
}


static const int_fast32_t s_apogee_freq_table[31 + 1][12] =
{
    { 0x157, 0x16b, 0x181, 0x198, 0x1b0, 0x1ca, 0x1e5, 0x202, 0x220, 0x241, 0x263, 0x287 },
    { 0x157, 0x16b, 0x181, 0x198, 0x1b0, 0x1ca, 0x1e5, 0x202, 0x220, 0x242, 0x264, 0x288 },
    { 0x158, 0x16c, 0x182, 0x199, 0x1b1, 0x1cb, 0x1e6, 0x203, 0x221, 0x243, 0x265, 0x289 },
    { 0x158, 0x16c, 0x183, 0x19a, 0x1b2, 0x1cc, 0x1e7, 0x204, 0x222, 0x244, 0x266, 0x28a },
    { 0x159, 0x16d, 0x183, 0x19a, 0x1b3, 0x1cd, 0x1e8, 0x205, 0x223, 0x245, 0x267, 0x28b },
    { 0x15a, 0x16e, 0x184, 0x19b, 0x1b3, 0x1ce, 0x1e9, 0x206, 0x224, 0x246, 0x268, 0x28c },
    { 0x15a, 0x16e, 0x185, 0x19c, 0x1b4, 0x1ce, 0x1ea, 0x207, 0x225, 0x247, 0x269, 0x28e },
    { 0x15b, 0x16f, 0x185, 0x19d, 0x1b5, 0x1cf, 0x1eb, 0x208, 0x226, 0x248, 0x26a, 0x28f },
    { 0x15b, 0x170, 0x186, 0x19d, 0x1b6, 0x1d0, 0x1ec, 0x209, 0x227, 0x249, 0x26b, 0x290 },
    { 0x15c, 0x170, 0x187, 0x19e, 0x1b7, 0x1d1, 0x1ec, 0x20a, 0x228, 0x24a, 0x26d, 0x291 },
    { 0x15d, 0x171, 0x188, 0x19f, 0x1b7, 0x1d2, 0x1ed, 0x20b, 0x229, 0x24b, 0x26e, 0x292 },
    { 0x15d, 0x172, 0x188, 0x1a0, 0x1b8, 0x1d3, 0x1ee, 0x20c, 0x22a, 0x24c, 0x26f, 0x293 },
    { 0x15e, 0x172, 0x189, 0x1a0, 0x1b9, 0x1d4, 0x1ef, 0x20d, 0x22b, 0x24d, 0x270, 0x295 },
    { 0x15f, 0x173, 0x18a, 0x1a1, 0x1ba, 0x1d4, 0x1f0, 0x20e, 0x22c, 0x24e, 0x271, 0x296 },
    { 0x15f, 0x174, 0x18a, 0x1a2, 0x1bb, 0x1d5, 0x1f1, 0x20f, 0x22d, 0x24f, 0x272, 0x297 },
    { 0x160, 0x174, 0x18b, 0x1a3, 0x1bb, 0x1d6, 0x1f2, 0x210, 0x22e, 0x250, 0x273, 0x298 },
    { 0x161, 0x175, 0x18c, 0x1a3, 0x1bc, 0x1d7, 0x1f3, 0x211, 0x22f, 0x251, 0x274, 0x299 },
    { 0x161, 0x176, 0x18c, 0x1a4, 0x1bd, 0x1d8, 0x1f4, 0x212, 0x230, 0x252, 0x276, 0x29b },
    { 0x162, 0x176, 0x18d, 0x1a5, 0x1be, 0x1d9, 0x1f5, 0x212, 0x231, 0x254, 0x277, 0x29c },
    { 0x162, 0x177, 0x18e, 0x1a6, 0x1bf, 0x1d9, 0x1f5, 0x213, 0x232, 0x255, 0x278, 0x29d },
    { 0x163, 0x178, 0x18f, 0x1a6, 0x1bf, 0x1da, 0x1f6, 0x214, 0x233, 0x256, 0x279, 0x29e },
    { 0x164, 0x179, 0x18f, 0x1a7, 0x1c0, 0x1db, 0x1f7, 0x215, 0x235, 0x257, 0x27a, 0x29f },
    { 0x164, 0x179, 0x190, 0x1a8, 0x1c1, 0x1dc, 0x1f8, 0x216, 0x236, 0x258, 0x27b, 0x2a1 },
    { 0x165, 0x17a, 0x191, 0x1a9, 0x1c2, 0x1dd, 0x1f9, 0x217, 0x237, 0x259, 0x27c, 0x2a2 },
    { 0x166, 0x17b, 0x192, 0x1aa, 0x1c3, 0x1de, 0x1fa, 0x218, 0x238, 0x25a, 0x27e, 0x2a3 },
    { 0x166, 0x17b, 0x192, 0x1aa, 0x1c3, 0x1df, 0x1fb, 0x219, 0x239, 0x25b, 0x27f, 0x2a4 },
    { 0x167, 0x17c, 0x193, 0x1ab, 0x1c4, 0x1e0, 0x1fc, 0x21a, 0x23a, 0x25c, 0x280, 0x2a6 },
    { 0x168, 0x17d, 0x194, 0x1ac, 0x1c5, 0x1e0, 0x1fd, 0x21b, 0x23b, 0x25d, 0x281, 0x2a7 },
    { 0x168, 0x17d, 0x194, 0x1ad, 0x1c6, 0x1e1, 0x1fe, 0x21c, 0x23c, 0x25e, 0x282, 0x2a8 },
    { 0x169, 0x17e, 0x195, 0x1ad, 0x1c7, 0x1e2, 0x1ff, 0x21d, 0x23d, 0x260, 0x283, 0x2a9 },
    { 0x16a, 0x17f, 0x196, 0x1ae, 0x1c8, 0x1e3, 0x1ff, 0x21e, 0x23e, 0x261, 0x284, 0x2ab },
    { 0x16a, 0x17f, 0x197, 0x1af, 0x1c8, 0x1e4, 0x200, 0x21f, 0x23f, 0x262, 0x286, 0x2ac }
};

static inline double s_apogeeFreq(double note, double bend)
{
    uint_fast32_t noteI = (uint_fast32_t)(note + 0.5);
    int_fast32_t bendI = 0;
    int_fast32_t outHz = 0.0;
    double bendDec = bend - (int)bend;
    int_fast32_t octave;
    int_fast32_t scaleNote;

    noteI += (int)bend;
    bendI = (int_fast32_t)(bendDec * 32) + 32;

    noteI += bendI / 32;
    noteI -= 1;

    scaleNote = noteI % 12;
    octave = noteI / 12;

    outHz = s_apogee_freq_table[bendI % 32][scaleNote];

    while(octave > 1)
    {
        outHz *= 2;
        octave -= 1;
    }

    return (double)outHz;
}

//static const double s_9x_opl_samplerate = 50000.0;
//static const double s_9x_opl_tune = 440.0;
static const uint_fast8_t s_9x_opl_pitchfrac = 8;

static const uint_fast32_t s_9x_opl_freq[12] =
{
    0xAB7, 0xB5A, 0xC07, 0xCBE, 0xD80, 0xE4D, 0xF27, 0x100E, 0x1102, 0x1205, 0x1318, 0x143A
};

static const int32_t s_9x_opl_uppitch = 31;
static const int32_t s_9x_opl_downpitch = 27;

static uint_fast32_t s_9x_opl_applypitch(uint_fast32_t freq, int_fast32_t pitch)
{
    int32_t diff;

    if(pitch > 0)
    {
        diff = (pitch * s_9x_opl_uppitch) >> s_9x_opl_pitchfrac;
        freq += (diff * freq) >> 15;
    }
    else if (pitch < 0)
    {
        diff = (-pitch * s_9x_opl_downpitch) >> s_9x_opl_pitchfrac;
        freq -= (diff * freq) >> 15;
    }

    return freq;
}

static inline double s_9xFreq(double noteD, double bendD)
{
    uint_fast32_t note = (uint_fast32_t)(noteD + 0.5);
    int_fast32_t bend;
    double bendDec = bendD - (int)bendD; // 0.0 ± 1.0 - one halftone

    uint_fast32_t freq;
    uint_fast32_t freqpitched;
    uint_fast32_t octave;

    uint_fast32_t bendMsb;
    uint_fast32_t bendLsb;

    note += (int)bendD;
    bend = (int_fast32_t)(bendDec * 4096) + 8192; // convert to MIDI standard value

    bendMsb = (bend >> 7) & 0x7F;
    bendLsb = (bend & 0x7F);

    bend = (bendMsb << 9) | (bendLsb << 2);
    bend = (int16_t)(uint16_t)(bend + 0x8000);

    octave = note / 12;
    freq = s_9x_opl_freq[note % 12];
    if(octave < 5)
        freq >>= (5 - octave);
    else if (octave > 5)
        freq <<= (octave - 5);

    freqpitched = s_9x_opl_applypitch(freq, bend);
    freqpitched *= 2;

    return (double)freqpitched;
}


enum { MasterVolumeDefault = 127 };

inline bool isXgPercChannel(uint8_t msb, uint8_t lsb)
{
    return (msb == 0x7E || msb == 0x7F) && (lsb == 0);
}

void MIDIplay::AdlChannel::addAge(int64_t us)
{
    const int64_t neg = 1000 * static_cast<int64_t>(-0x1FFFFFFFl);
    if(users.empty())
    {
        koff_time_until_neglible_us = std::max(koff_time_until_neglible_us - us, neg);
        if(koff_time_until_neglible_us < 0)
            koff_time_until_neglible_us = 0;
    }
    else
    {
        koff_time_until_neglible_us = 0;
        for(users_iterator i = users.begin(); !i.is_end(); ++i)
        {
            LocationData &d = i->value;
            if(!d.fixed_sustain)
                d.kon_time_until_neglible_us = std::max(d.kon_time_until_neglible_us - us, neg);
            d.vibdelay_us += us;
        }
    }
}

MIDIplay::MIDIplay(unsigned long sampleRate):
    m_cmfPercussionMode(false),
    m_sysExDeviceId(0),
    m_synthMode(Mode_XG),
    m_arpeggioCounter(0)
#if defined(ADLMIDI_AUDIO_TICK_HANDLER)
    , m_audioTickCounter(0)
#endif
{
    m_midiDevices.clear();

    m_setup.emulator = adl_getLowestEmulator();
    m_setup.runAtPcmRate = false;

    m_setup.PCM_RATE   = sampleRate;
    m_setup.mindelay = 1.0 / (double)m_setup.PCM_RATE;
    m_setup.maxdelay = 512.0 / (double)m_setup.PCM_RATE;

    m_setup.bankId    = 0;
    m_setup.numFourOps = -1;
    m_setup.numChips   = 2;
    m_setup.deepTremoloMode     = -1;
    m_setup.deepVibratoMode     = -1;
    m_setup.rhythmMode   = -1;
    m_setup.logarithmicVolumes  = false;
    m_setup.volumeScaleModel = ADLMIDI_VolumeModel_AUTO;
    //m_setup.SkipForward = 0;
    m_setup.scaleModulators     = -1;
    m_setup.fullRangeBrightnessCC74 = false;
    m_setup.delay = 0.0;
    m_setup.carry = 0.0;
    m_setup.tick_skip_samples_delay = 0;

    m_synth.reset(new Synth);

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    m_sequencer.reset(new MidiSequencer);
    initSequencerInterface();
#endif
    resetMIDI();
    applySetup();
    realTime_ResetState();
}

MIDIplay::~MIDIplay()
{
}

void MIDIplay::applySetup()
{
    Synth &synth = *m_synth;

    synth.m_musicMode = Synth::MODE_MIDI;

    m_setup.tick_skip_samples_delay = 0;

    synth.m_runAtPcmRate = m_setup.runAtPcmRate;

#ifndef DISABLE_EMBEDDED_BANKS
    if(synth.m_embeddedBank != Synth::CustomBankTag)
    {
        const BanksDump::BankEntry &b = g_embeddedBanks[m_setup.bankId];
        synth.m_insBankSetup.volumeModel = (b.bankSetup & 0x00FF);
        synth.m_insBankSetup.deepTremolo = (b.bankSetup >> 8 & 0x0001) != 0;
        synth.m_insBankSetup.deepVibrato = (b.bankSetup >> 8 & 0x0002) != 0;
    }
#endif

    synth.m_deepTremoloMode     = m_setup.deepTremoloMode < 0 ?
                            synth.m_insBankSetup.deepTremolo :
                            (m_setup.deepTremoloMode != 0);
    synth.m_deepVibratoMode     = m_setup.deepVibratoMode < 0 ?
                            synth.m_insBankSetup.deepVibrato :
                            (m_setup.deepVibratoMode != 0);
    synth.m_scaleModulators     = m_setup.scaleModulators < 0 ?
                            synth.m_insBankSetup.scaleModulators :
                            (m_setup.scaleModulators != 0);

    if(m_setup.logarithmicVolumes)
        synth.setVolumeScaleModel(ADLMIDI_VolumeModel_NativeOPL3);
    else
        synth.setVolumeScaleModel(static_cast<ADLMIDI_VolumeModels>(m_setup.volumeScaleModel));

    if(m_setup.volumeScaleModel == ADLMIDI_VolumeModel_AUTO)//Use bank default volume model
        synth.m_volumeScale = (Synth::VolumesScale)synth.m_insBankSetup.volumeModel;

    synth.m_numChips    = m_setup.numChips;
    m_cmfPercussionMode = false;

    if(m_setup.numFourOps >= 0)
        synth.m_numFourOps  = m_setup.numFourOps;
    else
        adlCalculateFourOpChannels(this, true);

    synth.reset(m_setup.emulator, m_setup.PCM_RATE, this);
    m_chipChannels.clear();
    m_chipChannels.resize(synth.m_numChannels);

    // Reset the arpeggio counter
    m_arpeggioCounter = 0;
}

void MIDIplay::partialReset()
{
    Synth &synth = *m_synth;
    realTime_panic();
    m_setup.tick_skip_samples_delay = 0;
    synth.m_runAtPcmRate = m_setup.runAtPcmRate;
    synth.reset(m_setup.emulator, m_setup.PCM_RATE, this);
    m_chipChannels.clear();
    m_chipChannels.resize((size_t)synth.m_numChannels);
    resetMIDIDefaults();
}

void MIDIplay::resetMIDI()
{
    Synth &synth = *m_synth;
    synth.m_masterVolume = MasterVolumeDefault;
    m_sysExDeviceId = 0;
    m_synthMode = Mode_XG;
    m_arpeggioCounter = 0;

    m_midiChannels.clear();
    m_midiChannels.resize(16, MIDIchannel());

    resetMIDIDefaults();

    caugh_missing_instruments.clear();
    caugh_missing_banks_melodic.clear();
    caugh_missing_banks_percussion.clear();
}

void MIDIplay::resetMIDIDefaults(int offset)
{
    Synth &synth = *m_synth;

    for(size_t c = offset, n = m_midiChannels.size(); c < n; ++c)
    {
        MIDIchannel &ch = m_midiChannels[c];
        if(synth.m_musicMode == Synth::MODE_XMIDI)
        {
            ch.def_volume = 127;
            ch.def_bendsense_lsb = 0;
            ch.def_bendsense_msb = 12;
        }
        else
        if(synth.m_musicMode == Synth::MODE_RSXX)
            ch.def_volume = 127;
    }
}

void MIDIplay::TickIterators(double s)
{
    Synth &synth = *m_synth;
    for(uint32_t c = 0, n = synth.m_numChannels; c < n; ++c)
    {
        AdlChannel &ch = m_chipChannels[c];
        ch.addAge(static_cast<int64_t>(s * 1e6));
    }

    // Resolve "hell of all times" of too short drum notes
    for(size_t c = 0, n = m_midiChannels.size(); c < n; ++c)
    {
        MIDIchannel &ch = m_midiChannels[c];
        if(ch.extended_note_count == 0)
            continue;

        for(MIDIchannel::notes_iterator inext = ch.activenotes.begin(); !inext.is_end();)
        {
            MIDIchannel::notes_iterator i(inext++);
            MIDIchannel::NoteInfo &ni = i->value;

            double ttl = ni.ttl;
            if(ttl <= 0)
                continue;

            ni.ttl = ttl = ttl - s;
            if(ttl <= 0)
            {
                --ch.extended_note_count;
                if(ni.isOnExtendedLifeTime)
                {
                    noteUpdate(c, i, Upd_Off);
                    ni.isOnExtendedLifeTime = false;
                }
            }
        }
    }

    updateVibrato(s);
    updateArpeggio(s);
#if !defined(ADLMIDI_AUDIO_TICK_HANDLER)
    updateGlide(s);
#endif
}

void MIDIplay::realTime_ResetState()
{
    Synth &synth = *m_synth;
    for(size_t ch = 0; ch < m_midiChannels.size(); ch++)
    {
        MIDIchannel &chan = m_midiChannels[ch];
        chan.resetAllControllers();
        chan.vibpos = 0.0;
        chan.lastlrpn = 0;
        chan.lastmrpn = 0;
        chan.nrpn = false;
        if((m_synthMode & Mode_GS) != 0)// Reset custom drum channels on GS
            chan.is_xg_percussion = false;
        noteUpdateAll(uint16_t(ch), Upd_All);
        noteUpdateAll(uint16_t(ch), Upd_Off);
    }
    synth.m_masterVolume = MasterVolumeDefault;
}

bool MIDIplay::realTime_NoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
    Synth &synth = *m_synth;

    if(note >= 128)
        note = 127;

    if((synth.m_musicMode == Synth::MODE_RSXX) && (velocity != 0))
    {
        // Check if this is just a note after-touch
        MIDIchannel::notes_iterator i = m_midiChannels[channel].find_activenote(note);
        if(!i.is_end())
        {
            MIDIchannel::NoteInfo &ni = i->value;
            const int veloffset = ni.ains->midi_velocity_offset;
            velocity = (uint8_t)std::min(127, std::max(1, (int)velocity + veloffset));
            ni.vol = velocity;
            noteUpdate(channel, i, Upd_Volume);
            return false;
        }
    }

    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    noteOff(channel, note, velocity != 0);
    // On Note on, Keyoff the note first, just in case keyoff
    // was omitted; this fixes Dance of sugar-plum fairy
    // by Microsoft. Now that we've done a Keyoff,
    // check if we still need to do a Keyon.
    // vol=0 and event 8x are both Keyoff-only.
    if(velocity == 0)
        return false;

    MIDIchannel &midiChan = m_midiChannels[channel];

    size_t midiins = midiChan.patch;
    bool isPercussion = (channel % 16 == 9) || midiChan.is_xg_percussion;

    size_t bank = (midiChan.bank_msb * 256) + midiChan.bank_lsb;

    if(isPercussion)
    {
        // == XG bank numbers ==
        // 0x7E00 - XG "SFX Kits" SFX1/SFX2 channel (16128 signed decimal)
        // 0x7F00 - XG "Drum Kits" Percussion channel (16256 signed decimal)

        // MIDI instrument defines the patch:
        if((m_synthMode & Mode_XG) != 0)
        {
            // Let XG SFX1/SFX2 bank will go in 128...255 range of LSB in WOPN file)
            // Let XG Percussion bank will use (0...127 LSB range in WOPN file)

            // Choose: SFX or Drum Kits
            bank = midiins + ((bank == 0x7E00) ? 128 : 0);
        }
        else
        {
            bank = midiins;
        }
        midiins = note; // Percussion instrument
    }

    if(isPercussion)
        bank += Synth::PercussionTag;

    const adlinsdata2 *ains = &Synth::m_emptyInstrument;

    //Set bank bank
    const Synth::Bank *bnk = NULL;
    bool caughtMissingBank = false;
    if((bank & ~static_cast<uint16_t>(Synth::PercussionTag)) > 0)
    {
        Synth::BankMap::iterator b = synth.m_insBanks.find(bank);
        if(b != synth.m_insBanks.end())
            bnk = &b->second;
        if(bnk)
            ains = &bnk->ins[midiins];
        else
            caughtMissingBank = true;
    }

    //Or fall back to bank ignoring LSB (GS)
    if((ains->flags & adlinsdata::Flag_NoSound) && ((m_synthMode & Mode_GS) != 0))
    {
        size_t fallback = bank & ~(size_t)0x7F;
        if(fallback != bank)
        {
            Synth::BankMap::iterator b = synth.m_insBanks.find(fallback);
            caughtMissingBank = false;
            if(b != synth.m_insBanks.end())
                bnk = &b->second;
            if(bnk)
                ains = &bnk->ins[midiins];
            else
                caughtMissingBank = true;
        }
    }

    if(caughtMissingBank && hooks.onDebugMessage)
    {
        std::set<size_t> &missing = (isPercussion) ?
                                    caugh_missing_banks_percussion : caugh_missing_banks_melodic;
        const char *text = (isPercussion) ?
                           "percussion" : "melodic";
        if(missing.insert(bank).second)
        {
            hooks.onDebugMessage(hooks.onDebugMessage_userData,
                                 "[%i] Playing missing %s MIDI bank %i (patch %i)",
                                 channel, text, (bank & ~static_cast<uint16_t>(Synth::PercussionTag)), midiins);
        }
    }

    //Or fall back to first bank
    if((ains->flags & adlinsdata::Flag_NoSound) != 0)
    {
        Synth::BankMap::iterator b = synth.m_insBanks.find(bank & Synth::PercussionTag);
        if(b != synth.m_insBanks.end())
            bnk = &b->second;
        if(bnk)
            ains = &bnk->ins[midiins];
    }

    const int veloffset = ains->midi_velocity_offset;
    velocity = (uint8_t)std::min(127, std::max(1, (int)velocity + veloffset));

    int32_t tone = note;
    if(!isPercussion && (bank > 0)) // For non-zero banks
    {
        if(ains->flags & adlinsdata::Flag_NoSound)
        {
            if(hooks.onDebugMessage && caugh_missing_instruments.insert(static_cast<uint8_t>(midiins)).second)
            {
                hooks.onDebugMessage(hooks.onDebugMessage_userData,
                     "[%i] Caught a blank instrument %i (offset %i) in the MIDI bank %u",
                     channel, m_midiChannels[channel].patch, midiins, bank);
            }
            bank = 0;
            midiins = midiChan.patch;
        }
    }

    if(ains->tone)
    {
        if(ains->tone >= 128)
            tone = ains->tone - 128;
        else
            tone = ains->tone;
    }

    //uint16_t i[2] = { ains->adlno1, ains->adlno2 };
    bool is_2op = !(ains->flags & (adlinsdata::Flag_Pseudo4op|adlinsdata::Flag_Real4op));
    bool pseudo_4op = ains->flags & adlinsdata::Flag_Pseudo4op;
#ifndef __WATCOMC__
    MIDIchannel::NoteInfo::Phys voices[MIDIchannel::NoteInfo::MaxNumPhysChans] =
    {
        {0, ains->adl[0], false},
        {0, (!is_2op) ? ains->adl[1] : ains->adl[0], pseudo_4op}
    };
#else /* Unfortunately, Watcom can't brace-initialize structure that incluses structure fields */
    MIDIchannel::NoteInfo::Phys voices[MIDIchannel::NoteInfo::MaxNumPhysChans];
    voices[0].chip_chan = 0;
    voices[0].ains = ains->adl[0];
    voices[0].pseudo4op = false;
    voices[1].chip_chan = 0;
    voices[1].ains = (!is_2op) ? ains->adl[1] : ains->adl[0];
    voices[1].pseudo4op = pseudo_4op;
#endif /* __WATCOMC__ */

    if(
        (synth.m_rhythmMode == 1) &&
        (
            ((ains->flags & adlinsdata::Mask_RhythmMode) != 0) ||
            (m_cmfPercussionMode && (channel >= 11))
        )
    )
    {
        voices[1] = voices[0];//i[1] = i[0];
    }

    bool isBlankNote = (ains->flags & adlinsdata::Flag_NoSound) != 0;

    if(hooks.onDebugMessage)
    {
        if(isBlankNote && caugh_missing_instruments.insert(static_cast<uint8_t>(midiins)).second)
            hooks.onDebugMessage(hooks.onDebugMessage_userData, "[%i] Playing missing instrument %i", channel, midiins);
    }

    if(isBlankNote)
    {
        // Don't even try to play the blank instrument! But, insert the dummy note.
        MIDIchannel::notes_iterator i = midiChan.ensure_find_or_create_activenote(note);
        MIDIchannel::NoteInfo &dummy = i->value;
        dummy.isBlank = true;
        dummy.isOnExtendedLifeTime = false;
        dummy.ttl = 0;
        dummy.ains = NULL;
        dummy.chip_channels_count = 0;
        // Record the last note on MIDI channel as source of portamento
        midiChan.portamentoSource = static_cast<int8_t>(note);
        return false;
    }

    // Allocate AdLib channel (the physical sound channel for the note)
    int32_t adlchannel[MIDIchannel::NoteInfo::MaxNumPhysChans] = { -1, -1 };

    for(uint32_t ccount = 0; ccount < MIDIchannel::NoteInfo::MaxNumPhysChans; ++ccount)
    {
        if(ccount == 1)
        {
            if(voices[0] == voices[1])
                break; // No secondary channel
            if(adlchannel[0] == -1)
                break; // No secondary if primary failed
        }

        int32_t c = -1;
        int32_t bs = -0x7FFFFFFFl;

        for(size_t a = 0; a < (size_t)synth.m_numChannels; ++a)
        {
            if(ccount == 1 && static_cast<int32_t>(a) == adlchannel[0]) continue;
            // ^ Don't use the same channel for primary&secondary

            if(is_2op || pseudo_4op)
            {
                // Only use regular channels
                uint32_t expected_mode = 0;

                if(synth.m_rhythmMode)
                {
                    if(m_cmfPercussionMode)
                    {
                        expected_mode = channel  < 11 ? OPL3::ChanCat_Regular : (OPL3::ChanCat_Rhythm_Bass + (channel  - 11)); // CMF
                    }
                    else
                    {
                        expected_mode = OPL3::ChanCat_Regular;
                        uint32_t rm = (ains->flags & adlinsdata::Mask_RhythmMode);
                        if(rm == adlinsdata::Flag_RM_BassDrum)
                            expected_mode = OPL3::ChanCat_Rhythm_Bass;
                        else if(rm == adlinsdata::Flag_RM_Snare)
                            expected_mode = OPL3::ChanCat_Rhythm_Snare;
                        else if(rm == adlinsdata::Flag_RM_TomTom)
                            expected_mode = OPL3::ChanCat_Rhythm_Tom;
                        else if(rm == adlinsdata::Flag_RM_Cymbal)
                            expected_mode = OPL3::ChanCat_Rhythm_Cymbal;
                        else if(rm == adlinsdata::Flag_RM_HiHat)
                            expected_mode = OPL3::ChanCat_Rhythm_HiHat;
                    }
                }

                if(synth.m_channelCategory[a] != expected_mode)
                    continue;
            }
            else
            {
                if(ccount == 0)
                {
                    // Only use four-op master channels
                    if(synth.m_channelCategory[a] != Synth::ChanCat_4op_Master)
                        continue;
                }
                else
                {
                    // The secondary must be played on a specific channel.
                    if(a != static_cast<uint32_t>(adlchannel[0]) + 3)
                        continue;
                }
            }

            int64_t s = calculateChipChannelGoodness(a, voices[ccount]);
            if(s > bs)
            {
                bs = (int32_t)s;    // Best candidate wins
                c = static_cast<int32_t>(a);
            }
        }

        if(c < 0)
        {
            if(hooks.onDebugMessage)
                hooks.onDebugMessage(hooks.onDebugMessage_userData,
                                     "ignored unplaceable note [bank %i, inst %i, note %i, MIDI channel %i]",
                                     bank, midiChan.patch, note, channel);
            continue; // Could not play this note. Ignore it.
        }

        prepareChipChannelForNewNote(static_cast<size_t>(c), voices[ccount]);
        adlchannel[ccount] = c;
    }

    if(adlchannel[0] < 0 && adlchannel[1] < 0)
    {
        // The note could not be played, at all.
        return false;
    }

    //if(hooks.onDebugMessage)
    //    hooks.onDebugMessage(hooks.onDebugMessage_userData, "i1=%d:%d, i2=%d:%d", i[0],adlchannel[0], i[1],adlchannel[1]);

    if(midiChan.softPedal) // Apply Soft Pedal level reducing
        velocity = static_cast<uint8_t>(std::floor(static_cast<float>(velocity) * 0.8f));

    // Allocate active note for MIDI channel
    MIDIchannel::notes_iterator ir = midiChan.ensure_find_or_create_activenote(note);
    MIDIchannel::NoteInfo &ni = ir->value;
    ni.vol     = velocity;
    ni.vibrato = midiChan.noteAftertouch[note];
    ni.noteTone = static_cast<int16_t>(tone);
    ni.currentTone = tone;
    ni.glideRate = HUGE_VAL;
    ni.midiins = midiins;
    ni.isPercussion = isPercussion;
    ni.isBlank = isBlankNote;
    ni.isOnExtendedLifeTime = false;
    ni.ttl = 0;
    ni.ains = ains;
    ni.chip_channels_count = 0;

    int8_t currentPortamentoSource = midiChan.portamentoSource;
    double currentPortamentoRate = midiChan.portamentoRate;
    bool portamentoEnable =
        midiChan.portamentoEnable && currentPortamentoRate != HUGE_VAL && !isPercussion;
    // Record the last note on MIDI channel as source of portamento
    midiChan.portamentoSource = static_cast<int8_t>(note);
    // midiChan.portamentoSource = portamentoEnable ? (int8_t)note : (int8_t)-1;

    // Enable gliding on portamento note
    if (portamentoEnable && currentPortamentoSource >= 0)
    {
        ni.currentTone = currentPortamentoSource;
        ni.glideRate = currentPortamentoRate;
        ++midiChan.gliding_note_count;
    }

    // Enable life time extension on percussion note
    if (isPercussion)
    {
        ni.ttl = s_drum_note_min_time;
        ++midiChan.extended_note_count;
    }

    for(unsigned ccount = 0; ccount < MIDIchannel::NoteInfo::MaxNumPhysChans; ++ccount)
    {
        int32_t c = adlchannel[ccount];
        if(c < 0)
            continue;
        uint16_t chipChan = static_cast<uint16_t>(adlchannel[ccount]);
        ni.phys_ensure_find_or_create(chipChan)->assign(voices[ccount]);
    }

    noteUpdate(channel, ir, Upd_All | Upd_Patch);

    for(unsigned ccount = 0; ccount < MIDIchannel::NoteInfo::MaxNumPhysChans; ++ccount)
    {
        int32_t c = adlchannel[ccount];
        if(c < 0)
            continue;
        m_chipChannels[c].recent_ins = voices[ccount];
        m_chipChannels[c].addAge(0);
    }

    return true;
}

void MIDIplay::realTime_NoteOff(uint8_t channel, uint8_t note)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    noteOff(channel, note);
}

void MIDIplay::realTime_NoteAfterTouch(uint8_t channel, uint8_t note, uint8_t atVal)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    MIDIchannel &chan = m_midiChannels[channel];
    MIDIchannel::notes_iterator i = m_midiChannels[channel].find_activenote(note);
    if(!i.is_end())
    {
        i->value.vibrato = atVal;
    }

    uint8_t oldAtVal = chan.noteAftertouch[note % 128];
    if(atVal != oldAtVal)
    {
        chan.noteAftertouch[note % 128] = atVal;
        bool inUse = atVal != 0;
        for(unsigned n = 0; !inUse && n < 128; ++n)
            inUse = chan.noteAftertouch[n] != 0;
        chan.noteAfterTouchInUse = inUse;
    }
}

void MIDIplay::realTime_ChannelAfterTouch(uint8_t channel, uint8_t atVal)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    m_midiChannels[channel].aftertouch = atVal;
}

void MIDIplay::realTime_Controller(uint8_t channel, uint8_t type, uint8_t value)
{
    Synth &synth = *m_synth;

    if(value > 127) // Allowed values 0~127 only
        value = 127;

    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;

    switch(type)
    {
    case 1: // Adjust vibrato
        //UI.PrintLn("%u:vibrato %d", MidCh,value);
        m_midiChannels[channel].vibrato = value;
        break;

    case 0: // Set bank msb (GM bank)
        m_midiChannels[channel].bank_msb = value;
        if((m_synthMode & Mode_GS) == 0)// Don't use XG drums on GS synth mode
            m_midiChannels[channel].is_xg_percussion = isXgPercChannel(m_midiChannels[channel].bank_msb, m_midiChannels[channel].bank_lsb);
        break;

    case 32: // Set bank lsb (XG bank)
        m_midiChannels[channel].bank_lsb = value;
        if((m_synthMode & Mode_GS) == 0)// Don't use XG drums on GS synth mode
            m_midiChannels[channel].is_xg_percussion = isXgPercChannel(m_midiChannels[channel].bank_msb, m_midiChannels[channel].bank_lsb);
        break;

    case 5: // Set portamento msb
        m_midiChannels[channel].portamento = static_cast<uint16_t>((m_midiChannels[channel].portamento & 0x007F) | (value << 7));
        updatePortamento(channel);
        break;

    case 37: // Set portamento lsb
        m_midiChannels[channel].portamento = static_cast<uint16_t>((m_midiChannels[channel].portamento & 0x3F80) | (value));
        updatePortamento(channel);
        break;

    case 65: // Enable/disable portamento
        m_midiChannels[channel].portamentoEnable = value >= 64;
        updatePortamento(channel);
        break;

    case 7: // Change volume
        m_midiChannels[channel].volume = value;
        noteUpdateAll(channel, Upd_Volume);
        break;

    case 74: // Change brightness
        m_midiChannels[channel].brightness = value;
        noteUpdateAll(channel, Upd_Volume);
        break;

    case 64: // Enable/disable sustain
        m_midiChannels[channel].sustain = (value >= 64);
        if(!m_midiChannels[channel].sustain)
            killSustainingNotes(channel, -1, AdlChannel::LocationData::Sustain_Pedal);
        break;

    case 66: // Enable/disable sostenuto
        if(value >= 64) //Find notes and mark them as sostenutoed
            markSostenutoNotes(channel);
        else
            killSustainingNotes(channel, -1, AdlChannel::LocationData::Sustain_Sostenuto);
        break;

    case 67: // Enable/disable soft-pedal
        m_midiChannels[channel].softPedal = (value >= 64);
        break;

    case 11: // Change expression (another volume factor)
        m_midiChannels[channel].expression = value;
        noteUpdateAll(channel, Upd_Volume);
        break;

    case 10: // Change panning
        m_midiChannels[channel].panning = value;

        noteUpdateAll(channel, Upd_Pan);
        break;

    case 121: // Reset all controllers
        m_midiChannels[channel].resetAllControllers121();
        noteUpdateAll(channel, Upd_Pan + Upd_Volume + Upd_Pitch);
        // Kill all sustained notes
        killSustainingNotes(channel, -1, AdlChannel::LocationData::Sustain_ANY);
        break;

    case 120: // All sounds off
        noteUpdateAll(channel, Upd_OffMute);
        break;

    case 123: // All notes off
        noteUpdateAll(channel, Upd_Off);
        break;

    case 91:
        break; // Reverb effect depth. We don't do per-channel reverb.

    case 92:
        break; // Tremolo effect depth. We don't do...

    case 93:
        break; // Chorus effect depth. We don't do.

    case 94:
        break; // Celeste effect depth. We don't do.

    case 95:
        break; // Phaser effect depth. We don't do.

    case 98:
        m_midiChannels[channel].lastlrpn = value;
        m_midiChannels[channel].nrpn = true;
        break;

    case 99:
        m_midiChannels[channel].lastmrpn = value;
        m_midiChannels[channel].nrpn = true;
        break;

    case 100:
        m_midiChannels[channel].lastlrpn = value;
        m_midiChannels[channel].nrpn = false;
        break;

    case 101:
        m_midiChannels[channel].lastmrpn = value;
        m_midiChannels[channel].nrpn = false;
        break;

    case 113:
        break; // Related to pitch-bender, used by missimp.mid in Duke3D

    case  6:
        setRPN(channel, value, true);
        break;

    case 38:
        setRPN(channel, value, false);
        break;

    case 103:
        if(synth.m_musicMode == Synth::MODE_CMF)
            m_cmfPercussionMode = (value != 0);
        break; // CMF (ctrl 0x67) rhythm mode

    default:
        break;
        //UI.PrintLn("Ctrl %d <- %d (ch %u)", ctrlno, value, MidCh);
    }
}

void MIDIplay::realTime_PatchChange(uint8_t channel, uint8_t patch)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    m_midiChannels[channel].patch = patch;
}

void MIDIplay::realTime_PitchBend(uint8_t channel, uint16_t pitch)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    m_midiChannels[channel].bend = int(pitch) - 8192;
    noteUpdateAll(channel, Upd_Pitch);
}

void MIDIplay::realTime_PitchBend(uint8_t channel, uint8_t msb, uint8_t lsb)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    m_midiChannels[channel].bend = int(lsb) + int(msb) * 128 - 8192;
    noteUpdateAll(channel, Upd_Pitch);
}

void MIDIplay::realTime_BankChangeLSB(uint8_t channel, uint8_t lsb)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    m_midiChannels[channel].bank_lsb = lsb;
}

void MIDIplay::realTime_BankChangeMSB(uint8_t channel, uint8_t msb)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    m_midiChannels[channel].bank_msb = msb;
}

void MIDIplay::realTime_BankChange(uint8_t channel, uint16_t bank)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    m_midiChannels[channel].bank_lsb = uint8_t(bank & 0xFF);
    m_midiChannels[channel].bank_msb = uint8_t((bank >> 8) & 0xFF);
}

void MIDIplay::setDeviceId(uint8_t id)
{
    m_sysExDeviceId = id;
}

bool MIDIplay::realTime_SysEx(const uint8_t *msg, size_t size)
{
    if(size < 4 || msg[0] != 0xF0 || msg[size - 1] != 0xF7)
        return false;

    unsigned manufacturer = msg[1];
    unsigned dev = msg[2];
    msg += 3;
    size -= 4;

    switch(manufacturer)
    {
    default:
        break;
    case Manufacturer_UniversalNonRealtime:
    case Manufacturer_UniversalRealtime:
        return doUniversalSysEx(
            dev, manufacturer == Manufacturer_UniversalRealtime, msg, size);
    case Manufacturer_Roland:
        return doRolandSysEx(dev, msg, size);
    case Manufacturer_Yamaha:
        return doYamahaSysEx(dev, msg, size);
    }

    return false;
}

bool MIDIplay::doUniversalSysEx(unsigned dev, bool realtime, const uint8_t *data, size_t size)
{
    bool devicematch = dev == 0x7F || dev == m_sysExDeviceId;
    if(size < 2 || !devicematch)
        return false;

    unsigned address =
        (((unsigned)data[0] & 0x7F) << 8) |
        (((unsigned)data[1] & 0x7F));
    data += 2;
    size -= 2;

    switch(((unsigned)realtime << 16) | address)
    {
        case (0 << 16) | 0x0901: // GM System On
            if(hooks.onDebugMessage)
                hooks.onDebugMessage(hooks.onDebugMessage_userData, "SysEx: GM System On");
            m_synthMode = Mode_GM;
            realTime_ResetState();
            return true;
        case (0 << 16) | 0x0902: // GM System Off
            if(hooks.onDebugMessage)
                hooks.onDebugMessage(hooks.onDebugMessage_userData, "SysEx: GM System Off");
            m_synthMode = Mode_XG;//TODO: TEMPORARY, make something RIGHT
            realTime_ResetState();
            return true;
        case (1 << 16) | 0x0401: // MIDI Master Volume
            if(size != 2)
                break;
            unsigned volume =
                (((unsigned)data[0] & 0x7F)) |
                (((unsigned)data[1] & 0x7F) << 7);
            if(m_synth.get())
                m_synth->m_masterVolume = static_cast<uint8_t>(volume >> 7);
            for(size_t ch = 0; ch < m_midiChannels.size(); ch++)
                noteUpdateAll(uint16_t(ch), Upd_Volume);
            return true;
    }

    return false;
}

bool MIDIplay::doRolandSysEx(unsigned dev, const uint8_t *data, size_t size)
{
    bool devicematch = dev == 0x7F || (dev & 0x0F) == m_sysExDeviceId;
    if(size < 6 || !devicematch)
        return false;

    unsigned model = data[0] & 0x7F;
    unsigned mode = data[1] & 0x7F;
    unsigned checksum = data[size - 1] & 0x7F;
    data += 2;
    size -= 3;

#if !defined(ADLMIDI_SKIP_ROLAND_CHECKSUM)
    {
        unsigned checkvalue = 0;
        for(size_t i = 0; i < size; ++i)
            checkvalue += data[i] & 0x7F;
        checkvalue = (128 - (checkvalue & 127)) & 127;
        if(checkvalue != checksum)
        {
            if(hooks.onDebugMessage)
                hooks.onDebugMessage(hooks.onDebugMessage_userData, "SysEx: Caught invalid roland SysEx message!");
            return false;
        }
    }
#endif

    unsigned address =
        (((unsigned)data[0] & 0x7F) << 16) |
        (((unsigned)data[1] & 0x7F) << 8)  |
        (((unsigned)data[2] & 0x7F));
    unsigned target_channel = 0;

    /* F0 41 10 42 12 40 00 7F 00 41 F7 */

    if((address & 0xFFF0FF) == 0x401015) // Turn channel 1 into percussion
    {
        address = 0x401015;
        target_channel = data[1] & 0x0F;
    }

    data += 3;
    size -= 3;

    if(mode != RolandMode_Send) // don't have MIDI-Out reply ability
        return false;

    // Mode Set
    // F0 {41 10 42 12} {40 00 7F} {00 41} F7

    // Custom drum channels
    // F0 {41 10 42 12} {40 1<ch> 15} {<state> <sum>} F7

    switch((model << 24) | address)
    {
    case (RolandModel_GS << 24) | 0x00007F: // System Mode Set
    {
        if(size != 1 || (dev & 0xF0) != 0x10)
            break;
        unsigned mode = data[0] & 0x7F;
        ADL_UNUSED(mode);//TODO: Hook this correctly!
        if(hooks.onDebugMessage)
            hooks.onDebugMessage(hooks.onDebugMessage_userData, "SysEx: Caught Roland System Mode Set: %02X", mode);
        m_synthMode = Mode_GS;
        realTime_ResetState();
        return true;
    }
    case (RolandModel_GS << 24) | 0x40007F: // Mode Set
    {
        if(size != 1 || (dev & 0xF0) != 0x10)
            break;
        unsigned value = data[0] & 0x7F;
        ADL_UNUSED(value);//TODO: Hook this correctly!
        if(hooks.onDebugMessage)
            hooks.onDebugMessage(hooks.onDebugMessage_userData, "SysEx: Caught Roland Mode Set: %02X", value);
        m_synthMode = Mode_GS;
        realTime_ResetState();
        return true;
    }
    case (RolandModel_GS << 24) | 0x401015: // Percussion channel
    {
        if(size != 1 || (dev & 0xF0) != 0x10)
            break;
        if(m_midiChannels.size() < 16)
            break;
        unsigned value = data[0] & 0x7F;
        const uint8_t channels_map[16] =
        {
            9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15
        };
        if(hooks.onDebugMessage)
            hooks.onDebugMessage(hooks.onDebugMessage_userData,
                                 "SysEx: Caught Roland Percussion set: %02X on channel %u (from %X)",
                                 value, channels_map[target_channel], target_channel);
        m_midiChannels[channels_map[target_channel]].is_xg_percussion = ((value == 0x01)) || ((value == 0x02));
        return true;
    }
    }

    return false;
}

bool MIDIplay::doYamahaSysEx(unsigned dev, const uint8_t *data, size_t size)
{
    bool devicematch = dev == 0x7F || (dev & 0x0F) == m_sysExDeviceId;
    if(size < 1 || !devicematch)
        return false;

    unsigned model = data[0] & 0x7F;
    ++data;
    --size;

    switch((model << 8) | (dev & 0xF0))
    {
    case (YamahaModel_XG << 8) | 0x10:  // parameter change
    {
        if(size < 3)
            break;

        unsigned address =
            (((unsigned)data[0] & 0x7F) << 16) |
            (((unsigned)data[1] & 0x7F) << 8)  |
            (((unsigned)data[2] & 0x7F));
        data += 3;
        size -= 3;

        switch(address)
        {
        case 0x00007E:  // XG System On
            if(size != 1)
                break;
            unsigned value = data[0] & 0x7F;
            ADL_UNUSED(value);//TODO: Hook this correctly!
            if(hooks.onDebugMessage)
                hooks.onDebugMessage(hooks.onDebugMessage_userData, "SysEx: Caught Yamaha XG System On: %02X", value);
            m_synthMode = Mode_XG;
            realTime_ResetState();
            return true;
        }

        break;
    }
    }

    return false;
}

void MIDIplay::realTime_panic()
{
    panic();
    killSustainingNotes(-1, -1, AdlChannel::LocationData::Sustain_ANY);
}

void MIDIplay::realTime_deviceSwitch(size_t track, const char *data, size_t length)
{
    const std::string indata(data, length);
    m_currentMidiDevice[track] = chooseDevice(indata);
}

size_t MIDIplay::realTime_currentDevice(size_t track)
{
    if(m_currentMidiDevice.empty())
        return 0;
    return m_currentMidiDevice[track];
}

void MIDIplay::realTime_rawOPL(uint8_t reg, uint8_t value)
{
    Synth &synth = *m_synth;
    if((reg & 0xF0) == 0xC0)
        value |= 0x30;
    //std::printf("OPL poke %02X, %02X\n", reg, value);
    //std::fflush(stdout);
    synth.writeReg(0, reg, value);
}

#if defined(ADLMIDI_AUDIO_TICK_HANDLER)
void MIDIplay::AudioTick(uint32_t chipId, uint32_t rate)
{
    if(chipId != 0)  // do first chip ticks only
        return;

    uint32_t tickNumber = m_audioTickCounter++;
    double timeDelta = 1.0 / rate;

    enum { portamentoInterval = 32 }; // for efficiency, set rate limit on pitch updates

    if(tickNumber % portamentoInterval == 0)
    {
        double portamentoDelta = timeDelta * portamentoInterval;
        updateGlide(portamentoDelta);
    }
}
#endif

void MIDIplay::noteUpdate(size_t midCh,
                          MIDIplay::MIDIchannel::notes_iterator i,
                          unsigned props_mask,
                          int32_t select_adlchn)
{
    Synth &synth = *m_synth;
    MIDIchannel::NoteInfo &info = i->value;
    const int16_t noteTone    = info.noteTone;
    const double currentTone    = info.currentTone;
    const uint8_t vol     = info.vol;
    const int midiins     = static_cast<int>(info.midiins);
    const adlinsdata2 &ains = *info.ains;
    AdlChannel::Location my_loc;
    my_loc.MidCh = static_cast<uint16_t>(midCh);
    my_loc.note  = info.note;

    if(info.isBlank)
    {
        if(props_mask & Upd_Off)
            m_midiChannels[midCh].activenotes.erase(i);
        return;
    }

    for(unsigned ccount = 0, ctotal = info.chip_channels_count; ccount < ctotal; ccount++)
    {
        const MIDIchannel::NoteInfo::Phys &ins = info.chip_channels[ccount];
        uint16_t c   = ins.chip_chan;

        if(select_adlchn >= 0 && c != select_adlchn) continue;

        if(props_mask & Upd_Patch)
        {
            synth.setPatch(c, ins.ains);
            AdlChannel::users_iterator i = m_chipChannels[c].find_or_create_user(my_loc);
            if(!i.is_end())    // inserts if necessary
            {
                AdlChannel::LocationData &d = i->value;
                d.sustained = AdlChannel::LocationData::Sustain_None;
                d.vibdelay_us = 0;
                d.fixed_sustain = (ains.ms_sound_kon == static_cast<uint16_t>(adlNoteOnMaxTime));
                d.kon_time_until_neglible_us = 1000 * ains.ms_sound_kon;
                d.ins       = ins;
            }
        }
    }

    for(unsigned ccount = 0; ccount < info.chip_channels_count; ccount++)
    {
        const MIDIchannel::NoteInfo::Phys &ins = info.chip_channels[ccount];
        uint16_t c          = ins.chip_chan;
        uint16_t c_slave    = info.chip_channels[1].chip_chan;

        if(select_adlchn >= 0 && c != select_adlchn)
            continue;

        if(props_mask & Upd_Off) // note off
        {
            if(!m_midiChannels[midCh].sustain)
            {
                AdlChannel::users_iterator k = m_chipChannels[c].find_user(my_loc);
                bool do_erase_user = (!k.is_end() && ((k->value.sustained & AdlChannel::LocationData::Sustain_Sostenuto) == 0));
                if(do_erase_user)
                    m_chipChannels[c].users.erase(k);

                if(hooks.onNote)
                    hooks.onNote(hooks.onNote_userData, c, noteTone, midiins, 0, 0.0);

                if(do_erase_user && m_chipChannels[c].users.empty())
                {
                    synth.noteOff(c);
                    if(props_mask & Upd_Mute) // Mute the note
                    {
                        synth.touchNote(c, 0, 0, 0);
                        m_chipChannels[c].koff_time_until_neglible_us = 0;
                    }
                    else
                    {
                        m_chipChannels[c].koff_time_until_neglible_us = 1000 * int64_t(ains.ms_sound_koff);
                    }
                }
            }
            else
            {
                // Sustain: Forget about the note, but don't key it off.
                //          Also will avoid overwriting it very soon.
                AdlChannel::users_iterator d = m_chipChannels[c].find_or_create_user(my_loc);
                if(!d.is_end())
                    d->value.sustained |= AdlChannel::LocationData::Sustain_Pedal; // note: not erased!
                if(hooks.onNote)
                    hooks.onNote(hooks.onNote_userData, c, noteTone, midiins, -1, 0.0);
            }

            info.phys_erase_at(&ins);  // decrements channel count
            --ccount;  // adjusts index accordingly
            continue;
        }

        if(props_mask & Upd_Pan)
            synth.setPan(c, m_midiChannels[midCh].panning);

        if(props_mask & Upd_Volume)
        {
            const MIDIchannel &ch = m_midiChannels[midCh];
            bool is_percussion = (midCh == 9) || ch.is_xg_percussion;
            uint_fast32_t brightness = is_percussion ? 127 : ch.brightness;

            if(!m_setup.fullRangeBrightnessCC74)
            {
                // Simulate post-High-Pass filter result which affects sounding by half level only
                if(brightness >= 64)
                    brightness = 127;
                else
                    brightness *= 2;
            }

            synth.touchNote(c,
                            vol,
                            ch.volume,
                            ch.expression,
                            static_cast<uint8_t>(brightness));

            /* DEBUG ONLY!!!
            static uint32_t max = 0;

            if(volume == 0)
                max = 0;

            if(volume > max)
                max = volume;

            printf("%d\n", max);
            fflush(stdout);
            */
        }

        if(props_mask & Upd_Pitch)
        {
            AdlChannel::users_iterator d = m_chipChannels[c].find_user(my_loc);

            // Don't bend a sustained note
            if(d.is_end() || (d->value.sustained == AdlChannel::LocationData::Sustain_None))
            {
                MIDIchannel &chan = m_midiChannels[midCh];
                double midibend = chan.bend * chan.bendsense;
                double bend = midibend + ins.ains.finetune;
                double phase = 0.0;
                uint8_t vibrato = std::max(chan.vibrato, chan.aftertouch);
                double finalFreq;

                vibrato = std::max(vibrato, info.vibrato);

                if((ains.flags & adlinsdata::Flag_Pseudo4op) && ins.pseudo4op)
                {
                    phase = ains.voice2_fine_tune;//0.125; // Detune the note slightly (this is what Doom does)
                }

                if(vibrato && (d.is_end() || d->value.vibdelay_us >= chan.vibdelay_us))
                    bend += static_cast<double>(vibrato) * chan.vibdepth * std::sin(chan.vibpos);

                bend += phase;

                // Use different frequency formulas in depend on a volume model
                switch(synth.m_volumeScale)
                {
                case Synth::VOLUME_DMX:
                case Synth::VOLUME_DMX_FIXED:
                    finalFreq = s_dmxFreq(currentTone, bend);
                    break;

                case Synth::VOLUME_APOGEE:
                case Synth::VOLUME_APOGEE_FIXED:
                    finalFreq = s_apogeeFreq(currentTone, bend);
                    break;

                case Synth::VOLUME_9X:
                case Synth::VOLUME_9X_GENERIC_FM:
                    finalFreq = s_9xFreq(currentTone, bend);
                    break;

                default:
                    finalFreq = s_commonFreq(currentTone, bend);
                }

                synth.noteOn(c, c_slave, finalFreq);

                if(hooks.onNote)
                    hooks.onNote(hooks.onNote_userData, c, noteTone, midiins, vol, midibend);
            }
        }
    }

    if(info.chip_channels_count == 0)
    {
        m_midiChannels[midCh].cleanupNote(i);
        m_midiChannels[midCh].activenotes.erase(i);
    }
}

void MIDIplay::noteUpdateAll(size_t midCh, unsigned props_mask)
{
    for(MIDIchannel::notes_iterator
        i = m_midiChannels[midCh].activenotes.begin(); !i.is_end();)
    {
        MIDIchannel::notes_iterator j(i++);
        noteUpdate(midCh, j, props_mask);
    }
}

const std::string &MIDIplay::getErrorString()
{
    return errorStringOut;
}

void MIDIplay::setErrorString(const std::string &err)
{
    errorStringOut = err;
}

int64_t MIDIplay::calculateChipChannelGoodness(size_t c, const MIDIchannel::NoteInfo::Phys &ins) const
{
    Synth &synth = *m_synth;
    const AdlChannel &chan = m_chipChannels[c];
    int64_t koff_ms = chan.koff_time_until_neglible_us / 1000;
    int64_t s = -koff_ms;

    // Rate channel with a releasing note
    if(s < 0 && chan.users.empty())
    {
        bool isSame = (chan.recent_ins == ins);
        s -= 40000;

        // If it's same instrument, better chance to get it when no free channels
        if(synth.m_musicMode == Synth::MODE_CMF)
        {
            if(isSame)
                s = 0; // Re-use releasing channel with the same instrument
        }
        else if(synth.m_volumeScale == Synth::VOLUME_HMI)
        {
            s = 0; // HMI doesn't care about the same instrument
        }
        else
        {
            if(isSame)
                s =  -koff_ms; // Wait until releasing sound will complete
        }

        return s;
    }

    // Same midi-instrument = some stability
    for(AdlChannel::const_users_iterator j = chan.users.begin(); !j.is_end(); ++j)
    {
        const AdlChannel::LocationData &jd = j->value;

        int64_t kon_ms = jd.kon_time_until_neglible_us / 1000;
        s -= (jd.sustained == AdlChannel::LocationData::Sustain_None) ?
            (4000000 + kon_ms) : (500000 + (kon_ms / 2));

        MIDIchannel::notes_iterator
        k = const_cast<MIDIchannel &>(m_midiChannels[jd.loc.MidCh]).find_activenote(jd.loc.note);

        if(!k.is_end())
        {
            const MIDIchannel::NoteInfo &info = k->value;

            // Same instrument = good
            if(jd.ins == ins)
            {
                s += 300;
                // Arpeggio candidate = even better
                if(jd.vibdelay_us < 70000
                   || jd.kon_time_until_neglible_us > 20000000)
                    s += 10;
            }

            // Percussion is inferior to melody
            s += info.isPercussion ? 50 : 0;
            /*
                    if(k->second.midiins >= 25
                    && k->second.midiins < 40
                    && j->second.ins != ins)
                    {
                        s -= 14000; // HACK: Don't clobber the bass or the guitar
                    }
                    */
        }

        // If there is another channel to which this note
        // can be evacuated to in the case of congestion,
        // increase the score slightly.
        unsigned n_evacuation_stations = 0;

        for(size_t c2 = 0; c2 < static_cast<size_t>(synth.m_numChannels); ++c2)
        {
            if(c2 == c) continue;

            if(synth.m_channelCategory[c2]
               != synth.m_channelCategory[c]) continue;

            for(AdlChannel::const_users_iterator m = m_chipChannels[c2].users.begin(); !m.is_end(); ++m)
            {
                const AdlChannel::LocationData &md = m->value;
                if(md.sustained != AdlChannel::LocationData::Sustain_None) continue;
                if(md.vibdelay_us >= 200000) continue;
                if(md.ins != jd.ins) continue;
                n_evacuation_stations += 1;
            }
        }

        s += (int64_t)n_evacuation_stations * 4;
    }

    return s;
}


void MIDIplay::prepareChipChannelForNewNote(size_t c, const MIDIchannel::NoteInfo::Phys &ins)
{
    if(m_chipChannels[c].users.empty()) return; // Nothing to do

    Synth &synth = *m_synth;

    //bool doing_arpeggio = false;
    for(AdlChannel::users_iterator jnext = m_chipChannels[c].users.begin(); !jnext.is_end();)
    {
        AdlChannel::users_iterator j = jnext;
        AdlChannel::LocationData &jd = jnext->value;
        ++jnext;

        if(jd.sustained == AdlChannel::LocationData::Sustain_None)
        {
            // Collision: Kill old note,
            // UNLESS we're going to do arpeggio
            MIDIchannel::notes_iterator i
            (m_midiChannels[jd.loc.MidCh].ensure_find_activenote(jd.loc.note));

            // Check if we can do arpeggio.
            if((jd.vibdelay_us < 70000
                || jd.kon_time_until_neglible_us > 20000000)
               && jd.ins == ins)
            {
                // Do arpeggio together with this note.
                //doing_arpeggio = true;
                continue;
            }

            killOrEvacuate(c, j, i);
            // ^ will also erase j from ch[c].users.
        }
    }

    // Kill all sustained notes on this channel
    // Don't keep them for arpeggio, because arpeggio requires
    // an intact "activenotes" record. This is a design flaw.
    killSustainingNotes(-1, static_cast<int32_t>(c), AdlChannel::LocationData::Sustain_ANY);

    // Keyoff the channel so that it can be retriggered,
    // unless the new note will be introduced as just an arpeggio.
    if(m_chipChannels[c].users.empty())
        synth.noteOff(c);
}

void MIDIplay::killOrEvacuate(size_t from_channel,
                              AdlChannel::users_iterator j,
                              MIDIplay::MIDIchannel::notes_iterator i)
{
    Synth &synth = *m_synth;
    uint32_t maxChannels = ADL_MAX_CHIPS * 18;
    AdlChannel::LocationData &jd = j->value;
    MIDIchannel::NoteInfo &info = i->value;

    // Before killing the note, check if it can be
    // evacuated to another channel as an arpeggio
    // instrument. This helps if e.g. all channels
    // are full of strings and we want to do percussion.
    // FIXME: This does not care about four-op entanglements.
    for(uint32_t c = 0; c < synth.m_numChannels; ++c)
    {
        uint16_t cs = static_cast<uint16_t>(c);

        if(c >= maxChannels)
            break;
        if(c == from_channel)
            continue;
        if(synth.m_channelCategory[c] != synth.m_channelCategory[from_channel])
            continue;

        AdlChannel &adlch = m_chipChannels[c];
        if(adlch.users.size() == adlch.users.capacity())
            continue;  // no room for more arpeggio on channel

        if(!m_chipChannels[cs].find_user(jd.loc).is_end())
            continue;  // channel already has this note playing (sustained)
                       // avoid introducing a duplicate location.

        for(AdlChannel::users_iterator m = adlch.users.begin(); !m.is_end(); ++m)
        {
            AdlChannel::LocationData &mv = m->value;

            if(mv.vibdelay_us >= 200000
               && mv.kon_time_until_neglible_us < 10000000) continue;
            if(mv.ins != jd.ins)
                continue;
            if(hooks.onNote)
            {
                hooks.onNote(hooks.onNote_userData,
                             (int)from_channel,
                             info.noteTone,
                             static_cast<int>(info.midiins), 0, 0.0);
                hooks.onNote(hooks.onNote_userData,
                             (int)c,
                             info.noteTone,
                             static_cast<int>(info.midiins),
                             info.vol, 0.0);
            }

            info.phys_erase(static_cast<uint16_t>(from_channel));
            info.phys_ensure_find_or_create(cs)->assign(jd.ins);
            m_chipChannels[cs].users.push_back(jd);
            m_chipChannels[from_channel].users.erase(j);
            return;
        }
    }

    /*UI.PrintLn(
                "collision @%u: [%ld] <- ins[%3u]",
                c,
                //ch[c].midiins<128?'M':'P', ch[c].midiins&127,
                ch[c].age, //adlins[ch[c].insmeta].ms_sound_kon,
                ins
                );*/
    // Kill it
    noteUpdate(jd.loc.MidCh,
               i,
               Upd_Off,
               static_cast<int32_t>(from_channel));
}

void MIDIplay::panic()
{
    for(uint8_t chan = 0; chan < m_midiChannels.size(); chan++)
    {
        for(uint8_t note = 0; note < 128; note++)
            realTime_NoteOff(chan, note);
    }
}

void MIDIplay::killSustainingNotes(int32_t midCh, int32_t this_adlchn, uint32_t sustain_type)
{
    Synth &synth = *m_synth;
    uint32_t first = 0, last = synth.m_numChannels;

    if(this_adlchn >= 0)
    {
        first = static_cast<uint32_t>(this_adlchn);
        last = first + 1;
    }

    for(uint32_t c = first; c < last; ++c)
    {
        if(m_chipChannels[c].users.empty())
            continue; // Nothing to do

        for(AdlChannel::users_iterator jnext = m_chipChannels[c].users.begin(); !jnext.is_end();)
        {
            AdlChannel::users_iterator j = jnext;
            AdlChannel::LocationData &jd = j->value;
            ++jnext;

            if((midCh < 0 || jd.loc.MidCh == midCh)
                && ((jd.sustained & sustain_type) != 0))
            {
                int midiins = '?';
                if(hooks.onNote)
                    hooks.onNote(hooks.onNote_userData, (int)c, jd.loc.note, midiins, 0, 0.0);
                jd.sustained &= ~sustain_type;
                if(jd.sustained == AdlChannel::LocationData::Sustain_None)
                    m_chipChannels[c].users.erase(j);//Remove only when note is clean from any holders
            }
        }

        // Keyoff the channel, if there are no users left.
        if(m_chipChannels[c].users.empty())
            synth.noteOff(c);
    }
}

void MIDIplay::markSostenutoNotes(int32_t midCh)
{
    Synth &synth = *m_synth;
    uint32_t first = 0, last = synth.m_numChannels;
    for(uint32_t c = first; c < last; ++c)
    {
        if(m_chipChannels[c].users.empty())
            continue; // Nothing to do

        for(AdlChannel::users_iterator jnext = m_chipChannels[c].users.begin(); !jnext.is_end();)
        {
            AdlChannel::users_iterator j = jnext;
            AdlChannel::LocationData &jd = j->value;
            ++jnext;
            if((jd.loc.MidCh == midCh) && (jd.sustained == AdlChannel::LocationData::Sustain_None))
                jd.sustained |= AdlChannel::LocationData::Sustain_Sostenuto;
        }
    }
}

void MIDIplay::setRPN(size_t midCh, unsigned value, bool MSB)
{
    bool nrpn = m_midiChannels[midCh].nrpn;
    unsigned addr = m_midiChannels[midCh].lastmrpn * 0x100 + m_midiChannels[midCh].lastlrpn;

    switch(addr + nrpn * 0x10000 + MSB * 0x20000)
    {
    case 0x0000 + 0*0x10000 + 1*0x20000: // Pitch-bender sensitivity
        m_midiChannels[midCh].bendsense_msb = value;
        m_midiChannels[midCh].updateBendSensitivity();
        break;
    case 0x0000 + 0*0x10000 + 0*0x20000: // Pitch-bender sensitivity LSB
        m_midiChannels[midCh].bendsense_lsb = value;
        m_midiChannels[midCh].updateBendSensitivity();
        break;
    case 0x0108 + 1*0x10000 + 1*0x20000:
        if((m_synthMode & Mode_XG) != 0) // Vibrato speed
        {
            if(value == 64)      m_midiChannels[midCh].vibspeed = 1.0;
            else if(value < 100) m_midiChannels[midCh].vibspeed = 1.0 / (1.6e-2 * (value ? value : 1));
            else                 m_midiChannels[midCh].vibspeed = 1.0 / (0.051153846 * value - 3.4965385);
            m_midiChannels[midCh].vibspeed *= 2 * 3.141592653 * 5.0;
        }
        break;
    case 0x0109 + 1*0x10000 + 1*0x20000:
        if((m_synthMode & Mode_XG) != 0) // Vibrato depth
        {
            m_midiChannels[midCh].vibdepth = (((int)value - 64) * 0.15) * 0.01;
        }
        break;
    case 0x010A + 1*0x10000 + 1*0x20000:
        if((m_synthMode & Mode_XG) != 0) // Vibrato delay in millisecons
        {
            m_midiChannels[midCh].vibdelay_us = value ? int64_t(209.2 * std::exp(0.0795 * (double)value)) : 0;
        }
        break;
    default:/* UI.PrintLn("%s %04X <- %d (%cSB) (ch %u)",
                "NRPN"+!nrpn, addr, value, "LM"[MSB], MidCh);*/
        break;
    }
}

void MIDIplay::updatePortamento(size_t midCh)
{
    double rate = HUGE_VAL;
    uint16_t midival = m_midiChannels[midCh].portamento;
    if(m_midiChannels[midCh].portamentoEnable && midival > 0)
        rate = 350.0 * std::pow(2.0, -0.062 * (1.0 / 128) * midival);
    m_midiChannels[midCh].portamentoRate = rate;
}


void MIDIplay::noteOff(size_t midCh, uint8_t note, bool forceNow)
{
    MIDIchannel &ch = m_midiChannels[midCh];
    MIDIchannel::notes_iterator i = ch.find_activenote(note);

    if(!i.is_end())
    {
        MIDIchannel::NoteInfo &ni = i->value;
        if(forceNow || ni.ttl <= 0)
            noteUpdate(midCh, i, Upd_Off);
        else
            ni.isOnExtendedLifeTime = true;
    }
}


void MIDIplay::updateVibrato(double amount)
{
    for(size_t a = 0, b = m_midiChannels.size(); a < b; ++a)
    {
        if(m_midiChannels[a].hasVibrato() && !m_midiChannels[a].activenotes.empty())
        {
            noteUpdateAll(static_cast<uint16_t>(a), Upd_Pitch);
            m_midiChannels[a].vibpos += amount * m_midiChannels[a].vibspeed;
        }
        else
            m_midiChannels[a].vibpos = 0.0;
    }
}

size_t MIDIplay::chooseDevice(const std::string &name)
{
    std::map<std::string, size_t>::iterator i = m_midiDevices.find(name);

    if(i != m_midiDevices.end())
        return i->second;

    size_t n = m_midiDevices.size() * 16;
    m_midiDevices.insert(std::make_pair(name, n));
    m_midiChannels.resize(n + 16);
    resetMIDIDefaults(n);
    return n;
}

void MIDIplay::updateArpeggio(double) // amount = amount of time passed
{
    // If there is an adlib channel that has multiple notes
    // simulated on the same channel, arpeggio them.

    Synth &synth = *m_synth;

#if 0
    const unsigned desired_arpeggio_rate = 40; // Hz (upper limit)
#   if 1
    static unsigned cache = 0;
    amount = amount; // Ignore amount. Assume we get a constant rate.
    cache += MaxSamplesAtTime * desired_arpeggio_rate;

    if(cache < PCM_RATE) return;

    cache %= PCM_RATE;
#   else
    static double arpeggio_cache = 0;
    arpeggio_cache += amount * desired_arpeggio_rate;

    if(arpeggio_cache < 1.0) return;

    arpeggio_cache = 0.0;
#   endif
#endif

    ++m_arpeggioCounter;

    for(uint32_t c = 0; c < synth.m_numChannels; ++c)
    {
retry_arpeggio:
        if(c > uint32_t(std::numeric_limits<int32_t>::max()))
            break;

        size_t n_users = m_chipChannels[c].users.size();

        if(n_users > 1)
        {
            AdlChannel::users_iterator i = m_chipChannels[c].users.begin();
            size_t rate_reduction = 3;

            if(n_users >= 3)
                rate_reduction = 2;

            if(n_users >= 4)
                rate_reduction = 1;

            for(size_t count = (m_arpeggioCounter / rate_reduction) % n_users,
                n = 0; n < count; ++n)
                ++i;

            AdlChannel::LocationData &d = i->value;
            if(d.sustained == AdlChannel::LocationData::Sustain_None)
            {
                if(d.kon_time_until_neglible_us <= 0)
                {
                    noteUpdate(
                        d.loc.MidCh,
                        m_midiChannels[ d.loc.MidCh ].ensure_find_activenote(d.loc.note),
                        Upd_Off,
                        static_cast<int32_t>(c));
                    goto retry_arpeggio;
                }

                noteUpdate(
                    d.loc.MidCh,
                    m_midiChannels[ d.loc.MidCh ].ensure_find_activenote(d.loc.note),
                    Upd_Pitch | Upd_Volume | Upd_Pan,
                    static_cast<int32_t>(c));
            }
        }
    }
}

void MIDIplay::updateGlide(double amount)
{
    size_t num_channels = m_midiChannels.size();

    for(size_t channel = 0; channel < num_channels; ++channel)
    {
        MIDIchannel &midiChan = m_midiChannels[channel];
        if(midiChan.gliding_note_count == 0)
            continue;

        for(MIDIchannel::notes_iterator it = midiChan.activenotes.begin();
            !it.is_end(); ++it)
        {
            MIDIchannel::NoteInfo &info = it->value;
            double finalTone = info.noteTone;
            double previousTone = info.currentTone;

            bool directionUp = previousTone < finalTone;
            double toneIncr = amount * (directionUp ? +info.glideRate : -info.glideRate);

            double currentTone = previousTone + toneIncr;
            bool glideFinished = !(directionUp ? (currentTone < finalTone) : (currentTone > finalTone));
            currentTone = glideFinished ? finalTone : currentTone;

            if(currentTone != previousTone)
            {
                info.currentTone = currentTone;
                noteUpdate(static_cast<uint16_t>(channel), it, Upd_Pitch);
            }
        }
    }
}

void MIDIplay::describeChannels(char *str, char *attr, size_t size)
{
    if (!str || size <= 0)
        return;

    Synth &synth = *m_synth;
    uint32_t numChannels = synth.m_numChannels;

    uint32_t index = 0;
    while(index < numChannels && index < size - 1)
    {
        const AdlChannel &adlChannel = m_chipChannels[index];

        AdlChannel::const_users_iterator loc = adlChannel.users.begin();
        AdlChannel::const_users_iterator locnext(loc);
        if(!loc.is_end()) ++locnext;

	    if(loc.is_end())  // off
        {
            str[index] = '-';
        }
        else if(!locnext.is_end())  // arpeggio
        {
            str[index] = '@';
        }
        else  // on
        {
            switch(synth.m_channelCategory[index])
            {
            case Synth::ChanCat_Regular:
                str[index] = '+';
                break;
            case Synth::ChanCat_4op_Master:
            case Synth::ChanCat_4op_Slave:
                str[index] = '#';
                break;
            default:  // rhythm-mode percussion
                str[index] = 'r';
                break;
            }
        }

        uint8_t attribute = 0;
        if (!loc.is_end())  // 4-bit color index of MIDI channel
            attribute |= (uint8_t)(loc->value.loc.MidCh & 0xF);

        attr[index] = (char)attribute;
        ++index;
    }

    str[index] = 0;
    attr[index] = 0;
}
