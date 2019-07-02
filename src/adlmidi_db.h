#include <stdint.h>
#include <stddef.h>
#include <vector>

#ifndef _MSC_VER
#define ATTRIB_PACKED __attribute__((__packed__))
#else
#define ATTRIB_PACKED
#endif

typedef uint16_t bank_count_t;

extern const size_t g_embeddedBanksCount;

namespace BanksDump
{

struct BankEntry
{
    uint16_t bankSetup;
    bank_count_t banksMelodicCount;
    bank_count_t banksPercussionCount;
    const char *title;
    bank_count_t banksOffsetMelodic;
    bank_count_t banksOffsetPercussive;
} ATTRIB_PACKED;

struct MidiBank
{
    uint8_t msb;
    uint8_t lsb;
    int16_t insts[128];
} ATTRIB_PACKED;

struct InstrumentEntry
{
    int16_t noteOffset1;
    int16_t noteOffset2;
    int8_t  midiVelocityOffset;
    uint8_t percussionKeyNumber;
    uint8_t instFlags;
    int8_t  secondVoiceDetune;
    uint16_t fbConn;
    uint16_t delay_on_ms;
    uint16_t delay_off_ms;
    int16_t ops[4];
} ATTRIB_PACKED;

struct Operator
{
    uint32_t d_E862;
    uint8_t  d_40;
} ATTRIB_PACKED;

} /* namespace BanksDump */

extern const char* const g_embeddedBankNames[];
extern const BanksDump::BankEntry g_embeddedBanks[];
extern const size_t g_embeddedBanksMidiIndex[];
extern const BanksDump::MidiBank g_embeddedBanksMidi[];
extern const BanksDump::InstrumentEntry g_embeddedBanksInstruments[];
extern const BanksDump::Operator g_embeddedBanksOperators[];

