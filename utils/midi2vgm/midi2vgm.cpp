/*
 * MIDI to VGM converter, an additional tool included with libADLMIDI library
 *
 * Copyright (c) 2015-2021 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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

#include <vector>
#include <string>
#include <cstdio>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <deque>
#include <algorithm>
#include <signal.h>

#include "compact/vgm_cmp.h"

#if defined(_MSC_VER) && _MSC_VER < 1900

#define snprintf c99_snprintf
#define vsnprintf c99_vsnprintf

__inline int c99_vsnprintf(char *outBuf, size_t size, const char *format, va_list ap)
{
    int count = -1;

    if (size != 0)
        count = _vsnprintf_s(outBuf, size, _TRUNCATE, format, ap);
    if (count == -1)
        count = _vscprintf(format, ap);

    return count;
}

__inline int c99_snprintf(char *outBuf, size_t size, const char *format, ...)
{
    int count;
    va_list ap;

    va_start(ap, format);
    count = c99_vsnprintf(outBuf, size, format, ap);
    va_end(ap);

    return count;
}
#endif

#include <adlmidi.h>

extern "C"
{
    extern ADLMIDI_DECLSPEC void adl_set_vgm_out_path(const char *path);
}

static void printError(const char *err)
{
    std::fprintf(stderr, "\nERROR: %s\n\n", err);
    std::fflush(stderr);
}

static int stop = 0;
static void sighandler(int dum)
{
    if((dum == SIGINT)
        || (dum == SIGTERM)
    #ifndef _WIN32
        || (dum == SIGHUP)
    #endif
    )
        stop = 1;
}


static void debugPrint(void * /*userdata*/, const char *fmt, ...)
{
    char buffer[4096];
    std::va_list args;
    va_start(args, fmt);
    int rc = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    if(rc > 0)
    {
        std::fprintf(stdout, " - Debug: %s\n", buffer);
        std::fflush(stdout);
    }
}

static void printBanks()
{
    // Get count of embedded banks (no initialization needed)
    int banksCount = adl_getBanksCount();
    //Get pointer to list of embedded bank names
    const char *const *banknames = adl_getBankNames();

    if(banksCount > 0)
    {
        std::printf("    Available embedded banks by number:\n\n");

        for(int a = 0; a < banksCount; ++a)
        {
            std::printf("%10s%2u = %s\n", a ? "" : "Banks:", a, banknames[a]);
        }

        std::printf(
            "\n"
            "     Use banks 2-5 to play Descent \"q\" soundtracks.\n"
            "     Look up the relevant bank number from descent.sng.\n"
            "\n"
            "     The fourth parameter can be used to specify the number\n"
            "     of four-op channels to use. Each four-op channel eats\n"
            "     the room of two regular channels. Use as many as required.\n"
            "     The Doom & Hexen sets require one or two, while\n"
            "     Miles four-op set requires the maximum of numcards*6.\n"
            "\n"
        );
    }
    else
    {
        std::printf("    This build of libADLMIDI has no embedded banks!\n\n");
    }

    flushout(stdout);
}


#ifdef DEBUG_TRACE_ALL_EVENTS
static void debugPrintEvent(void * /*userdata*/, ADL_UInt8 type, ADL_UInt8 subtype, ADL_UInt8 channel, const ADL_UInt8 * /*data*/, size_t len)
{
    std::fprintf(stdout, " - E: 0x%02X 0x%02X %02d (%d)\r\n", type, subtype, channel, (int)len);
    std::fflush(stdout);
}
#endif

static inline void secondsToHMSM(double seconds_full, char *hmsm_buffer, size_t hmsm_buffer_size)
{
    double seconds_integral;
    double seconds_fractional = std::modf(seconds_full, &seconds_integral);
    unsigned int milliseconds = static_cast<unsigned int>(seconds_fractional * 1000.0);
    unsigned int seconds = static_cast<unsigned int>(std::fmod(seconds_full, 60.0));
    unsigned int minutes = static_cast<unsigned int>(std::fmod(seconds_full / 60, 60.0));
    unsigned int hours   = static_cast<unsigned int>(seconds_full / 3600);
    std::memset(hmsm_buffer, 0, hmsm_buffer_size);
    if (hours > 0)
        snprintf(hmsm_buffer, hmsm_buffer_size, "%02u:%02u:%02u,%03u", hours, minutes, seconds, milliseconds);
    else
        snprintf(hmsm_buffer, hmsm_buffer_size, "%02u:%02u,%03u", minutes, seconds, milliseconds);
}


#ifndef DEFAULT_INSTALL_PREFIX
#define DEFAULT_INSTALL_PREFIX "/usr"
#endif

static std::string findDefaultBank()
{
    const size_t paths_count = sizeof(paths) / sizeof(const char*);
    std::string ret;

    for(size_t i = 0; i < paths_count; i++)
    {
        const char *p = paths[i];
        FILE *probe = std::fopen(p, "rb");
        if(probe)
        {
            std::fclose(probe);
            ret = std::string(p);
            break;
        }
    }

    return ret;
}

static bool is_number(const std::string &s)
{
    std::string::const_iterator it = s.begin();
    while(it != s.end() && std::isdigit(*it))
        ++it;
    return !s.empty() && it == s.end();
}


int main(int argc, char **argv)
{
    std::fprintf(stdout, "==========================================================\n"
                         "             MIDI to VGM conversion utility\n"
                         "==========================================================\n"
                         "   Source code: https://github.com/Wohlstand/libADLMIDI\n"
                         "==========================================================\n"
                         "\n");
    std::fflush(stdout);

    if(argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")
    {
        std::printf(
            "Usage:\n"
            "   midi2vgm [-s] [-w] [-l] [-na] [-z] [-frb] \\\n"
            "               [--chips <count>] <bank> <midifilename>\n"
            "\n"
            " <bank>   number of embeeded bank or filepath to custom WOPL bank file\n"
            " <midifilename>    Path to music file to play\n"
            "\n"
            " -l                Enables in-song looping support\n"
            " -s                Enables scaling of modulator volumes\n"
            " -vm <num> Chooses one of volume models: \n"
            "    0 auto (default)\n"
            "    1 Generic\n"
            "    2 Native OPL3\n"
            "    3 DMX\n"
            "    4 Apogee Sound System\n"
            "    5 9x SB16\n"
            "    6 DMX (Fixed AM voices)\n"
            "    7 Apogee Sound System (Fixed AM voices)\n"
            "    8 Audio Interface Library (AIL)\n"
            "    9 9x Generic FM\n"
            "   10 HMI Sound Operating System\n"
            " -na               Disables the automatical arpeggio\n"
            " -z                Make a compressed VGZ file\n"
            " -frb              Enables full-ranged CC74 XG Brightness controller\n"
            " -mc <nums>        Mute selected MIDI channels"
            "                     where <num> - space separated numbers list (0-based!):"
            "                     Example: \"-mc 2 5 6 will\" mute channels 2, 5 and 6.\n"
            " --chips <count>   Choose a count of chips (1 by default, 2 maximum)\n"
            "\n"
            "----------------------------------------------------------\n"
            "TIP-1: Single-chip mode limits polyphony up to 18 channels.\n"
            "       Use Dual-Chip mode (with \"--chips 2\" flag) to extend\n"
            "       polyphony up to 36 channels!\n"
            "\n"
            "TIP-2: Use addional WOPL bank files to alter sounding of generated song.\n"
            "\n"
            "TIP-3: Use \"vgm_cmp\" tool from 'VGM Tools' toolchain to optimize size of\n"
            "       generated VGM song when it appears too bug (300+, 500+, or even 1000+ KB).\n"
            "\n"
        );
        std::fflush(stdout);

        return 0;
    }

    int sampleRate = 44100;

    ADL_MIDIPlayer *myDevice;

    /*
     * Set library options by parsing of command line arguments
     */
    bool scaleModulators = false;
    bool makeVgz = false;
    bool fullRangedBrightness = false;
    int loopEnabled = 0;
    int volumeModel = ADLMIDI_VolumeModel_AUTO;
    int autoArpeggioEnabled = 1;
    size_t soloTrack = ~static_cast<size_t>(0);
    int chipsCount = 1;// Single-chip by default
    std::vector<int> muteChannels;

    std::string bankPath;
    std::string musPath;

    int arg = 1;
    for(arg = 1; arg < argc; arg++)
    {
        if(!std::strcmp("-frb", argv[arg]))
            fullRangedBrightness = true;
        else if(!std::strcmp("-s", argv[arg]))
            scaleModulators = true;
        else if(!std::strcmp("-z", argv[arg]))
            makeVgz = true;
        else if(!std::strcmp("-l", argv[arg]))
            loopEnabled = 1;
        else if(!std::strcmp("-na", argv[arg]))
            autoArpeggioEnabled = 0; //Enable automatical arpeggio
        else if(!std::strcmp("-vm", argv[arg]))
        {
            if(arg + 1 >= argc)
            {
                printError("The option -vm requires an argument!\n");
                return 1;
            }
            volumeModel = static_cast<int>(std::strtol(argv[++arg], NULL, 10));
        }
        else if(!std::strcmp("--chips", argv[arg]))
        {
            if(arg + 1 >= argc)
            {
                printError("The option --chips requires an argument!\n");
                return 1;
            }
            chipsCount = static_cast<int>(std::strtoul(argv[++arg], NULL, 0));
        }
        else if(!std::strcmp("--solo", argv[arg]))
        {
            if(arg + 1 >= argc)
            {
                printError("The option --solo requires an argument!\n");
                return 1;
            }
            soloTrack = std::strtoul(argv[++arg], NULL, 0);
        }
        else if(!std::strcmp("-mc", argv[arg]) || !std::strcmp("--mute-channels", argv[arg]))
        {
            if(arg + 1 >= argc)
            {
                printError("The option -mc/--mute-channels requires an argument!\n");
                return 1;
            }

            while(arg + 1 < argc && is_number(argv[arg + 1]))
            {
                int num = static_cast<int>(std::strtoul(argv[++arg], NULL, 0));
                if(num >= 0 && num <= 15)
                    muteChannels.push_back(num);
            }
        }
        else if(!std::strcmp("--", argv[arg]))
            break;
        else
            break;
    }

    if(arg == argc - 2)
    {
        bankPath = argv[arg];
        musPath = argv[arg + 1];
    }
    else if(arg == argc - 1)
    {
        std::fprintf(stdout, " - Bank is not specified, searching for default...\n");
        std::fflush(stdout);
        bankPath = findDefaultBank();
      /*  if(bankPath.empty())
        {
            printError("Missing default bank file xg.wopn!\n");
            return 2; */
        }
        musPath = argv[arg];
    }
    else if(arg > argc - 1)
    {
        printError("Missing music file path!\n");
        return 2;
    }

    std::string vgm_out = musPath + (makeVgz ? ".vgz" : ".vgm");
    adl _set_vgm_out_path(vgm_out.c_str());

    myDevice = adl_init(sampleRate);
    if(!myDevice)
    {
        std::fprintf(stderr, "Failed to init MIDI device!\n");
        return 1;
    }

    //Set internal debug messages hook to print all libADLMIDI's internal debug messages
    adl_setDebugMessageHook(myDevice, debugPrint, NULL);

    //Turn loop on/off (for WAV recording loop must be disabled!)
    if(scaleModulators)
        adl_setScaleModulators(myDevice, 1);//Turn on modulators scaling by volume
    if(fullRangedBrightness)
        adl_setFullRangeBrightness(myDevice, 1);//Turn on a full-ranged XG CC74 Brightness
    adl_setSoftPanEnabled(myDevice, 0);
    adl_setLoopEnabled(myDevice, loopEnabled);
    adl_setAutoArpeggio(myDevice, autoArpeggioEnabled);

#ifdef DEBUG_TRACE_ALL_EVENTS
    //Hook all MIDI events are ticking while generating an output buffer
    if(!recordWave)
        adl_setRawEventHook(myDevice, debugPrintEvent, NULL);
#endif

    if(adl_switchEmulator(myDevice, ADLMIDI_VGM_DUMPER) != 0)
    {
        std::fprintf(stdout, "FAILED!\n");
        std::fflush(stdout);
        printError(adl_errorInfo(myDevice));
        return 2;
    }

    std::fprintf(stdout, " - Library version %s\n", adl_linkedLibraryVersion());
    std::fprintf(stdout, " - %s Emulator in use\n", adl_chipEmulatorName(myDevice));

    std::fprintf(stdout, " - Use bank [%s]...", bankPath.c_str());
    std::fflush(stdout);

    if(adl_openBankFile(myDevice, bankPath.c_str()) != 0)
    {
        std::fprintf(stdout, "FAILED!\n");
        std::fflush(stdout);
        printError(adl_errorInfo(myDevice));
        return 2;
    }
    std::fprintf(stdout, "OK!\n");

    if(chipsCount < 0)
        chipsCount = 1;
    else if(chipsCount > 2)
        chipsCount = 2;
    adl_setNumChips(myDevice, chipsCount);

    if(volumeModel != ADLMIDI_VolumeModel_AUTO)
        adl_setVolumeRangeModel(myDevice, volumeModel);

    if(adl_openFile(myDevice, musPath.c_str()) != 0)
    {
        printError(adl_errorInfo(myDevice));
        return 2;
    }

    std::fprintf(stdout, " - Number of chips %d\n", adl_getNumChipsObtained(myDevice));
    std::fprintf(stdout, " - Track count: %lu\n", (unsigned long)adl_trackCount(myDevice));

    if(soloTrack != ~(size_t)0)
    {
        std::fprintf(stdout, " - Solo track: %lu\n", (unsigned long)soloTrack);
        adl_setTrackOptions(myDevice, soloTrack, ADLMIDI_TrackOption_Solo);
    }

    if(!muteChannels.empty())
    {
        std::printf(" - Mute MIDI channels:");

        for(size_t i = 0; i < muteChannels.size(); ++i)
        {
            adl_setChannelEnabled(myDevice, muteChannels[i], 0);
            std::printf(" %d", muteChannels[i]);
        }

        std::printf("\n");
        std::fflush(stdout);
    }

    std::fprintf(stdout, " - Automatic arpeggion is turned %s\n", autoArpeggioEnabled ? "ON" : "OFF");

    std::fprintf(stdout, " - File [%s] opened!\n", musPath.c_str());
    std::fflush(stdout);

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    #ifndef _WIN32
    signal(SIGHUP, sighandler);
    #endif

    double total        = adl_totalTimeLength(myDevice);
    double loopStart    = adl_loopStartTime(myDevice);
    double loopEnd      = adl_loopEndTime(myDevice);
    char totalHMS[25];
    char loopStartHMS[25];
    char loopEndHMS[25];
    secondsToHMSM(total, totalHMS, 25);
    if(loopStart >= 0.0 && loopEnd >= 0.0)
    {
        secondsToHMSM(loopStart, loopStartHMS, 25);
        secondsToHMSM(loopEnd, loopEndHMS, 25);
    }

    std::fprintf(stdout, " - Recording VGM file %s...\n", vgm_out.c_str());
    std::fprintf(stdout, "\n==========================================\n");
    std::fflush(stdout);

    short buff[4096];
    while(!stop)
    {
        size_t got = (size_t)adl_play(myDevice, 4096, buff);
        if(got <= 0)
            break;
    }
    std::fprintf(stdout, "                                               \n\n");

    if(stop)
    {
        std::fprintf(stdout, "Interrupted! Recorded VGM is incomplete, but playable!\n");
        std::fflush(stdout);
        adl_close(myDevice);
    }
    else
    {
        adl_close(myDevice);
        VgmCMP::vgm_cmp_main(vgm_out, makeVgz);
        std::fprintf(stdout, "Completed!\n");
        std::fflush(stdout);
    }

    return 0;
}
