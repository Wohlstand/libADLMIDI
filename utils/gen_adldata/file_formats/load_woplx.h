/*
 * libADLMIDI is a free Software MIDI synthesizer library with OPL3 emulation
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

#ifndef LOAD_WOPLX_H
#define LOAD_WOPLX_H

#include "../progs_cache.h"

static const char *woplx_magic      = "WOPLX-BANK\n";
static const char *woplx_magic_r    = "WOPLX-BANK\r\n"; // Allow files with CRLF too

static void trim(const char *in, size_t in_len, char *out)
{
    const char *begin = in;
    const char *end = in + in_len;

    while(begin < end && *begin && (*begin == ' ' || *begin == '\t' || *begin == '\r' || *begin == '\n'))
        ++begin;

    if(begin >= end || *begin == 0)
    {
        *out = 0;
        return; // Empty!
    }

    do
    {
        --end;
    } while(end >= begin && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'));

    ++end;

    if(end == begin)
        *out = 0;
    else
    {
        for( ; begin < end; ++begin)
            *(out++) = *begin;

        *out = 0;
    }
}

static bool str_starts_with(const char *line, size_t line_len, const char *needle)
{
    size_t needle_len = std::strlen(needle);

    if(needle_len > line_len)
        return false; // input shorter than needle!

    return std::strncmp(line, needle, needle_len) == 0;
}

static bool woplx_read_inst_line(const char *line, size_t line_len, std::string &name, BanksDump::InstrumentEntry *curInst, InstBuffer *tmp)
{
    unsigned readU;
    int readI;

    if(str_starts_with(line, line_len, "NAME="))
    {
        size_t i, o;

        name.resize(32);

        for(i = 5, o = 0; o < 33 && i < line_len && line[i] != '\n' && line[i] != '\r'; ++i, ++o)
            name[o] = line[i];

        name.resize(o);
        return true;
    }
    else if(str_starts_with(line, line_len, "FLAGS:"))
    {
        int opFlags = 0;

        if(std::strstr(line, "FN;"))
            curInst->instFlags |= BanksDump::InstrumentEntry::WOPL_Ins_FixedNote;

        if(std::strstr(line, "2OP;"))
        {
            curInst->instFlags |= BanksDump::InstrumentEntry::WOPL_Ins_2op;
            ++opFlags;
        }

        if(std::strstr(line, "DV;"))
        {
            curInst->instFlags |= BanksDump::InstrumentEntry::WOPL_Ins_4op | BanksDump::InstrumentEntry::WOPL_Ins_Pseudo4op;
            ++opFlags;
        }

        if(std::strstr(line, "4OP;"))
        {
            curInst->instFlags |= BanksDump::InstrumentEntry::WOPL_Ins_4op;
            ++opFlags;
        }

        if(opFlags != 1)
        {
            // Illegal combo!
            return false;
        }

        return true;
    }
    else if(str_starts_with(line, line_len, "ATTRS:"))
    {
        const char *key;

        if((key = std::strstr(line, "DRUM_KEY=")) != NULL)
        {
            if(std::sscanf(key, "DRUM_KEY=%u;", &readU) == 0)
                return false;

            curInst->percussionKeyNumber = readU;
        }

        if((key = std::strstr(line, "NOTE_OFF_1=")) != NULL)
        {
            if(std::sscanf(key, "NOTE_OFF_1=%d;", &readI) == 0)
                return false;

            curInst->noteOffset1 = (int8_t)readI;
        }

        if((key = std::strstr(line, "NOTE_OFF_2=")) != NULL)
        {
            if(std::sscanf(key, "NOTE_OFF_2=%d;", &readI) == 0)
                return false;

            curInst->noteOffset2 = (int8_t)readI;
        }

        if((key = std::strstr(line, "VEL_OFF=")) != NULL)
        {
            if(std::sscanf(key, "VEL_OFF=%d;", &readI) == 0)
                return false;

            curInst->midiVelocityOffset = (int8_t)readI;
        }

        if((key = std::strstr(line, "FINE_TUNE=")) != NULL)
        {
            if(std::sscanf(key, "FINE_TUNE=%d;", &readI) == 0)
                return false;

            curInst->secondVoiceDetune = (int8_t)readI;
        }

        if((key = std::strstr(line, "RHYTHM=")) != NULL)
        {
            if(std::sscanf(key, "RHYTHM=%u;", &readU) == 0)
                return false;

            if(readU != 0)
            {
                switch(readU)
                {
                case 6:
                    curInst->instFlags |= BanksDump::InstrumentEntry::WOPL_RM_BassDrum;
                    break;
                case 7:
                    curInst->instFlags |= BanksDump::InstrumentEntry::WOPL_RM_Snare;
                    break;
                case 8:
                    curInst->instFlags |= BanksDump::InstrumentEntry::WOPL_RM_TomTom;
                    break;
                case 9:
                    curInst->instFlags |= BanksDump::InstrumentEntry::WOPL_RM_Cymbal;
                    break;
                case 10:
                    curInst->instFlags |= BanksDump::InstrumentEntry::WOPL_RM_HiHat;
                    break;
                }
            }
        }

        if((key = std::strstr(line, "DUR_K_ON=")) != NULL)
        {
            if(std::sscanf(key, "DUR_K_ON=%u;", &readU) == 0)
                return false;

            curInst->delay_on_ms = readU;
        }

        if((key = std::strstr(line, "DUR_K_OFF=")) != NULL)
        {
            if(std::sscanf(key, "DUR_K_OFF=%u;", &readU) == 0)
                return false;

            curInst->delay_off_ms = readU;
        }

        return true;
    }
    else if(str_starts_with(line, line_len, "FBCONN:"))
    {
        const char *key;

        if((key = std::strstr(line, "FB1=")) != NULL)
        {
            if(std::sscanf(key, "FB1=%u;", &readU) == 0)
                return false;

            curInst->fbConn |= (readU << 1) & 0x0E;
        }

        if((key = std::strstr(line, "FB2=")) != NULL)
        {
            if(std::sscanf(key, "FB2=%u;", &readU) == 0)
                return false;

            curInst->fbConn |= ((readU << 1) & 0x0E) << 8;
        }

        if((key = std::strstr(line, "CONN1=")) != NULL)
        {
            if(std::sscanf(key, "CONN1=%u;", &readU) == 0)
                return false;

            curInst->fbConn |= readU & 0x01;
        }

        if((key = std::strstr(line, "CONN2=")) != NULL)
        {
            if(std::sscanf(key, "CONN2=%u;", &readU) == 0)
                return false;

            curInst->fbConn |= (readU & 0x01) << 8;
        }

        return true;
    }
    else if(str_starts_with(line, line_len, "OP0:") || str_starts_with(line, line_len, "OP1:") ||
            str_starts_with(line, line_len, "OP2:") || str_starts_with(line, line_len, "OP3:"))
    {
        const char *key;
        size_t op;

        if(std::sscanf(line, "OP%u: ", &readU) == 0)
            return false;

        op = readU;

        //FmBank::Operator &opr = curInst->OP[op];
#define SET_OP(op, key, val) \
        switch(op) \
        { \
        case 1: \
            tmp[0].d.op1_ ## key |= (val); \
            break; \
        case 0: \
            tmp[0].d.op2_ ## key |= (val); \
            break; \
        case 3: \
            tmp[1].d.op1_ ## key |= (val); \
            break; \
        case 2: \
            tmp[1].d.op2_ ## key |= (val); \
            break; \
        }

        if((key = std::strstr(line, "AT=")) != NULL)
        {
            if(std::sscanf(key, "AT=%u;", &readU) == 0)
                return false;

            SET_OP(op, atdec, (readU << 4) & 0xF0)
        }

        if((key = std::strstr(line, "DC=")) != NULL)
        {
            if(std::sscanf(key, "DC=%u;", &readU) == 0)
                return false;

            SET_OP(op, atdec, readU & 0x0F)
        }

        if((key = std::strstr(line, "ST=")) != NULL)
        {
            if(std::sscanf(key, "ST=%u;", &readU) == 0)
                return false;

            SET_OP(op, susrel, (readU << 4) & 0xF0)
        }

        if((key = std::strstr(line, "RL=")) != NULL)
        {
            if(std::sscanf(key, "RL=%u;", &readU) == 0)
                return false;

            SET_OP(op, susrel, readU & 0x0F)
        }

        if((key = std::strstr(line, "WF=")) != NULL)
        {
            if(std::sscanf(key, "WF=%u;", &readU) == 0)
                return false;

            SET_OP(op, wave, readU & 0x07)
        }

        if((key = std::strstr(line, "ML=")) != NULL)
        {
            if(std::sscanf(key, "ML=%u;", &readU) == 0)
                return false;

            SET_OP(op, amvib, readU & 0x0F)
        }

        if((key = std::strstr(line, "TL=")) != NULL)
        {
            if(std::sscanf(key, "TL=%u;", &readU) == 0)
                return false;

            SET_OP(op, ksltl, readU & 0x3F)
        }

        if((key = std::strstr(line, "KL=")) != NULL)
        {
            if(std::sscanf(key, "KL=%u;", &readU) == 0)
                return false;

            SET_OP(op, ksltl, (readU << 6) & 0xC0)
        }

        if((key = std::strstr(line, "VB=")) != NULL)
        {
            if(std::sscanf(key, "VB=%u;", &readU) == 0)
                return false;

            SET_OP(op, amvib, (readU << 6) & 0x40)
        }

        if((key = std::strstr(line, "AM=")) != NULL)
        {
            if(std::sscanf(key, "AM=%u;", &readU) == 0)
                return false;

            SET_OP(op, amvib, (readU << 7) & 0x80)
        }

        if((key = std::strstr(line, "EG=")) != NULL)
        {
            if(std::sscanf(key, "EG=%u;", &readU) == 0)
                return false;

            SET_OP(op, amvib, (readU << 5) & 0x20)
        }

        if((key = std::strstr(line, "KR=")) != NULL)
        {
            if(std::sscanf(key, "KR=%u;", &readU) == 0)
                return false;

            SET_OP(op, amvib, (readU << 4) & 0x10)
        }

#undef SET_OP
        return true;
    }

    return false;
}

bool BankFormats::LoadWoplX(BanksDump &db, const char *fn, unsigned bank, const std::string bankTitle, const char *prefix)
{
    char readBuf[2000], readBufTr[2000]; /*, nameBuf[33] = "";*/
    FILE *fp = std::fopen(fn, "r");
    char *line, *line_tr;
    size_t line_len = 0, line_tr_len = 0;
    bool parseInfo = false;
    bool isDrumBank = false;
    int level = 0; // 0 - begin of file, 1 - bank entry, 2 - instrument
    int curInst = -1;
    unsigned readU, msb, lsb;
    bool msbHas = false, lsbHas = false;

    uint_fast16_t bank_settings = 0;
    size_t bankDb = 0;
    bool bank_settings_sent = false;
    BanksDump::MidiBank bnk;
    InstBuffer tmp[2];
    BanksDump::InstrumentEntry inst;
    BanksDump::Operator ops[5];
    std::string name;

    if(!fp)
    {
        std::fprintf(stderr, "WOPLX: CAN'T OPEN FILE %s\n", fn);
        std::fflush(stderr);
        return false;
    }

    line = std::fgets(readBuf, 2000, fp);
    if(!line)
    {
        fclose(fp);
        std::fprintf(stderr, "WOPLX: CAN'T READ LINE IN THE FILE %s\n", fn);
        std::fflush(stderr);
        return false;
    }

    line_len = std::strlen(line);

    if(std::strncmp(line, woplx_magic, line_len) != 0 && std::strncmp(line, woplx_magic_r, line_len) != 0)
    {
        std::fprintf(stderr, "WOPLX: INVALID MAGIC NUMBER IN FILE FORMAT %s\n", fn);
        std::fflush(stderr);
        fclose(fp);
        return false;
    }

    while((line = std::fgets(readBuf, 2000, fp)) != NULL)
    {
        line_len = std::strlen(line);
        trim(line, line_len, readBufTr);
        line_tr = readBufTr;
        line_tr_len = std::strlen(line_tr);

        if(parseInfo)
        {
            if(line_tr_len > 0 && std::strncmp(line_tr, "BANK_INFO_END", line_tr_len) == 0)
                parseInfo = false;

            // Do nothing,
            continue;
        }
        else if(line_tr_len == 0)
            continue; // Skip empty lines
        else if(str_starts_with(line_tr, line_tr_len, "#") || str_starts_with(line_tr, line_tr_len, "//"))
            continue; // Commentary lines

        switch(level)
        {
        case 0: // External level
            if(std::strncmp(line_tr, "BANK_INFO:", line_tr_len) == 0)
                parseInfo = true;
            else if(std::strncmp(line_tr, "MELODIC_BANK:", line_tr_len) == 0)
            {
                msb = 0;
                lsb = 0;
                msbHas = false;
                lsbHas = false;
                isDrumBank = false;
                curInst = -1;
                ++level;
                bnk = BanksDump::MidiBank();

                if(!bank_settings_sent)
                {
                    bankDb = db.initBank(bank, bankTitle, bank_settings);
                    bank_settings_sent = true;
                }
            }
            else if(std::strncmp(line_tr, "PERCUSSION_BANK:", line_tr_len) == 0)
            {
                msb = 0;
                lsb = 0;
                msbHas = false;
                lsbHas = false;
                isDrumBank = true;
                curInst = -1;
                ++level;
                bnk = BanksDump::MidiBank();

                if(!bank_settings_sent)
                {
                    bankDb = db.initBank(bank, bankTitle, bank_settings);
                    bank_settings_sent = true;
                }
            }
            else if(str_starts_with(line, line_len, "DEEP_VIBRATO="))
            {
                if(std::sscanf(line, "DEEP_VIBRATO=%u", &readU) == 0)
                {
                    fclose(fp);
                    return false;
                }

                bank_settings |= ((readU & 0x01) << (8 + 1)) & 0xFF00;
            }
            else if(str_starts_with(line, line_len, "DEEP_TREMOLO="))
            {
                if(std::sscanf(line, "DEEP_TREMOLO=%u", &readU) == 0)
                {
                    fclose(fp);
                    return false;
                }

                bank_settings |= ((readU & 0x01) << 8) & 0xFF00;
            }
            else if(str_starts_with(line, line_len, "IS_MT32="))
            {
                if(std::sscanf(line, "IS_MT32=%u", &readU) == 0)
                {
                    fclose(fp);
                    return false;
                }

                bank_settings |= ((readU & 0x01) << (8 + 2)) & 0xFF00;
            }
            else if(str_starts_with(line, line_len, "VOLUME_MODEL="))
            {
                if(std::sscanf(line, "VOLUME_MODEL=%u", &readU) == 0)
                {
                    fclose(fp);
                    return false;
                }

                bank_settings |= (readU & 0xFF);
            }
            else
            {
                // Invalid data!
                fclose(fp);
                return false;
            }
            break;

        case 1: // Bank level
            if(str_starts_with(line, line_len, "NAME="))
            {
                // Do Nothing!
                // size_t i, o;

                // for(i = 5, o = 0; o < 33 && i < line_len && line[i] != '\n' && line[i] != '\r'; ++i, ++o)
                //     nameBuf[o] = line[i];

                // nameBuf[o] = 0;
            }
            else if(str_starts_with(line, line_len, "MIDI_BANK_MSB="))
            {
                if(std::sscanf(line, "MIDI_BANK_MSB=%u", &msb) == 0)
                {
                    fclose(fp);
                    return false;
                }

                msbHas = true;
            }
            else if(str_starts_with(line, line_len, "MIDI_BANK_LSB="))
            {
                if(std::sscanf(line, "MIDI_BANK_LSB=%u", &lsb) == 0)
                {
                    fclose(fp);
                    return false;
                }

                lsbHas = true;
            }
            else if(str_starts_with(line, line_len, "INSTRUMENT="))
            {
                if(!msbHas || !lsbHas)
                {
                    // Missing required data!
                    fclose(fp);
                    return false;
                }

                // Actually start instruments data
                ++level;
                bnk.msb = msb;
                bnk.lsb = lsb;

                goto instrument;
            }
            else if((!isDrumBank && std::strncmp(line_tr, "MELODIC_BANK_END", line_tr_len) == 0) ||
                     (isDrumBank && std::strncmp(line_tr, "PERCUSSION_BANK_END", line_tr_len) == 0))
            {
                if(!msbHas || !lsbHas)
                {
                    // Missing required data!
                    fclose(fp);
                    return false;
                }

                // An empty bank created
                level = 0;
                db.addMidiBank(bankDb, isDrumBank, bnk);
                bnk = BanksDump::MidiBank();
                // nameBuf[0] = 0;
            }
            else
            {
                // Invalid data!
                fclose(fp);
                return false;
            }

            break;

        case 2: // Instrument level
instrument:
            if(str_starts_with(line, line_len, "INSTRUMENT="))
            {
                if(std::sscanf(line, "INSTRUMENT=%u:", &readU) == 0 || readU > 127)
                {
                    fclose(fp);
                    return false;
                }

                if(curInst >= 0) // Previous instrument!
                {
                    uint32_t gmno = isDrumBank ? curInst + 128 : curInst;

                    char name2[512];
                    std::memset(name2, 0, 512);
                    if(isDrumBank)
                        snprintf(name2, 512, "%sP%u", prefix, gmno & 127);
                    else
                        snprintf(name2, 512, "%sM%u", prefix, curInst);

                    db.toOps(tmp[0].d, ops, 0);
                    db.toOps(tmp[1].d, ops, 2);
                    db.addInstrument(bnk, curInst, inst, ops, fn);
                }

                std::memset(tmp, 0, sizeof(tmp));
                inst = BanksDump::InstrumentEntry();
                curInst = readU;
                if(isDrumBank)
                    inst.instFlags |= BanksDump::InstrumentEntry::WOPL_Ins_FixedNote;

                break;
            }

            if(curInst < 0)
            {
                // Instrument is NOT selected!
                fclose(fp);
                return false;
            }

            if((!isDrumBank && std::strncmp(line_tr, "MELODIC_BANK_END", line_tr_len) == 0) ||
                (isDrumBank && std::strncmp(line_tr, "PERCUSSION_BANK_END", line_tr_len) == 0))
            {
                level = 0;
                if(curInst >= 0) // Previous instrument!
                {
                    uint32_t gmno = isDrumBank ? curInst + 128 : curInst;

                    char name2[512];
                    std::memset(name2, 0, 512);
                    if(isDrumBank)
                        snprintf(name2, 512, "%sP%u", prefix, gmno & 127);
                    else
                        snprintf(name2, 512, "%sM%u", prefix, curInst);

                    db.toOps(tmp[0].d, ops, 0);
                    db.toOps(tmp[1].d, ops, 2);
                    db.addInstrument(bnk, curInst, inst, ops, fn);
                }

                curInst = -1;
                db.addMidiBank(bankDb, isDrumBank, bnk);
                // nameBuf[0] = 0;
            }
            else if(!woplx_read_inst_line(line, line_len, name, &inst, tmp))
            {
                // Invalid data!
                fclose(fp);
                return false;
            }

            break;
        }
    }

    fclose(fp);

    return true;
}

#endif // LOAD_WOPLX_H
