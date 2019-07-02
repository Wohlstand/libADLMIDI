#ifndef PROGS_H
#define PROGS_H

#include <map>
#include <set>
#include <utility>
#include <memory>
#include <cstring>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include <limits>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include <cassert>

struct insdata
{
    uint8_t         data[11];
    int8_t          finetune;
    bool            diff;
    explicit insdata()
    {
        std::memset(data, 0, 11);
        finetune = 0;
        diff = false;
    }
    insdata(const insdata &b)
    {
        std::memcpy(data, b.data, 11);
        finetune = b.finetune;
        diff = b.diff;
    }
    bool operator==(const insdata &b) const
    {
        return (std::memcmp(data, b.data, 11) == 0) && (finetune == b.finetune) && (diff == b.diff);
    }
    bool operator!=(const insdata &b) const
    {
        return !operator==(b);
    }
    bool operator<(const insdata &b) const
    {
        int c = std::memcmp(data, b.data, 11);
        if(c != 0) return c < 0;
        if(finetune != b.finetune) return finetune < b.finetune;
        if(diff != b.diff) return (diff) == (!b.diff);
        return 0;
    }
    bool operator>(const insdata &b) const
    {
        return !operator<(b) && operator!=(b);
    }
};

inline bool equal_approx(double const a, double const b)
{
    int_fast64_t ai = static_cast<int_fast64_t>(a * 1000000.0);
    int_fast64_t bi = static_cast<int_fast64_t>(b * 1000000.0);
    return ai == bi;
}

struct ins
{
    enum { Flag_Pseudo4op = 0x01, Flag_NoSound = 0x02, Flag_Real4op = 0x04 };

    enum { Flag_RM_BassDrum  = 0x08, Flag_RM_Snare = 0x10, Flag_RM_TomTom = 0x18,
           Flag_RM_Cymbal = 0x20, Flag_RM_HiHat = 0x28, Mask_RhythmMode = 0x38 };

    size_t insno1, insno2;
    insdata instCache1, instCache2;
    unsigned char notenum;
    bool pseudo4op;
    bool real4op;
    uint32_t rhythmModeDrum;
    double voice2_fine_tune;
    int8_t midi_velocity_offset;
    explicit ins() :
        insno1(0),
        insno2(0),
        notenum(0),
        pseudo4op(false),
        real4op(false),
        rhythmModeDrum(false),
        voice2_fine_tune(0.0),
        midi_velocity_offset(0)
    {}

    ins(const ins &o) :
        insno1(o.insno1),
        insno2(o.insno2),
        instCache1(o.instCache1),
        instCache2(o.instCache2),
        notenum(o.notenum),
        pseudo4op(o.pseudo4op),
        real4op(o.real4op),
        rhythmModeDrum(o.rhythmModeDrum),
        voice2_fine_tune(o.voice2_fine_tune),
        midi_velocity_offset(o.midi_velocity_offset)
    {}

    bool operator==(const ins &b) const
    {
        return notenum == b.notenum
               && insno1 == b.insno1
               && insno2 == b.insno2
               && instCache1 == b.instCache2
               && instCache2 == b.instCache2
               && pseudo4op == b.pseudo4op
               && real4op == b.real4op
               && rhythmModeDrum == b.rhythmModeDrum
               && equal_approx(voice2_fine_tune, b.voice2_fine_tune)
               && midi_velocity_offset == b.midi_velocity_offset;
    }
    bool operator< (const ins &b) const
    {
        if(insno1 != b.insno1) return insno1 < b.insno1;
        if(insno2 != b.insno2) return insno2 < b.insno2;
        if(instCache1 != b.instCache1) return instCache1 < b.instCache1;
        if(instCache2 != b.instCache2) return instCache2 < b.instCache2;
        if(notenum != b.notenum) return notenum < b.notenum;
        if(pseudo4op != b.pseudo4op) return pseudo4op < b.pseudo4op;
        if(real4op != b.real4op) return real4op < b.real4op;
        if(rhythmModeDrum != b.rhythmModeDrum) return rhythmModeDrum < b.rhythmModeDrum;
        if(!equal_approx(voice2_fine_tune, b.voice2_fine_tune)) return voice2_fine_tune < b.voice2_fine_tune;
        if(midi_velocity_offset != b.midi_velocity_offset) return midi_velocity_offset < b.midi_velocity_offset;
        return 0;
    }
    bool operator!=(const ins &b) const
    {
        return !operator==(b);
    }
};

enum VolumesModels
{
    VOLUME_Generic,
    VOLUME_CMF,
    VOLUME_DMX,
    VOLUME_APOGEE,
    VOLUME_9X
};

struct AdlBankSetup
{
    int     volumeModel;
    bool    deepTremolo;
    bool    deepVibrato;
    bool    scaleModulators;
};

typedef std::map<insdata, std::pair<size_t, std::set<std::string> > > InstrumentDataTab;
extern InstrumentDataTab insdatatab;

typedef std::map<ins, std::pair<size_t, std::set<std::string> > > InstrumentsData;
extern InstrumentsData instab;

typedef std::map<size_t, std::map<size_t, size_t> > InstProgsData;
extern InstProgsData progs;

typedef std::map<size_t, AdlBankSetup> BankSetupData;
extern BankSetupData banksetup;

extern std::vector<std::string> banknames;

//static std::map<unsigned, std::map<unsigned, unsigned> > Correlate;
//extern unsigned maxvalues[30];

void SetBank(size_t bank, unsigned patch, size_t insno);
void SetBankSetup(size_t bank, const AdlBankSetup &setup);

/* 2op voice instrument */
size_t InsertIns(const insdata &id, ins &in,
                 const std::string &name, const std::string &name2);

/* 4op voice instrument or double-voice 2-op instrument */
size_t InsertIns(const insdata &id, const insdata &id2, ins &in,
                 const std::string &name, const std::string &name2,
                 bool oneVoice = false);

size_t InsertNoSoundIns();
insdata MakeNoSoundIns();









struct BanksDump
{
    struct BankEntry
    {
        uint_fast32_t bankId = 0;
        std::string bankTitle = "Untitled";

        /* Global OPL flags */
        typedef enum WOPLFileFlags
        {
            /* Enable Deep-Tremolo flag */
            WOPL_FLAG_DEEP_TREMOLO = 0x01,
            /* Enable Deep-Vibrato flag */
            WOPL_FLAG_DEEP_VIBRATO = 0x02
        } WOPLFileFlags;

        /* Volume scaling model implemented in the libADLMIDI */
        typedef enum WOPL_VolumeModel
        {
            WOPL_VM_Generic = 0,
            WOPL_VM_Native,
            WOPL_VM_DMX,
            WOPL_VM_Apogee,
            WOPL_VM_Win9x
        } WOPL_VolumeModel;

        /**
         * @brief Suggested bank setup in dependence from a driver that does use of this
         */
        enum BankSetup
        {
            SETUP_Generic = 0x0300,
            SETUP_Win9X   = 0x0304,
            SETUP_DMX     = 0x0002,
            SETUP_Apogee  = 0x0003,
            SETUP_AIL     = 0x0300,
            SETUP_IBK     = 0x0301,
            SETUP_IMF     = 0x0200,
            SETUP_CMF     = 0x0201
        };

        uint_fast16_t       bankSetup = SETUP_Generic; // 0xAABB, AA - OPL flags, BB - Volume model
        std::vector<size_t> melodic;
        std::vector<size_t> percussion;

        explicit BankEntry() = default;

        BankEntry(const BankEntry &o)
        {
            bankId = o.bankId;
            bankTitle = o.bankTitle;
            bankSetup = o.bankSetup;
            melodic = o.melodic;
            percussion = o.percussion;
        }

        BankEntry(const BankEntry &&o)
        {
            bankId = std::move(o.bankId);
            bankTitle = std::move(o.bankTitle);
            bankSetup = std::move(o.bankSetup);
            melodic = std::move(o.melodic);
            percussion = std::move(o.percussion);
        }
    };

    struct MidiBank
    {
        uint_fast32_t midiBankId = 0;
        uint_fast8_t  msb = 0, lsb = 0;
        int_fast32_t  instruments[128];

        MidiBank()
        {
            for(size_t i = 0; i < 128; i++)
                instruments[i] = -1;
        }

        MidiBank(const MidiBank &o)
        {
            midiBankId = o.midiBankId;
            msb = o.msb;
            lsb = o.lsb;
            std::memcpy(instruments, o.instruments, sizeof(int_fast32_t) * 128);
        }

        bool operator==(const MidiBank &o)
        {
            if(msb != o.msb)
                return false;
            if(lsb != o.lsb)
                return false;
            if(std::memcmp(instruments, o.instruments, sizeof(int_fast32_t) * 128) != 0)
                return false;
            return true;
        }
        bool operator!=(const MidiBank &o)
        {
            return !operator==(o);
        }
    };

    struct InstrumentEntry
    {
        uint_fast32_t instId = 0;

        typedef enum WOPL_InstrumentFlags
        {
            /* Is two-operator single-voice instrument (no flags) */
            WOPL_Ins_2op        = 0x00,
            /* Is true four-operator instrument */
            WOPL_Ins_4op        = 0x01,
            /* Is pseudo four-operator (two 2-operator voices) instrument */
            WOPL_Ins_Pseudo4op  = 0x02,
            /* Is a blank instrument entry */
            WOPL_Ins_IsBlank    = 0x04,

            /* RythmMode flags mask */
            WOPL_RhythmModeMask  = 0x38,

            /* Mask of the flags range */
            WOPL_Ins_ALL_MASK   = 0x07
        } WOPL_InstrumentFlags;

        typedef enum WOPL_RhythmMode
        {
            /* RythmMode: BassDrum */
            WOPL_RM_BassDrum  = 0x08,
            /* RythmMode: Snare */
            WOPL_RM_Snare     = 0x10,
            /* RythmMode: TomTom */
            WOPL_RM_TomTom    = 0x18,
            /* RythmMode: Cymbell */
            WOPL_RM_Cymbal   = 0x20,
            /* RythmMode: HiHat */
            WOPL_RM_HiHat     = 0x28
        } WOPL_RhythmMode;

        int_fast8_t   noteOffset1 = 0;
        int_fast8_t   noteOffset2 = 0;
        int_fast8_t   midiVelocityOffset = 0;
        uint_fast8_t  percussionKeyNumber = 0;
        uint_fast32_t instFlags = 0;
        int_fast8_t   secondVoiceDetune = 0;
        /*
            2op: modulator1, carrier1, feedback1
            2vo: modulator1, carrier1, modulator2, carrier2, feedback(1+2)
            4op: modulator1, carrier1, modulator2, carrier2, feedback1
        */
        //! Contains FeedBack-Connection for both operators 0xBBAA - AA - first, BB - second
        uint_fast16_t fbConn = 0;
        int_fast64_t delay_on_ms = 0;
        int_fast64_t delay_off_ms = 0;
        int_fast32_t ops[5] = {-1, -1, -1, -1, -1};

        void setFbConn(uint_fast16_t fbConn1, uint_fast16_t fbConn2 = 0x00);

        bool operator==(const InstrumentEntry &o)
        {
            return (
                (noteOffset1 == o.noteOffset1) &&
                (noteOffset2 == o.noteOffset2) &&
                (midiVelocityOffset == o.midiVelocityOffset) &&
                (percussionKeyNumber == o.percussionKeyNumber) &&
                (instFlags == o.instFlags) &&
                (secondVoiceDetune == o.secondVoiceDetune) &&
                (fbConn == o.fbConn) &&
                (delay_on_ms == o.delay_on_ms) &&
                (delay_off_ms == o.delay_off_ms) &&
                (std::memcmp(ops, o.ops, sizeof(int_fast32_t) * 5) == 0)
            );
        }
        bool operator!=(const InstrumentEntry &o)
        {
            return !operator==(o);
        }
    };

    struct Operator
    {
        uint_fast32_t opId = 0;
        uint_fast32_t d_E862 = 0;
        uint_fast32_t d_40 = 0;
        explicit Operator() {}
        Operator(const Operator &o)
        {
            opId = o.opId;
            d_E862 = o.d_E862;
            d_40 = o.d_40;
        }
        bool operator==(const Operator &o)
        {
            return ((d_E862 == o.d_E862) && (d_40 == o.d_40));
        }
        bool operator!=(const Operator &o)
        {
            return !operator==(o);
        }
    };

    std::vector<BankEntry>       banks;
    std::vector<MidiBank>        midiBanks;
    std::vector<InstrumentEntry> instruments;
    std::vector<Operator>        operators;

    static void toOps(const insdata &inData, Operator *outData, size_t begin = 0);
    //! WIP
    static bool isSilent(const Operator *ops, uint_fast16_t fbConn, size_t countOps = 2, bool pseudo4op = false);

    size_t initBank(size_t bankId, const std::string &title, uint_fast16_t bankSetup);
    void addMidiBank(size_t bankId, bool percussion, MidiBank b);
    void addInstrument(MidiBank &bank, size_t patchId, InstrumentEntry e, Operator *ops);
    void exportBanks(const std::string &outPath, const std::string &headerName = "adlmidi_db.h");
};


namespace BankFormats
{

bool LoadMiles(BanksDump &db, const char *fn, unsigned bank, const std::string &bankTitle, const char *prefix);
bool LoadBisqwit(BanksDump &db, const char *fn, unsigned bank, const std::string &bankTitle, const char *prefix);
bool LoadBNK(BanksDump &db, const char *fn, unsigned bank, const std::string &bankTitle, const char *prefix, bool is_fat, bool percussive);
bool LoadBNK2(BanksDump &db, const char *fn, unsigned bank, const std::string &bankTitle, const char *prefix,
                     const std::string &melo_filter,
                     const std::string &perc_filter);
bool LoadEA(BanksDump &db, const char *fn, unsigned bank, const std::string &bankTitle, const char *prefix);
bool LoadIBK(BanksDump &db, const char *fn, unsigned bank, const std::string &bankTitle, const char *prefix, bool percussive, bool noRhythmMode = false);
bool LoadJunglevision(BanksDump &db, const char *fn, unsigned bank, const std::string &bankTitle, const char *prefix);
bool LoadDoom(BanksDump &db, const char *fn, unsigned bank, const std::string &bankTitle, const char *prefix);
bool LoadTMB(BanksDump &db, const char *fn, unsigned bank, const std::string &bankTitle, const char *prefix);
bool LoadWopl(BanksDump &db, const char *fn, unsigned bank, const std::string bankTitle, const char *prefix);

}

#endif // PROGS_H
