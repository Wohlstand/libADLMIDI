#include "progs_cache.h"

#include "file_formats/load_ail.h"
#include "file_formats/load_bisqwit.h"
#include "file_formats/load_bnk2.h"
#include "file_formats/load_bnk.h"
#include "file_formats/load_ibk.h"
#include "file_formats/load_jv.h"
#include "file_formats/load_op2.h"
#include "file_formats/load_tmb.h"
#include "file_formats/load_wopl.h"
#include "file_formats/load_ea.h"

InstrumentDataTab insdatatab;

InstrumentsData instab;
InstProgsData   progs;
BankSetupData   banksetup;

std::vector<std::string> banknames;

//unsigned maxvalues[30] = { 0 };

void SetBank(size_t bank, unsigned patch, size_t insno)
{
    progs[bank][patch] = insno + 1;
}

void SetBankSetup(size_t bank, const AdlBankSetup &setup)
{
    banksetup[bank] = setup;
}

size_t InsertIns(const insdata &id, ins &in, const std::string &name, const std::string &name2)
{
    return InsertIns(id, id, in, name, name2, true);
}

size_t InsertIns(
    const insdata &id,
    const insdata &id2,
    ins &in,
    const std::string &name,
    const std::string &name2,
    bool oneVoice)
{
    {
        InstrumentDataTab::iterator i = insdatatab.lower_bound(id);

        size_t insno = ~size_t(0);
        if(i == insdatatab.end() || i->first != id)
        {
            std::pair<insdata, std::pair<size_t, std::set<std::string> > > res;
            res.first = id;
            res.second.first = insdatatab.size();
            if(!name.empty()) res.second.second.insert(name);
            if(!name2.empty()) res.second.second.insert(name2);
            insdatatab.insert(i, res);
            insno = res.second.first;
        }
        else
        {
            if(!name.empty()) i->second.second.insert(name);
            if(!name2.empty()) i->second.second.insert(name2);
            insno = i->second.first;
        }

        in.insno1 = insno;
    }

    if(oneVoice || (id == id2))
        in.insno2 = in.insno1;
    else
    {
        InstrumentDataTab::iterator i = insdatatab.lower_bound(id2);

        size_t insno2 = ~size_t(0);
        if(i == insdatatab.end() || i->first != id2)
        {
            std::pair<insdata, std::pair<size_t, std::set<std::string> > > res;
            res.first = id2;
            res.second.first = insdatatab.size();
            if(!name.empty()) res.second.second.insert(name);
            if(!name2.empty()) res.second.second.insert(name2);
            insdatatab.insert(i, res);
            insno2 = res.second.first;
        }
        else
        {
            if(!name.empty()) i->second.second.insert(name);
            if(!name2.empty()) i->second.second.insert(name2);
            insno2 = i->second.first;
        }
        in.insno2 = insno2;
    }

    {
        InstrumentsData::iterator i = instab.lower_bound(in);

        size_t resno = ~size_t(0);
        if(i == instab.end() || i->first != in)
        {
            std::pair<ins, std::pair<size_t, std::set<std::string> > > res;
            res.first = in;
            res.second.first = instab.size();
            if(!name.empty()) res.second.second.insert(name);
            if(!name2.empty()) res.second.second.insert(name2);
            instab.insert(i, res);
            resno = res.second.first;
        }
        else
        {
            if(!name.empty()) i->second.second.insert(name);
            if(!name2.empty()) i->second.second.insert(name2);
            resno = i->second.first;
        }
        return resno;
    }
}

// Create silent 'nosound' instrument
size_t InsertNoSoundIns()
{
    // { 0x0F70700,0x0F70710, 0xFF,0xFF, 0x0,+0 },
    insdata tmp1 = MakeNoSoundIns();
    struct ins tmp2 = { 0, 0, 0, false, false, 0u, 0.0, 0};
    return InsertIns(tmp1, tmp1, tmp2, "nosound", "");
}

insdata MakeNoSoundIns()
{
    return { {0x00, 0x10, 0x07, 0x07, 0xF7, 0xF7, 0x00, 0x00, 0xFF, 0xFF, 0x00}, 0, false};
}


void BanksDump::toOps(const insdata &inData, BanksDump::Operator *outData, size_t begin)
{
    outData[begin + 0].d_E862 =
            uint_fast32_t(inData.data[6] << 24)
          + uint_fast32_t(inData.data[4] << 16)
          + uint_fast32_t(inData.data[2] << 8)
          + uint_fast32_t(inData.data[0] << 0);
    outData[begin + 1].d_E862 =
            uint_fast32_t(inData.data[7] << 24)
          + uint_fast32_t(inData.data[5] << 16)
          + uint_fast32_t(inData.data[3] << 8)
          + uint_fast32_t(inData.data[1] << 0);
    outData[begin + 0].d_40 = inData.data[8];
    outData[begin + 1].d_40 = inData.data[9];
}

size_t BanksDump::initBank(size_t bankId, const std::string &title, uint_fast16_t bankSetup)
{
    for(size_t bID = 0; bID < banks.size(); bID++)
    {
        BankEntry &be = banks[bID];
        if(bankId == be.bankId)
        {
            be.bankTitle = title;
            be.bankSetup = bankSetup;
            return bID;
        }
    }

    size_t bankIndex = banks.size();
    banks.emplace_back();
    BankEntry &b = banks.back();

    b.bankId = bankId;
    b.bankTitle = title;
    b.bankSetup = bankSetup;
    return bankIndex;
}

void BanksDump::addMidiBank(size_t bankId, bool percussion, BanksDump::MidiBank b)
{
    assert(bankId < banks.size());
    BankEntry &be = banks[bankId];

    auto it = std::find(midiBanks.begin(), midiBanks.end(), b);
    if(it == midiBanks.end())
    {
        b.midiBankId = midiBanks.size();
        midiBanks.push_back(b);
    }
    else
    {
        b.midiBankId = it->midiBankId;
    }

    if(percussion)
        be.percussion.push_back(b.midiBankId);
    else
        be.melodic.push_back(b.midiBankId);
}

void BanksDump::addInstrument(BanksDump::MidiBank &bank, size_t patchId, BanksDump::InstrumentEntry e, BanksDump::Operator *ops)
{
    assert(patchId < 128);
    size_t opsCount = ((e.instFlags & InstrumentEntry::WOPL_Ins_4op) != 0 ||
            (e.instFlags & InstrumentEntry::WOPL_Ins_Pseudo4op) != 0) ?
                4 : 2;

    if((e.instFlags & InstrumentEntry::WOPL_Ins_IsBlank) != 0)
    {
        bank.instruments[patchId] = -1;
        return;
    }

    for(size_t op = 0; op < opsCount; op++)
    {
        Operator o = ops[op];
        auto it = std::find(operators.begin(), operators.end(), o);
        if(it == operators.end())
        {
            o.opId = operators.size();
            e.ops[op] = static_cast<int_fast32_t>(o.opId);
            operators.push_back(o);
        }
        else
        {
            e.ops[op] = static_cast<int_fast32_t>(it->opId);
        }
    }

    auto it = std::find(instruments.begin(), instruments.end(), e);
    if(it == instruments.end())
    {
        e.instId = instruments.size();
        instruments.push_back(e);
    }
    else
    {
        e.instId = it->instId;
    }
    bank.instruments[patchId] = static_cast<int_fast32_t>(e.instId);
}

void BanksDump::exportBanks(const std::string &outPath, const std::string &headerName)
{
    FILE *out = std::fopen(outPath.c_str(), "w");

    std::fprintf(out, "/**********************************************************\n"
                      "    This file is generated by `gen_adldata` automatically\n"
                      "                  Don't edit it directly!\n"
                      "        To modify content of this file, modify banks\n"
                      "          and re-run the `gen_adldata` build step.\n"
                      "***********************************************************/\n\n"
                      "#include \"%s\"\n\n\n", headerName.c_str());

    std::fprintf(out, "const size_t g_embeddedBanksCount = %zu;\n\n", banks.size());
    std::fprintf(out, "const BanksDump::BankEntry g_embeddedBanks[] =\n"
                      "{\n");

    std::vector<size_t> bankNumberLists;

    for(const BankEntry &be : banks)
    {
        std::fprintf(out, "    {0x%04lX, %zu, %zu, \"%s\", ",
                                   be.bankSetup,
                                   be.melodic.size(),
                                   be.percussion.size(),
                                   be.bankTitle.c_str());

        fprintf(out, "%zu, ", bankNumberLists.size()); // Use offset to point the common array of bank IDs
        for(const size_t &me : be.melodic)
            bankNumberLists.push_back(me);

        fprintf(out, "%zu", bankNumberLists.size());
        for(const size_t &me : be.percussion)
            bankNumberLists.push_back(me);

        std::fprintf(out, "},\n");
    }

    std::fprintf(out, "};\n\n");


    std::fprintf(out, "const size_t g_embeddedBanksMidiIndex[] =\n"
                      "{ ");
    {
        bool commaNeeded = false;
        for(const size_t &me : bankNumberLists)
        {
            if(commaNeeded)
                std::fprintf(out, ", ");
            else
                commaNeeded = true;
            std::fprintf(out, "%zu", me);
        }
    }
    std::fprintf(out, " };\n\n");

    std::fprintf(out, "const BanksDump::MidiBank g_embeddedBanksMidi[] =\n"
                      "{\n");
    for(const MidiBank &be : midiBanks)
    {
        bool commaNeeded = true;
        std::fprintf(out, "    { %u, %u, ", be.msb, be.lsb);

        std::fprintf(out, "{");
        commaNeeded = false;
        for(size_t i = 0; i < 128; i++)
        {
            if(commaNeeded)
                std::fprintf(out, ", ");
            else
                commaNeeded = true;
            std::fprintf(out, "%ld", be.instruments[i]);
        }
        std::fprintf(out, "} ");

        std::fprintf(out, "},\n");
    }
    std::fprintf(out, "};\n\n");


    std::fprintf(out, "const BanksDump::InstrumentEntry g_embeddedBanksInstruments[] =\n"
                      "{\n");
    for(const InstrumentEntry &be : instruments)
    {
        size_t opsCount = ((be.instFlags & InstrumentEntry::WOPL_Ins_4op) != 0 ||
                           (be.instFlags & InstrumentEntry::WOPL_Ins_Pseudo4op) != 0) ? 4 : 2;
        std::fprintf(out, "    { %d, %d, %d, %u, %s%lX, %d, %s%lX, %s%lX, %s%lX, ",
                     be.noteOffset1,
                     be.noteOffset2,
                     be.midiVelocityOffset,
                     be.percussionKeyNumber,
                     (be.instFlags == 0 ? "" : "0x"), be.instFlags, // for compactness, don't print "0x" when is zero
                     be.secondVoiceDetune,
                     (be.fbConn == 0 ? "" : "0x"), be.fbConn,
                     (be.delay_on_ms == 0 ? "" : "0x"), be.delay_on_ms,
                     (be.delay_off_ms == 0 ? "" : "0x"), be.delay_off_ms);

        if(opsCount == 4)
            std::fprintf(out, "{%ld, %ld, %ld, %ld} ",
                         be.ops[0], be.ops[1], be.ops[2], be.ops[3]);
        else
            std::fprintf(out, "{%ld, %ld} ",
                         be.ops[0], be.ops[1]);

        std::fprintf(out, "},\n");
    }
    std::fprintf(out, "};\n\n");

    std::fprintf(out, "const BanksDump::Operator g_embeddedBanksOperators[] =\n"
                      "{\n");
    size_t operatorEntryCounter = 0;
    for(const Operator &be : operators)
    {
        if(operatorEntryCounter == 0)
            std::fprintf(out, "    ");
        std::fprintf(out, "{0x%07lX, %s%02lX},",
                     be.d_E862,
                     (be.d_40 == 0 ? "" : "0x"), be.d_40);
        operatorEntryCounter++;
        if(operatorEntryCounter >= 25)
        {
            std::fprintf(out, "\n");
            operatorEntryCounter = 0;
        }
    }
    std::fprintf(out, "\n};\n\n");

    std::fclose(out);
}

bool BanksDump::isSilent(const BanksDump::Operator *ops, uint_fast16_t fbConn, size_t countOps, bool pseudo4op)
{
    // TODO: Implement this completely!!!
    const uint_fast8_t conn1 = (fbConn) & 0x01;
    const uint_fast8_t conn2 = (fbConn >> 8) & 0x01;
    const uint_fast8_t egEn[4] =
    {
        static_cast<uint_fast8_t>(((ops[0].d_E862 & 0xFF) >> 5) & 0x01),
        static_cast<uint_fast8_t>(((ops[1].d_E862 & 0xFF) >> 5) & 0x01),
        static_cast<uint_fast8_t>(((ops[2].d_E862 & 0xFF) >> 5) & 0x01),
        static_cast<uint_fast8_t>(((ops[3].d_E862 & 0xFF) >> 5) & 0x01)
    };
    const uint_fast8_t attack[4] =
    {
        static_cast<uint_fast8_t>((((ops[0].d_E862 >> 8) & 0xFF) >> 4) & 0x0F),
        static_cast<uint_fast8_t>((((ops[1].d_E862 >> 8) & 0xFF) >> 4) & 0x0F),
        static_cast<uint_fast8_t>((((ops[2].d_E862 >> 8) & 0xFF) >> 4) & 0x0F),
        static_cast<uint_fast8_t>((((ops[3].d_E862 >> 8) & 0xFF) >> 4) & 0x0F)
    };
    const uint_fast8_t decay[4] =
    {
        static_cast<uint_fast8_t>((ops[0].d_E862 >> 8) & 0x0F),
        static_cast<uint_fast8_t>((ops[1].d_E862 >> 8) & 0x0F),
        static_cast<uint_fast8_t>((ops[2].d_E862 >> 8) & 0x0F),
        static_cast<uint_fast8_t>((ops[3].d_E862 >> 8) & 0x0F)
    };
    const uint_fast8_t sustain[4] =
    {
        static_cast<uint_fast8_t>((((ops[0].d_E862 >> 16) & 0xFF) >> 4) & 0x0F),
        static_cast<uint_fast8_t>((((ops[1].d_E862 >> 16) & 0xFF) >> 4) & 0x0F),
        static_cast<uint_fast8_t>((((ops[2].d_E862 >> 16) & 0xFF) >> 4) & 0x0F),
        static_cast<uint_fast8_t>((((ops[3].d_E862 >> 16) & 0xFF) >> 4) & 0x0F)
    };
    const uint_fast8_t level[4] =
    {
        static_cast<uint_fast8_t>(ops[0].d_40),
        static_cast<uint_fast8_t>(ops[1].d_40),
        static_cast<uint_fast8_t>(ops[2].d_40),
        static_cast<uint_fast8_t>(ops[3].d_40)
    };

    // level=0x3f - silence
    // attack=0x00 - silence
    // attack=0x0F & sustain=0 & decay=0x0F & egOff - silence

    if(countOps == 2)
    {
        if(conn1 == 0)
        {
            if(level[1] == 0x3F)
                return true;
            if(attack[1] == 0x00)
                return true;
            if(attack[1] == 0x0F && sustain[1] == 0x00 && decay[1] == 0x0F && !egEn[1])
                return true;
        }
        if(conn1 == 1)
        {
            if(level[0] == 0x3F && level[1] == 0x3F)
                return true;
            if(attack[0] == 0x00 && attack[1] == 0x00)
                return true;
            if(attack[0] == 0x0F && sustain[0] == 0x00 && decay[0] == 0x0F && !egEn[0] &&
               attack[1] == 0x0F && sustain[1] == 0x00 && decay[1] == 0x0F && !egEn[1])
                return true;
        }
    }
    else if(countOps == 4 && pseudo4op)
    {
        bool silent1 = false;
        bool silent2 = false;
        if(conn1 == 0 && (level[1] == 0x3F))
            silent1 = true;
        if(conn1 == 1 && (level[0] == 0x3F) && (level[1] == 0x3F))
            silent1 = true;
        if(conn2 == 0 && (level[3] == 0x3F))
            silent2 = true;
        if(conn2 == 1 && (level[2] == 0x3F) && (level[3] == 0x3F))
            silent2 = true;
        if(silent1 && silent2)
            return true;
    }
    else if(countOps == 4 && !pseudo4op)
    {
        if(conn1 == 0 && conn1 == 0) // FM-FM [0, 0, 0, 1]
        {
            if(level[3] == 0x3F )
                return true;
        }

        if(conn1 == 1 && conn1 == 0) // AM-FM [1, 0, 0, 1]
        {
            if(level[0] == 0x3F && level[3] == 0x3F)
                return true;
        }
        if(conn1 == 0 && conn1 == 1) // FM-AM [0, 1, 0, 1]
        {
            if(level[1] == 0x3F && level[3] == 0x3F)
                return true;
        }
        if(conn1 == 1 && conn1 == 1) // FM-AM [1, 0, 1, 1]
        {
            if(level[0] == 0x3F && level[2] == 0x3F && level[3] == 0x3F)
                return true;
        }
    }

    return false;
}

void BanksDump::InstrumentEntry::setFbConn(uint_fast16_t fbConn1, uint_fast16_t fbConn2)
{
    fbConn = (static_cast<uint_fast16_t>(fbConn1 & 0x0F)) |
             (static_cast<uint_fast16_t>(fbConn2 & 0x0F) << 8);
}
