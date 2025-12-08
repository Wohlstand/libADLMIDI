/*
 * libADLMIDI is a free Software MIDI synthesizer library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2025 Vitaly Novichkov <admin@wohlnet.ru>
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

//#ifdef __MINGW32__
//typedef struct vswprintf {} swprintf;
//#endif
#include <cstdio>
#include <string>
#include <cstring>
#include <map>

// #define __STDC_FORMAT_MACROS
// #include <inttypes.h>

#include "ini/ini_processing.h"

#include "progs_cache.h"
#include "measurer.h"

// #include "midi_inst_list.h"
static int processBank(IniProcessing &ini, const std::string &banksRoot, BanksDump &db, size_t bank_id)
{
    std::string bank_name;
    std::string filepath;
    std::string filepath_d;
    std::string prefix;
    std::string prefix_d;
    std::string filter_m;
    std::string filter_p;
    std::string format;
    bool noRhythmMode = false;
    bool mt32defaults = false;

    ini.read("name",     bank_name, "Untitled");
    ini.read("format",   format,    "Unknown");
    ini.read("file",     filepath,  "");
    ini.read("file-p",   filepath_d,  "");

    if(!filepath.empty())
        filepath = banksRoot + "/" + filepath;

    if(!filepath_d.empty())
        filepath_d = banksRoot + "/" + filepath_d;

    ini.read("prefix",   prefix, "");
    ini.read("prefix-p", prefix_d, "");
    ini.read("filter-m", filter_m, "");
    ini.read("filter-p", filter_p, "");
    ini.read("no-rhythm-mode", noRhythmMode, false);
    ini.read("mt32-defaults", mt32defaults, false);

    if(filepath.empty())
    {
        std::fprintf(stderr, "Failed to load bank %u, file is empty!\n", (unsigned)bank_id);
        return 1;
    }

    std::fprintf(stdout, "Loading bank: %03u = %s...\n", (unsigned)bank_id, bank_name.c_str());
    std::fflush(stdout);

    if(format == "AIL")
    {
        if(!BankFormats::LoadMiles(db, filepath.c_str(), bank_id, bank_name, prefix.c_str(), mt32defaults))
        {
            std::fprintf(stderr, "Failed to load bank %u, file %s!\n", (unsigned)bank_id, filepath.c_str());
            return 1;
        }
    }
    else
    if(format == "Bisqwit")
    {
        if(!BankFormats::LoadBisqwit(db, filepath.c_str(), bank_id, bank_name, prefix.c_str()))
        {
            std::fprintf(stderr, "Failed to load bank %u, file %s!\n", (unsigned)bank_id, filepath.c_str());
            return 1;
        }
    }
    else
    if(format == "WOPL")
    {
        if(!BankFormats::LoadWopl(db, filepath.c_str(), bank_id, bank_name, prefix.c_str()))
        {
            std::fprintf(stderr, "Failed to load bank %u, file %s!\n", (unsigned)bank_id, filepath.c_str());
            return 1;
        }
    }
    else
    if(format == "WOPLX")
    {
        if(!BankFormats::LoadWoplX(db, filepath.c_str(), bank_id, bank_name, prefix.c_str()))
        {
            std::fprintf(stderr, "Failed to load bank %u, file %s!\n", (unsigned)bank_id, filepath.c_str());
            return 1;
        }
    }
    else
    if(format == "OP2")
    {
        if(!BankFormats::LoadDoom(db, filepath.c_str(), bank_id, bank_name, prefix.c_str()))
        {
            std::fprintf(stderr, "Failed to load bank %u, file %s!\n", (unsigned)bank_id, filepath.c_str());
            return 1;
        }
    }
    else
    if(format == "EA")
    {
        if(!BankFormats::LoadEA(db, filepath.c_str(), bank_id, bank_name, prefix.c_str()))
        {
            std::fprintf(stderr, "Failed to load bank %u, file %s!\n", (unsigned)bank_id, filepath.c_str());
            return 1;
        }
    }
    else
    if(format == "TMB")
    {
        if(!BankFormats::LoadTMB(db, filepath.c_str(), bank_id, bank_name, prefix.c_str()))
        {
            std::fprintf(stderr, "Failed to load bank %u, file %s!\n", (unsigned)bank_id, filepath.c_str());
            return 1;
        }
    }
    else
    if(format == "Junglevision")
    {
        if(!BankFormats::LoadJunglevision(db, filepath.c_str(), bank_id, bank_name, prefix.c_str()))
        {
            std::fprintf(stderr, "Failed to load bank %u, file %s!\n", (unsigned)bank_id, filepath.c_str());
            return 1;
        }
    }
    else
    if(format == "AdLibGold")
    {
        if(!BankFormats::LoadBNK2(db, filepath.c_str(), bank_id, bank_name, prefix.c_str(), filter_m, filter_p))
        {
            std::fprintf(stderr, "Failed to load bank %u, file %s!\n", (unsigned)bank_id, filepath.c_str());
            return 1;
        }
    }
    else
    if(format == "HMI")
    {
        if(!BankFormats::LoadBNK(db, filepath.c_str(),  bank_id, bank_name, prefix.c_str(),   false, false))
        {
            std::fprintf(stderr, "Failed to load bank %u, file %s!\n", (unsigned)bank_id, filepath.c_str());
            return 1;
        }

        if(!filepath_d.empty())
        {
            if(!BankFormats::LoadBNK(db, filepath_d.c_str(), bank_id, bank_name, prefix_d.c_str(), false, true))
            {
                std::fprintf(stderr, "Failed to load bank %u, file %s!\n", (unsigned)bank_id, filepath.c_str());
                return 1;
            }
        }
    }
    else
    if(format == "IBK")
    {
        if(!BankFormats::LoadIBK(db, filepath.c_str(),  bank_id, bank_name, prefix.c_str(),   false, false, mt32defaults))
        {
            std::fprintf(stderr, "Failed to load bank %u, file %s!\n", (unsigned)bank_id, filepath.c_str());
            return 1;
        }

        if(!filepath_d.empty())
        {
            //printf("Loading %s... \n", filepath_d.c_str());
            if(!BankFormats::LoadIBK(db, filepath_d.c_str(),bank_id, bank_name, prefix_d.c_str(), true, noRhythmMode, mt32defaults))
            {
                std::fprintf(stderr, "Failed to load bank %u, file %s!\n", (unsigned)bank_id, filepath.c_str());
                return 1;
            }
        }
    }
    else
    {
        std::fprintf(stderr, "Failed to load bank %u, file %s!\nUnknown format type %s\n",
                (unsigned)bank_id,
                filepath.c_str(),
                format.c_str());
        return 1;
    }

    return 0;
}


int main(int argc, char**argv)
{
    if(argc < 4)
    {
        std::printf("Usage:\n"
               "\n"
               "bin/gen_adldata banks.ini src/inst_db.cpp adldata-cache.dat\n"
               "\n");
        return 1;
    }

    const char *iniFile_s = argv[1];
    const char *outFile_s = argv[2];
    const char *cacheFile_s = argv[3];

    BanksDump db;

    {
        IniProcessing ini;
        if(!ini.open(iniFile_s))
        {
            std::fprintf(stderr, "Can't open %s!\n", iniFile_s);
            return 1;
        }

        std::string banksRoot(iniFile_s);
        bool banksRootFoundSlash = false;

        for(auto it = banksRoot.end() - 1; it != banksRoot.begin(); --it)
        {
            if(*it == '/' || *it == '\\')
            {
                banksRoot.erase(it, banksRoot.end());
                banksRootFoundSlash = true;
                break;
            }
        }

        if(!banksRootFoundSlash)
            banksRoot = "."; // Relative path to current directory

        std::fprintf(stdout, "Looking for banks at %s...\n", banksRoot.c_str());
        fflush(stdout);

        uint32_t banks_count;
        bool auto_id = false;

        ini.beginGroup("General");
        ini.read("banks", banks_count, 0);
        ini.read("auto-id", auto_id, false);
        ini.endGroup();

        if(!banks_count && !auto_id)
        {
            std::fprintf(stderr, "Zero count of banks found in %s!\n", iniFile_s);
            return 1;
        }

        if(auto_id) // All IDs will be counted from the scratch
        {
            std::vector<std::string> keys = ini.childGroups();
            std::map<std::string, std::string> keysOrdered;

            banks_count = 0;

            for(std::vector<std::string>::iterator key = keys.begin(); key != keys.end(); ++key)
            {
                std::string keySort;
                if(key->compare(0, 5, "bank-") != 0)
                {
                    std::fprintf(stderr, "Wrong key! (%s)\n", key->c_str());
                    continue; // Not our key!
                }

                if(!ini.beginGroup(*key))
                {
                    std::fprintf(stderr, "Failed to open section bank %s!\n", key->c_str());
                    return 1;
                }

                ini.read("name", keySort, "Untitled");
                keySort += *key;
                keysOrdered.insert({keySort, *key});

                ini.endGroup();
            }

            for(std::map<std::string, std::string>::iterator key = keysOrdered.begin(); key != keysOrdered.end(); ++key)
            {
                if(!ini.beginGroup(key->second))
                {
                    std::fprintf(stderr, "Failed to open section bank %s!\n", key->second.c_str());
                    return 1;
                }

                int ret = processBank(ini, banksRoot, db, banks_count);

                if(ret != 0)
                    return ret;

                ini.endGroup();

                ++banks_count;
            }

            if(banks_count == 0)
            {
                std::fprintf(stderr, "No valid banks found!\n");
                return 1;
            }
        }
        else for(uint32_t bank = 0; bank < banks_count; bank++) // Count banks by strict ID
        {
            if(!ini.beginGroup(std::string("bank-") + std::to_string(bank)))
            {
                std::fprintf(stderr, "Failed to find bank %u!\n", bank);
                return 1;
            }

            int ret = processBank(ini, banksRoot, db, bank);

            if(ret != 0)
                return ret;

            ini.endGroup();
        }

        std::printf("Loaded %u banks!\n", banks_count);
        std::fflush(stdout);
    }

    MeasureThreaded measureCounter;
    bool dontOverride = false;

    {
        measureCounter.LoadCache(cacheFile_s);
        measureCounter.m_cache_matches = 0;
        measureCounter.m_done = 0;
        measureCounter.m_total = db.instruments.size();

        std::printf("Beginning to generate measures data... (hardware concurrency of %d)\n", std::thread::hardware_concurrency());
        std::fflush(stdout);
        for(size_t b = 0; b < db.instruments.size(); ++b)
        {
            assert(db.instruments[b].instId == b);
            measureCounter.run(db, db.instruments[b]);
        }
        std::fflush(stdout);

        measureCounter.waitAll();

        if(measureCounter.m_cache_matches != measureCounter.m_total)
        {
            std::printf("-- Cache data was changed, saving...\n");
            std::fflush(stdout);

            if(!measureCounter.SaveCache(cacheFile_s))
            {
                std::fprintf(stderr, "-- FAILED TO SAVE THE CACHE! Data will not be overriden! --\n");
                std::fflush(stderr);
                dontOverride = true;
            }
            else
                dontOverride = false;
        }
        else
        {
            std::printf("-- Cache data was not changes.\n");
            std::fflush(stdout);
            dontOverride = true;
        }
    }

    db.exportBanks(std::string(outFile_s), dontOverride);

    std::printf("Generation of ADLMIDI data has been completed!\n");
    std::fflush(stdout);
}
