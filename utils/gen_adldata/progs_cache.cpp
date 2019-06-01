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
    for(const BankEntry &be : banks)
    {
        bool commaNeeded = true;
        std::fprintf(out, "    {\n");
        std::fprintf(out, "        0x%04lX, %zu, %zu, \"%s\",\n",
                                   be.bankSetup,
                                   be.melodic.size(),
                                   be.percussion.size(),
                                   be.bankTitle.c_str());
        // Melodic banks
        std::fprintf(out, "        {");
        commaNeeded = false;
        for(const size_t &me : be.melodic)
        {
            if(commaNeeded)
                std::fprintf(out, ", ");
            else
                commaNeeded = true;
            std::fprintf(out, "%zu", me);
        }
        std::fprintf(out, "},\n");

        // Percussive banks
        commaNeeded = false;
        std::fprintf(out, "        {");
        for(const size_t &me : be.percussion)
        {
            if(commaNeeded)
                std::fprintf(out, ", ");
            else
                commaNeeded = true;
            std::fprintf(out, "%zu", me);
        }
        std::fprintf(out, "}\n");

        std::fprintf(out, "    },\n");
    }

    std::fprintf(out, "}\n\n");


    std::fprintf(out, "const BanksDump::MidiBank g_embeddedBanksMidi[] =\n"
                      "{\n");
    for(const MidiBank &be : midiBanks)
    {
        bool commaNeeded = true;
        std::fprintf(out, "    {\n");
        std::fprintf(out, "        %u, %u,\n", be.msb, be.lsb);

        std::fprintf(out, "        {");
        commaNeeded = false;
        for(size_t i = 0; i < 128; i++)
        {
            if(commaNeeded)
                std::fprintf(out, ", ");
            else
                commaNeeded = true;
            std::fprintf(out, "%ld", be.instruments[i]);
        }
        std::fprintf(out, "},\n");

        std::fprintf(out, "    },\n");
    }
    std::fprintf(out, "}\n\n");


    std::fprintf(out, "const BanksDump::InstrumentEntry g_embeddedBanksInstruments[] =\n"
                      "{\n");
    for(const InstrumentEntry &be : instruments)
    {
        size_t opsCount = ((be.instFlags & InstrumentEntry::WOPL_Ins_4op) != 0 ||
                           (be.instFlags & InstrumentEntry::WOPL_Ins_Pseudo4op) != 0) ? 4 : 2;
        std::fprintf(out, "    {\n");
        std::fprintf(out, "        %d, %d, %d, %u, %lu, %d, 0x%04lX, 0x%lX, 0x%lX,\n",
                     be.noteOffset1,
                     be.noteOffset2,
                     be.midiVelocityOffset,
                     be.percussionKeyNumber,
                     be.instFlags,
                     be.secondVoiceDetune,
                     be.fbConn,
                     be.delay_on_ms,
                     be.delay_off_ms);

        if(opsCount == 4)
            std::fprintf(out, "        {%ld, %ld, %ld, %ld}\n",
                         be.ops[0], be.ops[1], be.ops[2], be.ops[3]);
        else
            std::fprintf(out, "        {%ld, %ld}\n",
                         be.ops[0], be.ops[1]);

        std::fprintf(out, "    },\n");
    }
    std::fprintf(out, "}\n\n");

    std::fprintf(out, "const BanksDump::Operator g_embeddedBanksOperators[] =\n"
                      "{\n");
    for(const Operator &be : operators)
    {
        std::fprintf(out, "    {0x%07lX, 0x%02lX},\n",
                     be.d_E862,
                     be.d_40);
    }
    std::fprintf(out, "}\n\n");

    std::fclose(out);
}
