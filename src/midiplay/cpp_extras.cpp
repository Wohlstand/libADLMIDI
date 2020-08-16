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

#include "adlmidi_private.hpp"
#include "adlmidi_midiplay.hpp"
#include "adlmidi_opl3.hpp"

#ifndef ADLMIDI_DISABLE_CPP_EXTRAS

struct AdlInstrumentTester::Impl
{
    uint32_t cur_gm;
    uint32_t ins_idx;
    std::vector<uint32_t> adl_ins_list;
    Synth *opl;
    MIDIplay *play;
};

ADLMIDI_EXPORT AdlInstrumentTester::AdlInstrumentTester(ADL_MIDIPlayer *device)
    : P(new Impl)
{
#ifndef DISABLE_EMBEDDED_BANKS
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    P->cur_gm = 0;
    P->ins_idx = 0;
    P->play = play;
    P->opl = play ? play->m_synth.get() : NULL;
#else
    ADL_UNUSED(device);
#endif
}

ADLMIDI_EXPORT AdlInstrumentTester::~AdlInstrumentTester()
{
    delete P;
}

ADLMIDI_EXPORT void AdlInstrumentTester::FindAdlList()
{
#ifndef DISABLE_EMBEDDED_BANKS
    const unsigned NumBanks = (unsigned)adl_getBanksCount();
    std::set<unsigned> adl_ins_set;
    for(unsigned bankno = 0; bankno < NumBanks; ++bankno)
        adl_ins_set.insert(banks[bankno][P->cur_gm]);
    P->adl_ins_list.assign(adl_ins_set.begin(), adl_ins_set.end());
    P->ins_idx = 0;
    NextAdl(0);
    P->opl->silenceAll();
#endif
}



ADLMIDI_EXPORT void AdlInstrumentTester::Touch(unsigned c, unsigned volume) // Volume maxes at 127*127*127
{
#ifndef DISABLE_EMBEDDED_BANKS
    Synth *opl = P->opl;
    if(opl->m_volumeScale == Synth::VOLUME_NATIVE)
        opl->touchNote(c, static_cast<uint8_t>(volume * 127 / (127 * 127 * 127) / 2));
    else
    {
        // The formula below: SOLVE(V=127^3 * 2^( (A-63.49999) / 8), A)
        opl->touchNote(c, static_cast<uint8_t>(volume > 8725 ? static_cast<unsigned int>(std::log((double)volume) * 11.541561 + (0.5 - 104.22845)) : 0));
        // The incorrect formula below: SOLVE(V=127^3 * (2^(A/63)-1), A)
        //Touch_Real(c, volume>11210 ? 91.61112 * std::log(4.8819E-7*volume + 1.0)+0.5 : 0);
    }
#else
    ADL_UNUSED(c);
    ADL_UNUSED(volume);
#endif
}

ADLMIDI_EXPORT void AdlInstrumentTester::DoNote(int note)
{
#ifndef DISABLE_EMBEDDED_BANKS
    MIDIplay *play = P->play;
    Synth *opl = P->opl;
    if(P->adl_ins_list.empty()) FindAdlList();
    const unsigned meta = P->adl_ins_list[P->ins_idx];
    const adlinsdata2 ains = adlinsdata2::from_adldata(::adlins[meta]);

    int tone = (P->cur_gm & 128) ? (P->cur_gm & 127) : (note + 50);
    if(ains.tone)
    {
        /*if(ains.tone < 20)
                tone += ains.tone;
            else */
        if(ains.tone < 128)
            tone = ains.tone;
        else
            tone -= ains.tone - 128;
    }
    double hertz = 172.00093 * std::exp(0.057762265 * (tone + 0.0));
    int32_t adlchannel[2] = { 0, 3 };
    if((ains.flags & (adlinsdata::Flag_Pseudo4op|adlinsdata::Flag_Real4op)) == 0)
    {
        adlchannel[1] = -1;
        adlchannel[0] = 6; // single-op
        if(play->hooks.onDebugMessage)
        {
            play->hooks.onDebugMessage(play->hooks.onDebugMessage_userData,
                                       "noteon at %d for %g Hz\n", adlchannel[0], hertz);
        }
    }
    else
    {
        if(play->hooks.onDebugMessage)
        {
            play->hooks.onDebugMessage(play->hooks.onDebugMessage_userData,
                                       "noteon at %d and %d for %g Hz\n", adlchannel[0], adlchannel[1], hertz);
        }
    }

    opl->noteOff(0);
    opl->noteOff(3);
    opl->noteOff(6);
    for(unsigned c = 0; c < 2; ++c)
    {
        if(adlchannel[c] < 0) continue;
        opl->setPatch(static_cast<size_t>(adlchannel[c]), ains.adl[c]);
        opl->touchNote(static_cast<size_t>(adlchannel[c]), 63);
        opl->setPan(static_cast<size_t>(adlchannel[c]), 0x30);
        opl->noteOn(static_cast<size_t>(adlchannel[c]), static_cast<size_t>(adlchannel[1]), hertz);
    }
#else
    ADL_UNUSED(note);
#endif
}

ADLMIDI_EXPORT void AdlInstrumentTester::NextGM(int offset)
{
#ifndef DISABLE_EMBEDDED_BANKS
    P->cur_gm = (P->cur_gm + 256 + (uint32_t)offset) & 0xFF;
    FindAdlList();
#else
    ADL_UNUSED(offset);
#endif
}

ADLMIDI_EXPORT void AdlInstrumentTester::NextAdl(int offset)
{
#ifndef DISABLE_EMBEDDED_BANKS
    //Synth *opl = P->opl;
    if(P->adl_ins_list.empty()) FindAdlList();
    const unsigned NumBanks = (unsigned)adl_getBanksCount();
    P->ins_idx = (uint32_t)((int32_t)P->ins_idx + (int32_t)P->adl_ins_list.size() + offset) % (int32_t)P->adl_ins_list.size();

#if 0
    UI.Color(15);
    std::fflush(stderr);
    std::printf("SELECTED G%c%d\t%s\n",
                cur_gm < 128 ? 'M' : 'P', cur_gm < 128 ? cur_gm + 1 : cur_gm - 128,
                "<-> select GM, ^v select ins, qwe play note");
    std::fflush(stdout);
    UI.Color(7);
    std::fflush(stderr);
#endif

    for(size_t a = 0, n = P->adl_ins_list.size(); a < n; ++a)
    {
        const unsigned i = P->adl_ins_list[a];
        const adlinsdata2 ains = adlinsdata2::from_adldata(::adlins[i]);

        char ToneIndication[8] = "   ";
        if(ains.tone)
        {
            /*if(ains.tone < 20)
                    snprintf(ToneIndication, 8, "+%-2d", ains.tone);
                else*/
            if(ains.tone < 128)
                snprintf(ToneIndication, 8, "=%-2d", ains.tone);
            else
                snprintf(ToneIndication, 8, "-%-2d", ains.tone - 128);
        }
        std::printf("%s%s%s%u\t",
                    ToneIndication,
                    (ains.flags & (adlinsdata::Flag_Pseudo4op|adlinsdata::Flag_Real4op)) ? "[2]" : "   ",
                    (P->ins_idx == a) ? "->" : "\t",
                    i
                   );

        for(unsigned bankno = 0; bankno < NumBanks; ++bankno)
            if(banks[bankno][P->cur_gm] == i)
                std::printf(" %u", bankno);

        std::printf("\n");
    }
#else
    ADL_UNUSED(offset);
#endif
}

ADLMIDI_EXPORT bool AdlInstrumentTester::HandleInputChar(char ch)
{
#ifndef DISABLE_EMBEDDED_BANKS
    static const char notes[] = "zsxdcvgbhnjmq2w3er5t6y7ui9o0p";
    //                           c'd'ef'g'a'bC'D'EF'G'A'Bc'd'e
    switch(ch)
    {
    case '/':
    case 'H':
    case 'A':
        NextAdl(-1);
        break;
    case '*':
    case 'P':
    case 'B':
        NextAdl(+1);
        break;
    case '-':
    case 'K':
    case 'D':
        NextGM(-1);
        break;
    case '+':
    case 'M':
    case 'C':
        NextGM(+1);
        break;
    case 3:
#if !((!defined(__WIN32__) || defined(__CYGWIN__)) && !defined(__DJGPP__))
    case 27:
#endif
        return false;
    default:
        const char *p = std::strchr(notes, ch);
        if(p && *p)
            DoNote((int)(p - notes) - 12);
    }
#else
    ADL_UNUSED(ch);
#endif
    return true;
}

#endif /* ADLMIDI_DISABLE_CPP_EXTRAS */
