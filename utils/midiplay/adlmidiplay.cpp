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

#include <string>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <vector>

#ifndef ADLMIDI_ENABLE_HW_DOS
#   include <signal.h>
#   include "utf8main.h" // IWYU pragma: keep
#else
#   include <adlmidi_dos.h>
#endif

#include <adlmidi.h>

#include "misc.h"
#include "time_counter.h"
#include "dev_setup.h"
#include "playback/playback.h"


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
    case ADLMIDI_VolumeModel_MS_ADLIB:
        return "MS AdLib";
    case ADLMIDI_VolumeModel_IMF_Creator:
        return "IMF Creator";
    case ADLMIDI_VolumeModel_OConnell:
        return "Jammie O'Connel's FM Synth";
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


#ifndef ADLMIDI_ENABLE_HW_DOS
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
#endif // ADLMIDI_ENABLE_HW_DOS

static void debugPrint(void * /*userdata*/, const char *fmt, ...)
{
    char buffer[4096];
    std::va_list args;
    va_start(args, fmt);
    int rc = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    if(rc > 0)
    {
        s_fprintf(stdout, "                                                                              \r");
        s_fprintf(stdout, " - Debug: %s\n", buffer);
        flushout(stdout);
    }
}

#ifdef ADLMIDI_ENABLE_HW_DOS
static void debugPrintDpmiTail()
{}

static void debugPrintLock()
{
    void (*c_lock_begin)(void * /*userdata*/, const char *fmt, ...) = &debugPrint;
    void (*c_lock_end)(void) = &debugPrintDpmiTail;
    adl_dpmi_lock_region((void*&)c_lock_begin, (void*&)c_lock_end);
}

static void debugPrintUnlock()
{
    void (*c_lock_begin)(void * /*userdata*/, const char *fmt, ...) = &debugPrint;
    void (*c_lock_end)(void) = &debugPrintDpmiTail;
    adl_dpmi_unlock_region((void*&)c_lock_begin, (void*&)c_lock_end);
}
#endif

#ifdef DEBUG_TRACE_ALL_EVENTS
static void debugPrintEvent(void * /*userdata*/, ADL_UInt8 type, ADL_UInt8 subtype, ADL_UInt8 channel, const ADL_UInt8 * /*data*/, size_t len)
{
    s_fprintf(stdout, " - E: 0x%02X 0x%02X %02d (%d)\r\n", type, subtype, channel, (int)len);
    flushout(stdout);
}
#endif

#ifdef ADLMIDI_ENABLE_HW_DOS
static void oplSend(ADL_UInt16 port, ADL_UInt16 reg, ADL_UInt8 val)
{
    int delay;

    outportb(port, reg);

    for(delay = 6; delay > 0; --delay)
        inportb(port);

    outportb(port + 1, val);

    for(delay = 27; delay > 0; --delay)
        inportb(port);
}

static bool oplChipInit(ADL_UInt16 address)
{
    int status1 = 0;
    int status2 = 0;
    int i;
    bool ret;

    if(address == 0)
        address = 0x388; // Default!

    oplSend(address, 4, 0x60);
    oplSend(address, 4, 0x80);

    status1 = inportb(address);

    oplSend(address, 2, 0xFF);
    oplSend(address, 4, 0x21);

    for(i = 100; i > 0; --i)
        inportb(address);

    status2 = inp(address);

    oplSend(address, 4, 0x60);
    oplSend(address, 4, 0x80);

    ret = ((status1 & 0xE0) == 0x00) && ((status2 & 0xE0) == 0xC0);

    if(!ret)
        s_fprintf(stdout, " - OPL detect status: addr=0x%02X 0x%02X, 0x%02X\n", address, status1, status2);

    return ret;
}

#endif // ADLMIDI_ENABLE_HW_DOS


int main(int argc, char **argv)
{
    s_fprintf(stdout, "==========================================\n"
#ifdef ADLMIDI_ENABLE_HW_DOS
                      "      libADLMIDI demo utility (HW OPL)\n"
#else
                      "         libADLMIDI demo utility\n"
#endif
                      "==========================================\n\n");
    flushout(stdout);

    bool doQuit = false;
    int parseRet = s_devSetup.parseArgs(argc, argv, &doQuit);

    if(doQuit)
        return parseRet;

    ADL_MIDIPlayer *myDevice;

#ifdef ADLMIDI_ENABLE_HW_DOS
    __djgpp_nearptr_enable();

    if(!oplChipInit(s_devSetup.setHwAddress))
    {
        printError("Failed to detect OPL2/OPL3 chip!\n");
        return 1;
    }

    DosTaskman taskMan;

    if(s_devSetup.setHwAddress > 0 || s_devSetup.setChipType != ADLMIDI_DOS_ChipAuto)
        adl_switchDOSHW(s_devSetup.setChipType, s_devSetup.setHwAddress);
#endif

    //Initialize libADLMIDI and create the instance (you can initialize multiple of them!)
    myDevice = adl_init(s_devSetup.sampleRate);
    if(myDevice == NULL)
    {
        printError("Failed to init MIDI device!\n");
        return 1;
    }

    //Set internal debug messages hook to print all libADLMIDI's internal debug messages
    adl_setDebugMessageHook(myDevice, debugPrint, NULL);

#if !defined(ADLMIDI_ENABLE_HW_DOS) && !defined(OUTPUT_WAVE_ONLY)
    g_audioFormat.type = ADLMIDI_SampleType_S16;
    g_audioFormat.containerSize = sizeof(int16_t);
    g_audioFormat.sampleOffset = sizeof(int16_t) * 2;
#endif

    if(s_devSetup.setHwVibrato >= 0)
        adl_setHVibrato(myDevice, s_devSetup.setHwVibrato);//Force turn on deep vibrato

    if(s_devSetup.setHwTremolo >= 0)
        adl_setHTremolo(myDevice, s_devSetup.setHwTremolo);//Force turn on deep tremolo

    if(s_devSetup.setScaleMods >= 0)
        adl_setScaleModulators(myDevice, s_devSetup.setScaleMods);//Turn on modulators scaling by volume

    if(s_devSetup.setFullRangeBright >= 0)
        adl_setFullRangeBrightness(myDevice, s_devSetup.setFullRangeBright);//Turn on a full-ranged XG CC74 Brightness

    if(s_devSetup.enableFullPanning >= 0)
        adl_setSoftPanEnabled(myDevice, s_devSetup.enableFullPanning);

#ifndef OUTPUT_WAVE_ONLY
    //Turn loop on/off (for WAV recording loop must be disabled!)
    adl_setLoopEnabled(myDevice, s_devSetup.recordWave ? 0 : s_devSetup.loopEnabled);
#endif

    adl_setModeEMIDI(myDevice, s_devSetup.modeEMIDI);

    adl_setAutoArpeggio(myDevice, s_devSetup.autoArpeggioEnabled);
    adl_setChannelAllocMode(myDevice, s_devSetup.chanAlloc);

#ifdef DEBUG_TRACE_ALL_EVENTS
    //Hook all MIDI events are ticking while generating an output buffer
    if(!recordWave)
        adl_setRawEventHook(myDevice, debugPrintEvent, NULL);
#endif

#if defined(ADLMIDI_ENABLE_HW_SERIAL) && !defined(OUTPUT_WAVE_ONLY)
    if(s_devSetup.hwSerial)
        adl_switchSerialHW(myDevice, s_devSetup.serialName.c_str(), s_devSetup.serialBaud, s_devSetup.serialProto);
    else
#endif
#ifndef ADLMIDI_ENABLE_HW_DOS
    adl_switchEmulator(myDevice, s_devSetup.emulator);
#endif

    s_fprintf(stdout, " - Library version %s\n", adl_linkedLibraryVersion());
#ifdef ADLMIDI_ENABLE_HW_DOS
    s_fprintf(stdout, " - Hardware chip in use: %s\n", adl_chipEmulatorName(myDevice));
#elif defined(ADLMIDI_ENABLE_HW_SERIAL) && !defined(OUTPUT_WAVE_ONLY)
    if(s_devSetup.hwSerial)
        s_fprintf(stdout, " - %s [device %s] in use\n", adl_chipEmulatorName(myDevice), s_devSetup.serialName.c_str());
    else
        s_fprintf(stdout, " - %s Emulator in use\n", adl_chipEmulatorName(myDevice));
#else
    s_fprintf(stdout, " - %s Emulator in use\n", adl_chipEmulatorName(myDevice));
#endif

    if(s_devSetup.setBankNo >= 0)
    {
        //Choose one of embedded banks
        if(adl_setBank(myDevice, s_devSetup.setBankNo) != 0)
        {
            printError(adl_errorInfo(myDevice), "Can't set an embedded bank");
            adl_close(myDevice);
            return 1;
        }

        s_fprintf(stdout, " - Use embedded bank #%d [%s]\n", s_devSetup.setBankNo, adl_getBankNames()[s_devSetup.setBankNo]);
    }
    else if(!s_devSetup.setBankFile.empty())
    {
        s_fprintf(stdout, " - Use custom bank [%s]...", s_devSetup.setBankFile.c_str());
        flushout(stdout);

        //Open external bank file (WOPL format is supported)
        //to create or edit them, use OPL3 Bank Editor you can take here https://github.com/Wohlstand/OPL3BankEditor
        if(adl_openBankFile(myDevice, s_devSetup.setBankFile.c_str()) != 0)
        {
            s_fprintf(stdout, "FAILED!\n");
            flushout(stdout);
            printError(adl_errorInfo(myDevice), "Can't open a custom bank file");
            adl_close(myDevice);
            return 1;
        }

        s_fprintf(stdout, "OK!\n");
    }


    if(s_devSetup.multibankFromEnbededTest)
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

        s_fprintf(stdout, " - Ran a test of multibank over embedded\n");
    }

#ifndef ADLMIDI_ENABLE_HW_DOS
    int numOfChips = 4;

    if(s_devSetup.setNumChips >= 0)
        numOfChips = s_devSetup.setNumChips;

#if defined(ADLMIDI_ENABLE_HW_SERIAL) && !defined(OUTPUT_WAVE_ONLY)
    if(s_devSetup.hwSerial)
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
    if(s_devSetup.setNumChips >= 0 && s_devSetup.setNumChips <= 2 && s_devSetup.setChipType == ADLMIDI_DOS_ChipOPL2)
        adl_setNumChips(myDevice, s_devSetup.setNumChips);
#endif

    if(s_devSetup.volumeModel != ADLMIDI_VolumeModel_AUTO)
        adl_setVolumeRangeModel(myDevice, s_devSetup.volumeModel);

    if(s_devSetup.setNum4op >= 0)
    {
        //Set total count of 4-operator channels between all emulated chips
        if(adl_setNumFourOpsChn(myDevice, s_devSetup.setNum4op) != 0)
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

    if(s_devSetup.songNumLoad >= 0)
        adl_selectSongNum(myDevice, s_devSetup.songNumLoad);

#if defined(DEBUG_SONG_SWITCHING) || defined(ENABLE_TERMINAL_HOTKEYS)
    set_conio_terminal_mode();
#   ifdef DEBUG_SONG_SWITCHING
    if(s_devSetup.songNumLoad < 0)
        s_devSetup.songNumLoad = 0;
#   endif
#endif

    //Open MIDI file to play
    if(adl_openFile(myDevice, s_devSetup.musPath.c_str()) != 0)
    {
        s_fprintf(stdout, " - File [%s] failed!\n", s_devSetup.musPath.c_str());
        flushout(stdout);
        printError(adl_errorInfo(myDevice), "Can't open MIDI file");
        adl_close(myDevice);
        return 2;
    }

    s_fprintf(stdout, " - Number of chips %d\n", adl_getNumChipsObtained(myDevice));
    s_fprintf(stdout, " - Number of four-ops %d\n", adl_getNumFourOpsChnObtained(myDevice));
    s_fprintf(stdout, " - Track count: %lu\n", static_cast<unsigned long>(adl_trackCount(myDevice)));
    s_fprintf(stdout, " - Volume model: %s\n", volume_model_to_str(adl_getVolumeRangeModel(myDevice)));
    s_fprintf(stdout, " - Channel allocation mode: %s\n", chanalloc_to_str(adl_getChannelAllocMode(myDevice)));

#ifndef ADLMIDI_ENABLE_HW_DOS
#   ifdef ADLMIDI_ENABLE_HW_SERIAL
    if(!s_devSetup.hwSerial)
#   endif
    {
        s_fprintf(stdout, " - Gain level: %g\n", g_gaining);
    }
#endif

    int songsCount = adl_getSongsCount(myDevice);
    if(s_devSetup.songNumLoad >= 0)
        s_fprintf(stdout, " - Attempt to load song number: %d / %d\n", s_devSetup.songNumLoad, songsCount);
    else if(songsCount > 0)
        s_fprintf(stdout, " - File contains %d song(s)\n", songsCount);

    if(s_devSetup.soloTrack != ~static_cast<size_t>(0))
    {
        s_fprintf(stdout, " - Solo track: %lu\n", static_cast<unsigned long>(s_devSetup.soloTrack));
        adl_setTrackOptions(myDevice, s_devSetup.soloTrack, ADLMIDI_TrackOption_Solo);
    }

    if(!s_devSetup.onlyTracks.empty())
    {
        size_t count = adl_trackCount(myDevice);

        for(size_t track = 0; track < count; ++track)
            adl_setTrackOptions(myDevice, track, ADLMIDI_TrackOption_Off);

        s_fprintf(stdout, " - Only tracks:");

        for(size_t i = 0, n = s_devSetup.onlyTracks.size(); i < n; ++i)
        {
            size_t track = s_devSetup.onlyTracks[i];
            adl_setTrackOptions(myDevice, track, ADLMIDI_TrackOption_On);
            s_fprintf(stdout, " %lu", static_cast<unsigned long>(track));
        }

        s_fprintf(stdout, "\n");
    }

    s_fprintf(stdout, " - Automatic arpeggio is turned %s\n", adl_getAutoArpeggio(myDevice) ? "ON" : "OFF");

    s_fprintf(stdout, " - File [%s] opened!\n", s_devSetup.musPath.c_str());
    flushout(stdout);

#ifndef ADLMIDI_ENABLE_HW_DOS
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
#   if !defined(_WIN32) && !defined(__WATCOMC__)
    signal(SIGHUP, sighandler);
#   endif

#else // ADLMIDI_ENABLE_HW_DOS
    s_fprintf(stdout, " - [DOS] Running clock with %d hz\n", s_devSetup.clock_freq);
#endif//ADLMIDI_ENABLE_HW_DOS

    s_timeCounter.setTotal(adl_totalTimeLength(myDevice));

#ifndef OUTPUT_WAVE_ONLY
    s_timeCounter.setLoop(adl_loopStartTime(myDevice), adl_loopEndTime(myDevice));

#   ifndef ADLMIDI_ENABLE_HW_DOS
    if(!s_devSetup.recordWave)
#   endif
    {
        s_fprintf(stdout, " - Loop is turned %s\n", s_devSetup.loopEnabled ? "ON" : "OFF");
        if(s_timeCounter.hasLoop)
            s_fprintf(stdout, " - Has loop points: %s ... %s\n", s_timeCounter.loopStartHMS, s_timeCounter.loopEndHMS);
        s_fprintf(stdout, "\n==========================================\n");

#ifndef ADLMIDI_ENABLE_HW_DOS
        s_fprintf(stdout, "Playing... Hit Ctrl+C to quit!\n");
#else
        s_fprintf(stdout, "Playing... Hit Ctrl+C or ESC to quit!\n");
#endif
        flushout(stdout);

#   ifndef ADLMIDI_ENABLE_HW_DOS
#       ifdef ADLMIDI_ENABLE_HW_SERIAL
        if(s_devSetup.hwSerial)
            runHWSerialLoop(myDevice);
        else
#       endif
        {
            int ret = runAudioLoop(myDevice, s_devSetup.spec);
            if (ret != 0)
            {
                adl_close(myDevice);
                return ret;
            }
        }
#   else
        debugPrintLock();
        runDOSLoop(&taskMan, myDevice);
        debugPrintUnlock();
#   endif

        s_timeCounter.clearLine();
    }
#endif //OUTPUT_WAVE_ONLY

#ifndef ADLMIDI_ENABLE_HW_DOS

#   ifndef OUTPUT_WAVE_ONLY
    else
#   endif //OUTPUT_WAVE_ONLY
    {
        int ret = runWaveOutLoopLoop(myDevice, s_devSetup.musPath, s_devSetup.spec, s_devSetup.sampleRate);
        if(ret != 0)
        {
            adl_close(myDevice);
            return ret;
        }
    }
#endif //ADLMIDI_ENABLE_HW_DOS

    adl_close(myDevice);

    s_fprintf(stdout, "\n");
    flushout(stdout);

    return 0;
}
