/*
 * libADLMIDI is a free MIDI to WAV conversion library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2017 Vitaly Novichkov <admin@wohlnet.ru>
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

#include "adlmidi_private.hpp"

#ifdef ADLMIDI_HW_OPL
#define MaxCards 1
#define MaxCards_STR "1" //Why not just "#MaxCards" ? Watcom fails to pass this with "syntax error" :-P
#else
#define MaxCards 100
#define MaxCards_STR "100"
#endif


/*---------------------------EXPORTS---------------------------*/

ADLMIDI_EXPORT struct ADL_MIDIPlayer *adl_init(long sample_rate)
{
    ADL_MIDIPlayer *midi_device;
    midi_device = (ADL_MIDIPlayer *)malloc(sizeof(ADL_MIDIPlayer));
    if(!midi_device)
    {
        ADLMIDI_ErrorString = "Can't initialize ADLMIDI: out of memory!";
        return NULL;
    }

    MIDIplay *player = new MIDIplay;
    if(!player)
    {
        free(midi_device);
        ADLMIDI_ErrorString = "Can't initialize ADLMIDI: out of memory!";
        return NULL;
    }

    midi_device->adl_midiPlayer = player;
    player->m_setup.PCM_RATE = static_cast<unsigned long>(sample_rate);
    player->m_setup.mindelay = 1.0 / (double)player->m_setup.PCM_RATE;
    // player->m_setup.maxdelay = 512.0 / (double)player->m_setup.PCM_RATE;
    player->ChooseDevice("none");
    adlRefreshNumCards(midi_device);
    return midi_device;
}

ADLMIDI_EXPORT int adl_setNumChips(ADL_MIDIPlayer *device, int numCards)
{
    if(device == NULL)
        return -2;

    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->m_setup.NumCards = static_cast<unsigned int>(numCards);
    if(play->m_setup.NumCards < 1 || play->m_setup.NumCards > MaxCards)
    {
        play->setErrorString("number of chips may only be 1.." MaxCards_STR ".\n");
        return -1;
    }

    play->opl.NumCards = play->m_setup.NumCards;

    return adlRefreshNumCards(device);
}

ADLMIDI_EXPORT int adl_getNumChips(struct ADL_MIDIPlayer *device)
{
    if(device == NULL)
        return -2;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(play)
        return (int)play->m_setup.NumCards;
    return -2;
}

ADLMIDI_EXPORT int adl_setBank(ADL_MIDIPlayer *device, int bank)
{
    #ifdef DISABLE_EMBEDDED_BANKS
    ADL_UNUSED(device);
    ADL_UNUSED(bank);
    ADLMIDI_ErrorString = "This build of libADLMIDI has no embedded banks. Please load bank by using of adl_openBankFile() or adl_openBankData() functions instead of adl_setBank()";
    return -1;
    #else
    const uint32_t NumBanks = static_cast<uint32_t>(maxAdlBanks());
    int32_t bankno = bank;

    if(bankno < 0)
        bankno = 0;

    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(static_cast<uint32_t>(bankno) >= NumBanks)
    {
        #ifndef __WATCOMC__
        std::stringstream s;
        s << "bank number may only be 0.." << (NumBanks - 1) << ".\n";
        play->setErrorString(s.str());
        #else
        play->setErrorString("Selected embedded bank is not exists!\n");
        #endif
        return -1;
    }

    play->m_setup.AdlBank = static_cast<uint32_t>(bankno);
    play->opl.setEmbeddedBank(play->m_setup.AdlBank);

    return adlRefreshNumCards(device);
    #endif
}

ADLMIDI_EXPORT int adl_getBanksCount()
{
    return maxAdlBanks();
}

ADLMIDI_EXPORT const char *const *adl_getBankNames()
{
    return banknames;
}

ADLMIDI_EXPORT int adl_setNumFourOpsChn(ADL_MIDIPlayer *device, int ops4)
{
    if(!device)
        return -1;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if((unsigned int)ops4 > 6 * play->m_setup.NumCards)
    {
        #ifndef __WATCOMC__
        std::stringstream s;
        s << "number of four-op channels may only be 0.." << (6 * (play->m_setup.NumCards)) << " when " << play->m_setup.NumCards << " OPL3 cards are used.\n";
        play->setErrorString(s.str());
        #else
        play->setErrorString("number of four-op channels may not be more than 6 channels per each OPL3 chip!\n");
        #endif

        return -1;
    }

    play->m_setup.NumFourOps = static_cast<unsigned int>(ops4);
    play->opl.NumFourOps = play->m_setup.NumFourOps;

    return 0; //adlRefreshNumCards(device);
}

ADLMIDI_EXPORT int adl_getNumFourOpsChn(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return -1;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(play)
        return (int)play->m_setup.NumFourOps;
    return -1;
}

ADLMIDI_EXPORT void adl_setPercMode(ADL_MIDIPlayer *device, int percmod)
{
    if(!device) return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->m_setup.AdlPercussionMode = (percmod != 0);
    play->opl.AdlPercussionMode = play->m_setup.AdlPercussionMode;
}

ADLMIDI_EXPORT void adl_setHVibrato(ADL_MIDIPlayer *device, int hvibro)
{
    if(!device) return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->m_setup.HighVibratoMode = (hvibro != 0);
    play->opl.HighVibratoMode = play->m_setup.HighVibratoMode;
}

ADLMIDI_EXPORT void adl_setHTremolo(ADL_MIDIPlayer *device, int htremo)
{
    if(!device) return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->m_setup.HighTremoloMode = (htremo != 0);
    play->opl.HighTremoloMode = play->m_setup.HighTremoloMode;
}

ADLMIDI_EXPORT void adl_setScaleModulators(ADL_MIDIPlayer *device, int smod)
{
    if(!device) return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->m_setup.ScaleModulators = (smod != 0);
    play->opl.ScaleModulators = play->m_setup.ScaleModulators;
}

ADLMIDI_EXPORT void adl_setLoopEnabled(ADL_MIDIPlayer *device, int loopEn)
{
    if(!device) return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->m_setup.loopingIsEnabled = (loopEn != 0);
}

ADLMIDI_EXPORT void adl_setLogarithmicVolumes(struct ADL_MIDIPlayer *device, int logvol)
{
    if(!device) return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->m_setup.LogarithmicVolumes = (logvol != 0);
    play->opl.LogarithmicVolumes = play->m_setup.LogarithmicVolumes;
}

ADLMIDI_EXPORT void adl_setVolumeRangeModel(struct ADL_MIDIPlayer *device, int volumeModel)
{
    if(!device) return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->m_setup.VolumeModel = volumeModel;
    play->opl.ChangeVolumeRangesModel(static_cast<ADLMIDI_VolumeModels>(volumeModel));
}

ADLMIDI_EXPORT int adl_openBankFile(struct ADL_MIDIPlayer *device, const char *filePath)
{
    if(device && device->adl_midiPlayer)
    {
        MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
        if(!play->LoadBank(filePath))
        {
            std::string err = play->getErrorString();
            if(err.empty())
                play->setErrorString("ADL MIDI: Can't load file");
            return -1;
        }
        else return adlRefreshNumCards(device);
    }

    ADLMIDI_ErrorString = "Can't load file: ADLMIDI is not initialized";
    return -1;
}

ADLMIDI_EXPORT int adl_openBankData(struct ADL_MIDIPlayer *device, const void *mem, unsigned long size)
{
    if(device && device->adl_midiPlayer)
    {
        MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
        if(!play->LoadBank(mem, static_cast<size_t>(size)))
        {
            std::string err = play->getErrorString();
            if(err.empty())
                play->setErrorString("ADL MIDI: Can't load data from memory");
            return -1;
        }
        else return adlRefreshNumCards(device);
    }

    ADLMIDI_ErrorString = "Can't load file: ADL MIDI is not initialized";
    return -1;
}

ADLMIDI_EXPORT int adl_openFile(ADL_MIDIPlayer *device, const char *filePath)
{
    if(device && device->adl_midiPlayer)
    {
        MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
        if(!play->LoadMIDI(filePath))
        {
            std::string err = play->getErrorString();
            if(err.empty())
                play->setErrorString("ADL MIDI: Can't load file");
            return -1;
        }
        else return 0;
    }

    ADLMIDI_ErrorString = "Can't load file: ADL MIDI is not initialized";
    return -1;
}

ADLMIDI_EXPORT int adl_openData(ADL_MIDIPlayer *device, const void *mem, unsigned long size)
{
    if(device && device->adl_midiPlayer)
    {
        MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
        if(!play->LoadMIDI(mem, static_cast<size_t>(size)))
        {
            std::string err = play->getErrorString();
            if(err.empty())
                play->setErrorString("ADL MIDI: Can't load data from memory");
            return -1;
        }
        else return 0;
    }
    ADLMIDI_ErrorString = "Can't load file: ADL MIDI is not initialized";
    return -1;
}


ADLMIDI_EXPORT const char *adl_emulatorName()
{
    #ifdef ADLMIDI_USE_DOSBOX_OPL
    return "DosBox";
    #else
    return "Nuked";
    #endif
}

ADLMIDI_EXPORT const char *adl_linkedLibraryVersion()
{
    return ADLMIDI_VERSION;
}


ADLMIDI_EXPORT const char *adl_errorString()
{
    return ADLMIDI_ErrorString.c_str();
}

ADLMIDI_EXPORT const char *adl_errorInfo(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return adl_errorString();
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!play)
        return adl_errorString();
    return play->getErrorString().c_str();
}

ADLMIDI_EXPORT const char *adl_getMusicTitle(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return "";
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!play)
        return "";
    return play->musTitle.c_str();
}

ADLMIDI_EXPORT void adl_close(struct ADL_MIDIPlayer *device)
{
    if(device->adl_midiPlayer)
        delete reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    device->adl_midiPlayer = NULL;
    free(device);
    device = NULL;
}

ADLMIDI_EXPORT void adl_reset(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->opl.Reset(play->m_setup.PCM_RATE);
}

ADLMIDI_EXPORT double adl_totalTimeLength(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return -1.0;
    return reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->timeLength();
}

ADLMIDI_EXPORT double adl_loopStartTime(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return -1.0;
    return reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->getLoopStart();
}

ADLMIDI_EXPORT double adl_loopEndTime(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return -1.0;
    return reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->getLoopEnd();
}

ADLMIDI_EXPORT double adl_positionTell(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return -1.0;
    return reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->tell();
}

ADLMIDI_EXPORT void adl_positionSeek(struct ADL_MIDIPlayer *device, double seconds)
{
    if(!device)
        return;
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->seek(seconds);
}

ADLMIDI_EXPORT void adl_positionRewind(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return;
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->rewind();
}

ADLMIDI_EXPORT void adl_setTempo(struct ADL_MIDIPlayer *device, double tempo)
{
    if(!device || (tempo <= 0.0))
        return;
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->setTempo(tempo);
}


ADLMIDI_EXPORT const char *adl_metaMusicTitle(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return "";
    return reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->musTitle.c_str();
}


ADLMIDI_EXPORT const char *adl_metaMusicCopyright(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return "";
    return reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->musCopyright.c_str();
}

ADLMIDI_EXPORT size_t adl_metaTrackTitleCount(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return 0;
    return reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->musTrackTitles.size();
}

ADLMIDI_EXPORT const char *adl_metaTrackTitle(struct ADL_MIDIPlayer *device, size_t index)
{
    if(!device)
        return 0;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(index >= play->musTrackTitles.size())
        return "INVALID";
    return play->musTrackTitles[index].c_str();
}


ADLMIDI_EXPORT size_t adl_metaMarkerCount(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return 0;
    return reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->musMarkers.size();
}

ADLMIDI_EXPORT const Adl_MarkerEntry adl_metaMarker(struct ADL_MIDIPlayer *device, size_t index)
{
    struct Adl_MarkerEntry marker;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!device || !play || (index >= play->musMarkers.size()))
    {
        marker.label = "INVALID";
        marker.pos_time = 0.0;
        marker.pos_ticks = 0;
        return marker;
    }
    else
    {
        MIDIplay::MIDI_MarkerEntry &mk = play->musMarkers[index];
        marker.label = mk.label.c_str();
        marker.pos_time = mk.pos_time;
        marker.pos_ticks = (unsigned long)mk.pos_ticks;
    }
    return marker;
}

ADLMIDI_EXPORT void adl_setRawEventHook(struct ADL_MIDIPlayer *device, ADL_RawEventHook rawEventHook, void *userData)
{
    if(!device)
        return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->hooks.onEvent = rawEventHook;
    play->hooks.onEvent_userData = userData;
}

/* Set note hook */
ADLMIDI_EXPORT void adl_setNoteHook(struct ADL_MIDIPlayer *device, ADL_NoteHook noteHook, void *userData)
{
    if(!device)
        return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->hooks.onNote = noteHook;
    play->hooks.onNote_userData = userData;
}

/* Set debug message hook */
ADLMIDI_EXPORT void adl_setDebugMessageHook(struct ADL_MIDIPlayer *device, ADL_DebugMessageHook debugMessageHook, void *userData)
{
    if(!device)
        return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->hooks.onDebugMessage = debugMessageHook;
    play->hooks.onDebugMessage_userData = userData;
}

ADLMIDI_EXPORT int adl_play(ADL_MIDIPlayer *device, int sampleCount, short *out)
{
    #ifdef ADLMIDI_HW_OPL
    (void)device;
    (void)sampleCount;
    (void)out;
    return 0;
    #else
    sampleCount -= sampleCount % 2; //Avoid even sample requests
    if(sampleCount < 0)
        return 0;
    if(!device)
        return 0;

    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    MIDIplay::Setup &setup = player->m_setup;

    int ret = 0;

    std::memset(out, 0, static_cast<size_t>(sampleCount * 2) * sizeof(int16_t));

    while(sampleCount && !player->atEnd)
    {
        const uint32_t samples = setup.nextTick < sampleCount ? setup.nextTick : sampleCount;

        #ifdef ADLMIDI_USE_DOSBOX_OPL
        ssize_t in_generatedStereo = (samples > 512) ? 512 : samples;
        #else
        ssize_t in_generatedStereo = samples;
        #endif

        unsigned int chips = player->opl.NumCards;
        if(chips == 1)
        {
            #ifdef ADLMIDI_USE_DOSBOX_OPL
            player->opl.cards[0].GenerateArr(out, &in_generatedStereo);
            #else
            OPL3_GenerateStream(&player->opl.cards[0], out, static_cast<Bit32u>(in_generatedStereo));
            #endif                
        }
        else
        {
            /* Generate data from every chip and mix result */
            for(unsigned card = 0; card < chips; ++card)
            {
                #ifdef ADLMIDI_USE_DOSBOX_OPL
                player->opl.cards[card].GenerateArrMix(out, &in_generatedStereo);
                #else
                OPL3_GenerateStreamMix(&player->opl.cards[card], out, static_cast<Bit32u>(in_generatedStereo));
                #endif
            }                
        }
        out += in_generatedStereo * 2;

        sampleCount -= in_generatedStereo;
        ret += in_generatedStereo;

        double eat_delay = in_generatedStereo / (double)setup.PCM_RATE;
        double delay = player->Tick(eat_delay, setup.mindelay);
        setup.nextTick = delay * setup.PCM_RATE;
    }

    return static_cast<int>(ret);
    #endif
}


ADLMIDI_EXPORT int adl_generate(struct ADL_MIDIPlayer *device, int sampleCount, short *out)
{
    #ifdef ADLMIDI_HW_OPL
    (void)device;
    (void)sampleCount;
    (void)out;
    return 0;
    #else
    sampleCount -= sampleCount % 2; //Avoid even sample requests
    if(sampleCount < 0)
        return 0;
    if(!device)
        return 0;

    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    sampleCount = (sampleCount > 1024) ? 1024 : sampleCount;

    //! Count of stereo samples
    ssize_t in_generatedStereo = sampleCount / 2;
    //! Unsigned total sample count
    //fill buffer with zeros
    std::memset(out, 0, static_cast<size_t>(sampleCount) * sizeof(int16_t));

    if(player->m_setup.NumCards == 1)
    {
        #ifdef ADLMIDI_USE_DOSBOX_OPL
        player->opl.cards[0].GenerateArr(out, &in_generatedStereo);
        sampleCount = in_generatedStereo * 2;
        #else
        OPL3_GenerateStream(&player->opl.cards[0], out, static_cast<Bit32u>(in_generatedStereo));
        #endif
    }
    else
    {
        /* Generate data from every chip and mix result */
        for(unsigned card = 0; card < player->m_setup.NumCards; ++card)
        {
            #ifdef ADLMIDI_USE_DOSBOX_OPL
            player->opl.cards[card].GenerateArrMix(out, &in_generatedStereo);
            sampleCount = in_generatedStereo * 2;
            #else
            OPL3_GenerateStreamMix(&player->opl.cards[card], out, static_cast<Bit32u>(in_generatedStereo));
            #endif
        }
    }

    return sampleCount;
    #endif
}

ADLMIDI_EXPORT double adl_tickEvents(struct ADL_MIDIPlayer *device, double seconds, double granuality)
{
    if(!device)
        return -1.0;
    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!player)
        return -1.0;
    return player->Tick(seconds, granuality);
}

ADLMIDI_EXPORT int adl_atEnd(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return 1;
    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!player)
        return 1;
    return (int)player->atEnd;
}

ADLMIDI_EXPORT void adl_panic(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return;
    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!player)
        return;
    player->realTime_panic();
}
