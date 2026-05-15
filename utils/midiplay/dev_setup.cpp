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

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include "adlmidi.h"
#include "misc.h"
#include "dev_setup.h"


Args s_devSetup;
ADLMIDI_AudioFormat g_audioFormat;
float g_gaining = 2.0f;


static bool is_number(const std::string &s)
{
    std::string::const_iterator it = s.begin();
    while(it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

void printError(const char *err, const char *what)
{
    if(what)
        s_fprintf(stderr, "\nERROR (%s): %s\n\n", what, err);
    else
        s_fprintf(stderr, "\nERROR: %s\n\n", err);
    flushout(stderr);
}

static void printBanks()
{
    // Get count of embedded banks (no initialization needed)
    int banksCount = adl_getBanksCount();
    //Get pointer to list of embedded bank names
    const char *const *banknames = adl_getBankNames();

    if(banksCount > 0)
    {
        s_fprintf(stdout, "    Available embedded banks by number:\n\n");

        for(int a = 0; a < banksCount; ++a)
        {
            std::printf("%10s%2u = %s\n", a ? "" : "Banks:", a, banknames[a]);
#ifdef ADLMIDI_ENABLE_HW_DOS
            if(((a - 16) % 24 == 0 && a != 0))
                keyWait();
#endif
        }

        s_fprintf(stdout,
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
        s_fprintf(stdout, "    This build of libADLMIDI has no embedded banks!\n\n");
    }

    flushout(stdout);
}


Args::Args() :
    setHwVibrato(-1)
    , setHwTremolo(-1)
    , setScaleMods(-1)
    , setBankNo(-1)
    , setNum4op(-1)
    , setNumChips(-1)

    , setFullRangeBright(-1)
    , enableFullPanning(-1)

#ifndef OUTPUT_WAVE_ONLY
    , recordWave(false)
    , loopEnabled(1)
#endif

    , sampleRate(44100)

// #if !defined(ADLMIDI_ENABLE_HW_DOS) && !defined(OUTPUT_WAVE_ONLY)
//         , AudioBufferLength(0.08)
// #endif

#ifdef ADLMIDI_ENABLE_HW_DOS
    , setHwAddress(0)
    , setChipType(ADLMIDI_DOS_ChipAuto)
    , clock_freq(209)
#endif

#if defined(ADLMIDI_ENABLE_HW_SERIAL) && !defined(OUTPUT_WAVE_ONLY)
    , hwSerial(false)
    , serialBaud(115200)
    , serialProto(ADLMIDI_SerialProtocol_RetroWaveOPL3)
#endif
    , multibankFromEnbededTest(false)
    , autoArpeggioEnabled(0)
    , chanAlloc(ADLMIDI_ChanAlloc_AUTO)

#ifndef ADLMIDI_ENABLE_HW_DOS
    , emulator(ADLMIDI_EMU_NUKED)
#endif
    , volumeModel(ADLMIDI_VolumeModel_AUTO)
    , soloTrack(~(size_t)0)
    , songNumLoad(-1)
{
#if !defined(ADLMIDI_ENABLE_HW_DOS)
    spec.freq     = sampleRate;
    spec.format   = ADLMIDI_SampleType_S16;
#   if !defined(OUTPUT_WAVE_ONLY)
    spec.is_msb   = audio_is_big_endian();
#   else
    spec.is_msb   = 0;
#   endif
    spec.channels = 2;
    spec.samples  = 1024; //uint16_t((double)spec.freq * AudioBufferLength);
#endif
}

int Args::parseArgs(int argc, char **argv_arr, bool *quit)
{
    const char* const* argv = argv_arr;

    if(argc >= 2 && std::string(argv[1]) == "--list-banks")
    {
        printBanks();
        *quit = true;
        return 0;
    }

    if(argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")
    {
        const char *help_text =
            "Usage: adlmidi <midifilename> [ <options> ] \n"
            "                              [ <bank> [ <numchips> [ <numfourops>] ] ]\n"
            "\n"
            //------------------------------------------------------------------------------|
            "Where <bank> -     number of embeeded bank or filepath to custom WOPL bank file\n"
            "Where <numchips> - total number of parallel emulated chips running to\n"
            "                   extend poliphony (on hardware 1 chip can only be used)\n"
            "Where <numfourops> - total number of 4-operator channels on OPL3 chips.\n"
            "                     By defaullt value depends on the used bank.\n"
            "\n"
            // " -p Enables adlib percussion instrument mode\n"
            " -t Enables force deep tremolo mode (Default: depens on bank)\n"
            " -v Enables force deep vibrato mode (Default: depens on bank)\n"
            " -s Enables scaling of modulator volumes\n"
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
            "   11 Old HMI Sound Operating System (Some instruments might distort)\n"
            "   12 MS Adlib Driver for Win3x\n"
            "   13 IMF Creator\n"
            "   14 Jammie O'Connel's FM Synth\n"
            " -frb  Enables full-ranged CC74 XG Brightness controller\n"
            " -nl   Quit without looping\n"
            " -w    Write WAV file rather than playing\n"
            //------------------------------------------------------------------------------|
            " -mb   Run the test of multibank over embedded. 62, 14, 68, and 74'th banks\n"
            "       will be combined into one\n"
            " --solo <track>             Selects a solo track to play\n"
            " --only <track1,...,trackN> Selects a subset of tracks to play\n"
            " --song <song ID 0...N-1>   Selects a song to play (if XMI)\n"
            " -ea   Enable the auto-arpeggio\n"
#ifndef ADLMIDI_ENABLE_HW_DOS
            " -fp Enables full-panning stereo support\n"
            " --gain <value> Set the gaining factor (default 2.0)\n"
#   ifndef ADLMIDI_DISABLE_NUKED_EMULATOR
            " --emu-nuked  Uses Nuked OPL3 v 1.8 emulator\n"
            " --emu-nuked7 Uses Nuked OPL3 Fast emulator\n"
            " --emu-nuked-opl2 Uses Nuked OPL2 Lite emulator\n"
#   endif
#   ifndef ADLMIDI_DISABLE_DOSBOX_EMULATOR
            " --emu-dosbox Uses DosBox 0.74 OPL3 emulator\n"
#   endif
#   ifndef ADLMIDI_DISABLE_OPAL_EMULATOR
            " --emu-opal   Uses Opal OPL3 emulator\n"
#   endif
#   ifndef ADLMIDI_DISABLE_JAVA_EMULATOR
            " --emu-java   Uses Java OPL3 emulator\n"
#   endif
#   ifndef ADLMIDI_DISABLE_ESFMU_EMULATOR
            " --emu-esfmu  Uses ESFMu OPL3/ESFM emulator\n"
#   endif
#   ifndef ADLMIDI_DISABLE_MAME_OPL2_EMULATOR
            " --emu-mame-opl2  Uses MAME OPL2 emulator\n"
#   endif
#   ifndef ADLMIDI_DISABLE_YMFM_EMULATOR
            " --emu-ymfm-opl2  Uses YMFM OPL2 emulator\n"
            " --emu-ymfm-opl3  Uses YMFM OPL2 emulator\n"
#   endif
#   ifdef ADLMIDI_ENABLE_OPL2_LLE_EMULATOR
            " --emu-lle-opl2 Uses Nuked OPL2-LLE emulator !!EXTRA HEAVY!!\n"
#   endif
#   ifdef ADLMIDI_ENABLE_OPL3_LLE_EMULATOR
            " --emu-lle-opl3 Uses Nuked OPL3-LLE emulator !!EXTRA HEAVY!!\n"
#   endif
#else
            "\n"
            //------------------------------------------------------------------------------|
            " --time-freq <hz>      Uses a different time value, DEFAULT 209\n"
            " --type <opl2|opl3>    Type of hardware chip ('opl2' or 'opl3'), default AUTO\n"
            " --addr <hex>          Hardware address of the chip, DEFAULT 0x388\n"
            " --list-banks          Print a lost of all built-in FM banks\n"
#endif
            "\n"
            //------------------------------------------------------------------------------|
            "Note: To create WOPL bank files use OPL Bank Editor you can get here: \n"
            "https://github.com/Wohlstand/OPL3BankEditor\n"
#ifdef ADLMIDI_ENABLE_HW_DOS
            "\n\n"
            //------------------------------------------------------------------------------|
            "TIP: If you have the SoundBlaster Pro with Dual OPL2, you can use two cips\n"
            "if you specify the base address of sound card itself (for example 0x220) and\n"
            "set two chips. However, keep a note that SBPro's chips were designed for the\n"
            "Stereo, not for polyphony, and therefore, you will hear voices randomly going\n"
            "between left and right speaker.\n"
#endif
            "\n"
            ;

#ifdef ADLMIDI_ENABLE_HW_DOS
        int lines = 5;
        const char *cur = help_text;

        for(; *cur != '\0'; ++cur)
        {
            char c = *cur;
            std::putc(c, stdout);
            if(c == '\n')
                lines++;

            if(lines >= 24)
            {
                keyWait();
                lines = 0;
            }
        }
#else
        s_fprintf(stdout, "%s", help_text);
        flushout(stdout);
#endif

#ifndef ADLMIDI_ENABLE_HW_DOS
        printBanks();
#endif
        *quit = true;
        return 0;
    }

    musPath = argv[1];

    while(argc > 2)
    {
        bool had_option = false;

        if(!std::strcmp("-p", argv[2]))
            s_fprintf(stderr, "Warning: -p argument is deprecated and useless!\n"); //adl_setPercMode(myDevice, 1);//Turn on AdLib percussion mode
        else if(!std::strcmp("-v", argv[2]))
            setHwVibrato = 1;

#if !defined(ADLMIDI_ENABLE_HW_DOS)
#   if !defined(OUTPUT_WAVE_ONLY)
        else if(!std::strcmp("-w", argv[2]))
        {
            //Current Wave output implementation allows only SINT16 output
            g_audioFormat.type = ADLMIDI_SampleType_S16;
            g_audioFormat.containerSize = sizeof(ADL_SInt16);
            g_audioFormat.sampleOffset = sizeof(ADL_SInt16) * 2;
            recordWave = true;//Record library output into WAV file
        }
#   endif
        else if(!std::strcmp("-s8", argv[2]))
            spec.format = ADLMIDI_SampleType_S8;
        else if(!std::strcmp("-u8", argv[2]))
            spec.format = ADLMIDI_SampleType_U8;
        else if(!std::strcmp("-s16", argv[2]))
            spec.format = ADLMIDI_SampleType_S16;
        else if(!std::strcmp("-u16", argv[2]))
            spec.format = ADLMIDI_SampleType_U16;
        else if(!std::strcmp("-s32", argv[2]))
            spec.format = ADLMIDI_SampleType_S32;
        else if(!std::strcmp("-f32", argv[2]))
            spec.format = ADLMIDI_SampleType_F32;
#endif

        else if(!std::strcmp("-t", argv[2]))
            setHwTremolo = 1;

        else if(!std::strcmp("-frb", argv[2]))
            setFullRangeBright = 1;

#ifndef OUTPUT_WAVE_ONLY
        else if(!std::strcmp("-nl", argv[2]))
            loopEnabled = 0; //Enable loop
#endif
        else if(!std::strcmp("-na", argv[2])) // Deprecated
            autoArpeggioEnabled = 0; //Enable auto-arpeggio
        else if(!std::strcmp("-ea", argv[2]))
            autoArpeggioEnabled = 1; //Enable auto-arpeggio

#ifndef ADLMIDI_ENABLE_HW_DOS
        else if(!std::strcmp("--emu-nuked", argv[2]))
            emulator = ADLMIDI_EMU_NUKED;
        else if(!std::strcmp("--emu-nuked7", argv[2]))
            emulator = ADLMIDI_EMU_NUKED_174;
        else if(!std::strcmp("--emu-nuked-opl2", argv[2]))
            emulator = ADLMIDI_EMU_NUKED_OPL2_LITE;
        else if(!std::strcmp("--emu-dosbox", argv[2]))
            emulator = ADLMIDI_EMU_DOSBOX;
        else if(!std::strcmp("--emu-opal", argv[2]))
            emulator = ADLMIDI_EMU_OPAL;
        else if(!std::strcmp("--emu-java", argv[2]))
            emulator = ADLMIDI_EMU_JAVA;
        else if(!std::strcmp("--emu-esfmu", argv[2]))
            emulator = ADLMIDI_EMU_ESFMu;
        else if(!std::strcmp("--emu-mame-opl2", argv[2]))
            emulator = ADLMIDI_EMU_MAME_OPL2;
        else if(!std::strcmp("--emu-ymfm-opl2", argv[2]))
            emulator = ADLMIDI_EMU_YMFM_OPL2;
        else if(!std::strcmp("--emu-ymfm-opl3", argv[2]))
            emulator = ADLMIDI_EMU_YMFM_OPL3;
        else if(!std::strcmp("--emu-lle-opl2", argv[2]))
            emulator = ADLMIDI_EMU_NUKED_OPL2_LLE;
        else if(!std::strcmp("--emu-lle-opl3", argv[2]))
            emulator = ADLMIDI_EMU_NUKED_OPL3_LLE;
#endif

#if defined(ADLMIDI_ENABLE_HW_SERIAL) && !defined(OUTPUT_WAVE_ONLY)
        else if(!std::strcmp("--serial", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option --serial requires an argument!\n");
                *quit = true;
                return 1;
            }
            had_option = true;
            hwSerial = true;
            serialName = argv[3];
        }
        else if(!std::strcmp("--serial-baud", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option --serial-baud requires an argument!\n");
                *quit = true;
                return 1;
            }
            had_option = true;
            serialBaud = std::strtol(argv[3], NULL, 10);
        }
        else if(!std::strcmp("--serial-proto", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option --serial-proto requires an argument!\n");
                *quit = true;
                return 1;
            }
            had_option = true;
            serialProto = std::strtol(argv[3], NULL, 10);
        }
#endif
        else if(!std::strcmp("-fp", argv[2]))
            enableFullPanning = 1;
        else if(!std::strcmp("-mb", argv[2]))
            multibankFromEnbededTest = true;
        else if(!std::strcmp("-s", argv[2]))
            setScaleMods = 1;
#ifndef ADLMIDI_HW_OPL
        else if(!std::strcmp("--gain", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option --gain requires an argument!\n");
                *quit = true;
                return 1;
            }
            had_option = true;
            g_gaining = std::atof(argv[3]);
        }
#endif // ADLMIDI_ENABLE_HW_DOS

#ifdef ADLMIDI_ENABLE_HW_DOS
        else if(!std::strcmp("--time-freq", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option --time-freq requires an argument!\n");
                *quit = true;
                return 1;
            }

            clock_freq = std::strtoul(argv[3], NULL, 0);
            if(clock_freq == 0)
            {
                printError("The option --time-freq requires a non-zero integer argument!\n");
                *quit = true;
                return 1;
            }

            had_option = true;
        }
        else if(!std::strcmp("--type", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option --type requires an argument!\n");
                *quit = true;
                return 1;
            }

            if(!std::strcmp(argv[3], "opl3") || !std::strcmp(argv[3], "OPL3"))
                setChipType = ADLMIDI_DOS_ChipOPL3;
            else if(!std::strcmp(argv[3], "opl2") || !std::strcmp(argv[3], "OPL2"))
                setChipType = ADLMIDI_DOS_ChipOPL2;
            else
            {
                printError("Given invalid option for --type: accepted 'opl2' or 'opl3'!\n");
                *quit = true;
                return 1;
            }

            had_option = true;
        }
        else if(!std::strcmp("--addr", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option --addr requires an argument!\n");
                *quit = true;
                return 1;
            }

            setHwAddress = std::strtoul(argv[3], NULL, 0);
            if(setHwAddress == 0)
            {
                printError("The option --time-freq requires a non-zero integer argument!\n");
                *quit = true;
                return 1;
            }

            had_option = true;
        }
#endif
        else if(!std::strcmp("-vm", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option -vm requires an argument!\n");
                *quit = true;
                return 1;
            }
            volumeModel = std::strtol(argv[3], NULL, 10);
            had_option = true;
        }
        else if(!std::strcmp("-ca", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option -carequires an argument!\n");
                *quit = true;
                return 1;
            }

            chanAlloc = std::strtol(argv[3], NULL, 10);
            had_option = true;
        }
        else if(!std::strcmp("--solo", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option --solo requires an argument!\n");
                *quit = true;
                return 1;
            }
            soloTrack = std::strtoul(argv[3], NULL, 10);
            had_option = true;
        }
        else if(!std::strcmp("--song", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option --song requires an argument!\n");
                *quit = true;
                return 1;
            }
            songNumLoad = std::strtol(argv[3], NULL, 10);
            had_option = true;
        }
        else if(!std::strcmp("--only", argv[2]))
        {
            if(argc <= 3)
            {
                printError("The option --only requires an argument!\n");
                *quit = true;
                return 1;
            }

            const char *strp = argv[3];
            unsigned long value;
            unsigned size;
            bool err = std::sscanf(strp, "%lu%n", &value, &size) != 1;
            while(!err && *(strp += size))
            {
                onlyTracks.push_back(value);
                err = std::sscanf(strp, ",%lu%n", &value, &size) != 1;
            }
            if(err)
            {
                printError("Invalid argument to --only!\n");
                *quit = true;
                return 1;
            }
            onlyTracks.push_back(value);
            had_option = true;
        }
        else
            break;

        argv += (had_option ? 2 : 1);
        argc -= (had_option ? 2 : 1);
    }

    if(argc >= 3)
    {
        if(is_number(argv[2]))
            setBankNo = std::atoi(argv[2]);
        else
            setBankFile = argv[2];
    }

    if(argc >= 4)
        setNumChips = std::atoi(argv[3]);

    if(argc >= 5)
        setNum4op = std::atoi(argv[4]);

    *quit = false;
    return 0;
}
