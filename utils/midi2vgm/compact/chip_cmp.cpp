#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "vgm_stdtype.h"


//#define REMOVE_NES_DPCM_0
// TODO: K053260, K054539 (for mega size reduction)
namespace VgmCMP
{

typedef struct sn76496_data
{
    UINT8 FreqMSB[0x04];
    UINT8 FreqLSB[0x04];
    UINT8 VolData[0x04];
    UINT8 LastReg;
    bool LastRet;
} SN76496_DATA;
typedef struct ym2413_data
{
    UINT8 RegData[0x40];
    UINT8 RegFirst[0x40];
} YM2413_DATA;
typedef struct ym2612_data
{
    UINT8 RegData[0x200];
    UINT8 RegFirst[0x200];
    UINT8 KeyOn[0x08];
    UINT8 KeyFirst[0x08];
} YM2612_DATA;
typedef struct ym2151_data
{
    UINT8 RegData[0x100];
    UINT8 RegFirst[0x100];
    UINT8 MCMask[0x08];
    UINT8 MCFirst[0x08];
    UINT8 MDMask[0x02]; // 0x00 - AMD, 0x01 - PMD
    UINT8 MDFirst[0x02];
} YM2151_DATA;
typedef struct segapcm_data
{
    UINT8 *ROMData;
    UINT8 *ROMUsage;
    UINT8 RAMData[0x800];
    UINT8 RAMFirst[0x800];
    UINT8 ChnPrg[0x10]; // channel is progressing
} SEGAPCM_DATA;
typedef struct rf5c68_channel
{
    UINT8 ChnReg[0x07];
    UINT8 RegFirst[0x07];
} RF5C68_CHANNEL;
#define RF_CBANK    0x00
#define RF_WBANK    0x01
#define RF_ENABLE   0x02
#define RF_CHN_MASK 0x03
#define RF_CHN_LOOP 0x04
typedef struct rf5c68_data
{
    RF5C68_CHANNEL chan[0x08];
    UINT8 RegData[0x05];
    UINT8 RegFirst[0x05];
    //UINT8 ChnSelLoopSave;
} RF5C68_DATA;
typedef struct ym2203_data
{
    UINT8 RegData[0x100];
    UINT8 RegFirst[0x100];
    UINT8 KeyOn[0x04];
    UINT8 KeyFirst[0x04];
    UINT8 PreSclCmd;
} YM2203_DATA;
typedef struct ym2608_data
{
    UINT8 RegData[0x200];
    UINT8 RegFirst[0x200];
    UINT8 KeyOn[0x08];
    UINT8 KeyFirst[0x08];
    UINT8 PreSclCmd;
} YM2608_DATA;
typedef struct ym2610_data
{
    UINT8 RegData[0x200];
    UINT8 RegFirst[0x200];
    UINT8 KeyOn[0x08];
    UINT8 KeyFirst[0x08];
    UINT8 PreSclCmd;
} YM2610_DATA;
typedef struct ym3812_data
{
    UINT8 RegData[0x100];
    UINT8 RegFirst[0x100];
} YM3812_DATA;
typedef struct ym3526_data
{
    UINT8 RegData[0x100];
    UINT8 RegFirst[0x100];
} YM3526_DATA;
typedef struct y8950_data
{
    UINT8 RegData[0x100];
    UINT8 RegFirst[0x100];
} Y8950_DATA;
typedef struct ymf262_data
{
    UINT8 RegData[0x200];
    UINT8 RegFirst[0x200];
} YMF262_DATA;
typedef struct ymf278b_data
{
    UINT8 RegData[0x300];
    UINT8 RegFirst[0x300];
} YMF278B_DATA;
typedef struct ymf271_slot
{
    UINT8 RegData[0x10];
    UINT8 RegFirst[0x10];

    UINT8 PCMRegFirst[0x10];
    UINT8 PCMRegData[0x10];
    /*UINT8 startaddr[3];
    UINT8 loopaddr[3];
    UINT8 endaddr[3];
    UINT8 slotnote;
    UINT8 sltnfirst;*/
} YMF271_SLOT;
typedef struct ymf271_group
{
    UINT8 Data;
    UINT8 First;
    UINT8 sync;
} YMF271_GROUP;
typedef struct ymf271_chip
{
    YMF271_SLOT slots[48];
    YMF271_GROUP groups[12];

    UINT8 ext_address[3];
    UINT8 ext_read;
} YMF271_DATA;
/*typedef struct ymf278b_data
{
    UINT8 RegData[0x300];
    UINT8 RegFirst[0x300];
} YMF278B_DATA;*/
typedef struct ymz280b_data
{
    UINT8 RegData[0x100];
    UINT8 RegFirst[0x100];
    UINT8 KeyOn[0x08];
} YMZ280B_DATA;
typedef struct ay8910_data
{
    UINT8 RegData[0x10];
    UINT8 RegFirst[0x10];
} AY8910_DATA;
typedef struct gameboy_dmg_data
{
    UINT8 RegData[0x30];
    UINT8 RegFirst[0x30];
} GBDMG_DATA;
typedef struct nes_apu_data
{
    UINT8 RegData[0x80];
    UINT8 RegFirst[0x80];
} NESAPU_DATA;
enum
{
    C140_TYPE_SYSTEM2,
    C140_TYPE_SYSTEM21,
    C140_TYPE_ASIC219
};
typedef struct c140_data
{
    UINT8 banking_type;
    UINT8 RegData[0x200];
    UINT8 RegFirst[0x200];
} C140_DATA;
typedef struct qsound_data
{
    UINT16 RegData[0x100];
    UINT8 RegFirst[0x100];
    UINT8 KeyOn[0x10];
} QSOUND_DATA;
typedef struct pokey_data
{
    UINT8 RegData[0x10];
    UINT8 RegFirst[0x10];
} POKEY_DATA;
typedef struct huc6280_channel
{
    UINT8 ChnReg[0x07];
    UINT8 RegFirst[0x07];
} C6280_CHANNEL;
#define C6280_CHN_SEL   0x00
#define C6280_BALANCE   0x01
#define C6280_LFO_FRQ   0x02
#define C6280_LFO_CTRL  0x03
#define C6280_CHN_LOOP  0x04
typedef struct huc6280_data
{
    C6280_CHANNEL chan[0x08];   // the chip has only 6 channels, but the register supports 8
    UINT8 RegData[0x05];
    UINT8 RegFirst[0x05];
    //UINT8 ChnSelLoopSave;
} C6280_DATA;
#define K054539_RESET_FLAGS     0x00
#define K054539_REVERSE_STEREO  0x01
#define K054539_DISABLE_REVERB  0x02
#define K054539_UPDATE_AT_KEYON 0x04
typedef struct k054539_data
{
    UINT8 RegData[0x230];
    UINT8 RegFirst[0x230];

    //  UINT8 k054539_flags;
} K054539_DATA;
typedef struct k051649_data
{
    UINT8 WaveData[0x20 * 5];
    UINT8 WaveFirst[0x20 * 5];
    UINT8 FreqData[0x02 * 5];
    UINT8 FreqFirst[0x02 * 5];
    UINT8 VolData[0x01 * 5];
    UINT8 VolFirst[0x01 * 5];
    UINT8 KeyOn;
    UINT8 KeyFirst;
} K051649_DATA;
typedef struct okim6295_data
{
    UINT8 RegData[0x14];
    UINT8 RegFirst[0x14];
} OKIM6295_DATA;
typedef struct okim6295_data OKIM6258_DATA;
typedef struct upd7759_data
{
    UINT8 RegData[0x04];
    UINT8 RegFirst[0x04];
} UPD7759_DATA;
#define C352_FLG_LINKLOOP   0x0022
typedef struct c352_data
{
    UINT16 RegData[0x208];
    UINT8 RegFirst[0x208];
} C352_DATA;
typedef struct x1_010_data
{
    UINT8 RegData[0x2000];
    UINT8 RegFirst[0x2000];
} X1_010_DATA;
typedef struct es5503_data
{
    UINT8 RegData[0xE2];
    UINT8 RegFirst[0xE2];
} ES5503_DATA;
typedef struct vsu_data
{
    UINT8 RegData[0x160];
    UINT8 RegFirst[0x160];
} VSU_DATA;

typedef struct all_chips
{
    UINT8 GGSt;
    SN76496_DATA SN76496;
    YM2413_DATA YM2413;
    YM2612_DATA YM2612;
    YM2151_DATA YM2151;
    SEGAPCM_DATA SegaPCM;
    RF5C68_DATA RF5C68;
    YM2203_DATA YM2203;
    YM2608_DATA YM2608;
    YM2610_DATA YM2610;
    YM3812_DATA YM3812;
    YM3526_DATA YM3526;
    Y8950_DATA Y8950;
    YMF262_DATA YMF262;
    YMF278B_DATA YMF278B;
    YMF271_DATA YMF271;
    //YMF278B_DATA YMF278B;
    YMZ280B_DATA YMZ280B;
    RF5C68_DATA RF5C164;
    AY8910_DATA AY8910;
    GBDMG_DATA GBDMG;
    NESAPU_DATA NES;
    UPD7759_DATA UPD7759;
    OKIM6258_DATA OKIM6258;
    OKIM6295_DATA OKIM6295;
    K051649_DATA K051649;
    K054539_DATA K054539;
    C6280_DATA C6280;
    C140_DATA C140;
    POKEY_DATA Pokey;
    QSOUND_DATA QSound;
    //SCSP_DATA SCSP;
    ES5503_DATA ES5503;
    C352_DATA C352;
    X1_010_DATA X1_010;
    VSU_DATA VSU;
} ALL_CHIPS;


void InitAllChips(void);
void ResetAllChips(void);
void FreeAllChips(void);
void SetChipSet(UINT8 ChipID);
bool dac_stream_control_freq(UINT8 strmID, UINT32 freq);
bool GGStereo(UINT8 Data);
bool sn76496_write(UINT8 Command/*, UINT8 NextCmd*/);
bool ym2413_write(UINT8 Register, UINT8 Data);
bool ym2612_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool ym2151_write(UINT8 Register, UINT8 Data);
bool segapcm_mem_write(UINT16 Offset, UINT8 Data);
static bool rf_pcm_reg_write(RF5C68_DATA *chip, UINT8 Register, UINT8 Data);
bool rf5c68_reg_write(UINT8 Register, UINT8 Data);
bool ym2203_write(UINT8 Register, UINT8 Data);
bool ym2608_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool ym2610_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool ym3812_write(UINT8 Register, UINT8 Data);
bool ym3526_write(UINT8 Register, UINT8 Data);
bool y8950_write(UINT8 Register, UINT8 Data);
bool ymf262_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool ymf278b_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool ymz280b_write(UINT8 Register, UINT8 Data);
bool rf5c164_reg_write(UINT8 Register, UINT8 Data);
static bool ay8910_part_write(UINT8 *RegData, UINT8 *RegFirst, UINT8 Register, UINT8 Data);
bool ay8910_write_reg(UINT8 Register, UINT8 Data);
static bool ymf271_write_fm_reg(YMF271_DATA *chip, UINT8 SlotNum, UINT8 Register, UINT8 Data);
static bool ymf271_write_fm(YMF271_DATA *chip, UINT8 Port, UINT8 Register, UINT8 Data);
bool ymf271_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool gameboy_write_reg(UINT8 Register, UINT8 Data);
static bool ymdeltat_write(UINT8 Register, UINT8 Data, UINT8 *RegData, UINT8 *RegFirst);
bool nes_psg_write(UINT8 Register, UINT8 Data);
bool c140_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool qsound_write(UINT8 Offset, UINT16 Value);
bool pokey_write(UINT8 Register, UINT8 Data);
static bool fmadpcm_write(UINT8 Register, UINT8 Data, UINT8 *RegData, UINT8 *RegFirst);
bool k054539_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool k051649_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool scsp_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool okim6295_write(UINT8 Port, UINT8 Data);
bool upd7759_write(UINT8 Port, UINT8 Data);
bool okim6258_write(UINT8 Port, UINT8 Data);
bool c352_write(UINT16 Offset, UINT16 Value);
bool x1_010_write(UINT16 Offset, UINT8 Value);
bool es5503_write(UINT8 Register, UINT8 Data);
bool vsu_write(UINT16 Register, UINT8 Data);

// Function Prototypes from vgm_cmp.c
bool GetNextChipCommand(void);


UINT8 ChipCount = 0x02;
ALL_CHIPS *ChipData = NULL;
ALL_CHIPS *ChDat;
UINT32 StreamFreqs[0x100];

extern bool JustTimerCmds;
extern bool DoOKI6258;

extern UINT16 NxtCmdReg;
extern UINT8 NxtCmdVal;
bool VGM_Loops;

void InitAllChips(void)
{
    UINT8 CurChip;
    ALL_CHIPS *TempChp;

    if(ChipData == NULL)
        ChipData = (ALL_CHIPS *)malloc(ChipCount * sizeof(ALL_CHIPS));
    for(CurChip = 0x00; CurChip < ChipCount; CurChip ++)
    {
        TempChp = &ChipData[CurChip];
        memset(TempChp, 0xFF, sizeof(ALL_CHIPS));

        TempChp->GGSt = 0x00;
        memset(TempChp->SegaPCM.ChnPrg, 0x00, sizeof(UINT8) * 0x10);
        // FM ADPCM restarts notes always, so leaving them at FF works fine.
        //TempChp->YM2608.RegData[0x010] = 0x00;
        //TempChp->YM2610.RegData[0x100] = 0x00;
        memset(TempChp->YMZ280B.KeyOn, 0x00, sizeof(UINT8) * 0x08);
        TempChp->C140.banking_type = 0x00;
        memset(TempChp->QSound.KeyOn, 0x00, sizeof(UINT8) * 0x10);

        memset(&TempChp->OKIM6258.RegFirst[0x08], 0x00, 0x0D - 0x08);
    }
    memset(StreamFreqs, 0xFF, sizeof(UINT32) * 0x100);
    VGM_Loops = false;

    SetChipSet(0x00);

    return;
}

void ResetAllChips(void)
{
    UINT8 CurChip;
    ALL_CHIPS *TempChp;
    UINT8 RegBak[0x05];
    UINT8 ClkBak[0x05];
    UINT8 VSUBak[0x60];

    for(CurChip = 0x00; CurChip < ChipCount; CurChip ++)
    {
        TempChp = &ChipData[CurChip];
        RegBak[0x00] = TempChp->RF5C68.RegData[RF_CBANK];
        RegBak[0x01] = TempChp->RF5C164.RegData[RF_CBANK];
        RegBak[0x02] = TempChp->C6280.RegData[C6280_CHN_SEL];
        RegBak[0x03] = TempChp->C140.banking_type;
        //RegBak[0x03] = TempChp->YM2608.RegData[0x010];
        //RegBak[0x04] = TempChp->YM2610.RegData[0x100];
        memcpy(ClkBak, &TempChp->OKIM6258.RegData[0x08], 0x05);
        memcpy(VSUBak, &TempChp->VSU.RegData[0x100], 0x60);

        // TODO: reset RegFist for each chip separately
        memset(TempChp, 0xFF, sizeof(ALL_CHIPS));

        TempChp->GGSt = 0x00;
        memset(TempChp->SegaPCM.ChnPrg, 0x00, sizeof(UINT8) * 0x10);
        memset(TempChp->YMZ280B.KeyOn, 0x00, sizeof(UINT8) * 0x08);
        TempChp->C140.banking_type = RegBak[0x03];
        memset(TempChp->QSound.KeyOn, 0x00, sizeof(UINT8) * 0x10);

        TempChp->RF5C68.RegData[RF_CBANK] = RegBak[0x00];
        TempChp->RF5C68.RegData[RF_CHN_LOOP] = RegBak[0x00];
        TempChp->RF5C164.RegData[RF_CBANK] = RegBak[0x01];
        TempChp->RF5C164.RegData[RF_CHN_LOOP] = RegBak[0x01];
        TempChp->C6280.RegData[C6280_CHN_SEL] = RegBak[0x02];
        TempChp->C6280.RegData[C6280_CHN_LOOP] = RegBak[0x02];
        //TempChp->YM2608.RegData[0x010] = RegBak[0x03];
        //TempChp->YM2610.RegData[0x100] = RegBak[0x04];
        memcpy(&TempChp->OKIM6258.RegData[0x08], ClkBak, 0x05);
        memcpy(&TempChp->VSU.RegData[0x100], VSUBak, 0x60);
    }
    memset(StreamFreqs, 0xFF, sizeof(UINT32) * 0x100);

    VGM_Loops = true;

    return;
}

void FreeAllChips(void)
{
    if(ChipData == NULL)
        return;

    free(ChipData);
    ChipData = NULL;

    return;
}

void SetChipSet(UINT8 ChipID)
{
    ChDat = ChipData + ChipID;

    return;
}

bool dac_stream_control_freq(UINT8 strmID, UINT32 freq)
{
    if(JustTimerCmds)
        return true;

    if(freq == StreamFreqs[strmID])
        return false;

    StreamFreqs[strmID] = freq;
    return true;
}

bool GGStereo(UINT8 Data)
{
    if(Data == ChDat->GGSt && ! JustTimerCmds)
        return false;

    ChDat->GGSt = Data;
    return true;
}

bool sn76496_write(UINT8 Command/*, UINT8 NextCmd*/)
{
    SN76496_DATA *chip;
    UINT8 Channel;
    UINT8 Reg;
    UINT8 Data;
    bool RetVal;
    UINT8 NextCmd;

    if(JustTimerCmds)
        return true;

    chip = &ChDat->SN76496;

    RetVal = GetNextChipCommand();
    NextCmd = RetVal ? NxtCmdVal : 0x80;

    RetVal = true;
    if(Command & 0x80)
    {
        Reg = (Command & 0x70) >> 4;
        Channel = Reg >> 1;
        Data = Command & 0x0F;
        chip->LastReg = Reg;

        switch(Reg)
        {
        case 0: // Tone 0: Frequency
        case 2: // Tone 1: Frequency
        case 4: // Tone 2: Frequency
            if(Data == chip->FreqMSB[Channel])
            {
                if(NextCmd & 0x80)
                {
                    // Next command doesn't depend on the current one
                    RetVal = false;
                }
                else
                {
                    Data = NextCmd & 0x7F;  // NextCmd & 0x3F
                    if(Data == chip->FreqLSB[Channel])
                        RetVal = false;
                }
            }
            else
                chip->FreqMSB[Channel] = Data;
            break;
        case 6: // Noise:  Frequency, Mode
            return true;    // a Noise Mode write resets the Noise Shifter
        case 1: // Tone 0: Volume
        case 3: // Tone 1: Volume
        case 5: // Tone 2: Volume
        case 7: // Noise:  Volume
            if((Command & 0x0F) == chip->VolData[Channel])
            {
                if(NextCmd & 0x80)
                {
                    // Next command doesn't depend on the current one
                    RetVal = false;
                }
                else
                {
                    Data = NextCmd & 0x0F;
                    if(Data == chip->VolData[Channel])
                        RetVal = false;
                    printf("Warning! Data Command after Volume Command!\n");
                }
            }
            else
                chip->VolData[Channel] = Data;
            break;
        }
    }
    else
    {
        Reg = chip->LastReg;
        Channel = Reg >> 1;
        Data = Command & 0x7F;  // Command & 0x3F

        if(!(Reg & 0x10))
        {
            if(Data == chip->FreqLSB[Channel])
                RetVal = chip->LastRet; // remove event only, if previous event was removed
            else
                chip->FreqLSB[Channel] = Data;
        }
        else
        {
            // I still handle this correctly
            if(Data == chip->VolData[Channel])
                RetVal = chip->LastRet;
            else
                chip->VolData[Channel] = Data;
        }
    }
    chip->LastRet = RetVal;
    //if (Channel != 0x00)
    //if (Channel != 0x01)
    //if (Channel != 0x02 || (ChDat != ChipData && !(Reg & 0x01)))
    //if (! (Channel == 0x03 || (ChDat != ChipData && Channel == 0x02 && !(Reg & 0x01))))
    //  RetVal = false;

    return RetVal;
}

bool ym2413_write(UINT8 Register, UINT8 Data)
{
    YM2413_DATA *chip = &ChDat->YM2413;

    Register &= 0x3F;

    if(! chip->RegFirst[Register] && Data == chip->RegData[Register])
        return false;

    chip->RegFirst[Register] = JustTimerCmds;
    chip->RegData[Register] = Data;
    return true;
}

bool ym2612_write(UINT8 Port, UINT8 Register, UINT8 Data)
{
    YM2612_DATA *chip = &ChDat->YM2612;
    UINT16 RegVal;
    UINT8 Channel;

    RegVal = (Port << 8) | Register;
    switch(RegVal)
    {
    case 0x024: // Timer Registers
    case 0x025:
    case 0x026:
        return false;
    // no OPN Prescaler Registers for YM2612
    case 0x027:
        Data &= 0xC0;   // mask out all timer-relevant bits

        if(! chip->RegFirst[RegVal] && Data == chip->RegData[RegVal])
            return false;

        chip->RegFirst[RegVal] = 0x00;
        chip->RegData[RegVal] = Data;
        break;
    case 0x028:
        Channel = Data & 0x07;

        if(! chip->KeyFirst[Channel] && Data == chip->KeyOn[Channel])
            return false;

        chip->KeyFirst[Channel] = JustTimerCmds;
        chip->KeyOn[Channel] = Data;
        break;
    case 0x02A:
        /*// Hack for Pier Solar Beta
        // Remove every 2nd DAC command (because every DAC command is sent 2 times)
        // this includes a check and a warning output
        chip->RegFirst[RegVal] ^= 0x01;
        if (chip->RegFirst[RegVal] & 0x01)
        {
            if (Data == chip->RegData[RegVal])
                return false;
            else //if (Data != chip->RegData[RegVal])
                printf("Warning! DAC Compression failed!\n");
        }
        chip->RegData[RegVal] = Data;*/
        return true;    // I leave this on for later optimizations
    default:
        // no SSG emulator for YM2612
        switch(RegVal & 0xF4)
        {
        case 0xA0:  // A0-A3 and A8-AB
            if((RegVal & 0x03) == 0x03)
                break;

            if(! chip->RegFirst[RegVal] && Data == chip->RegData[RegVal])
                return false;

            chip->RegFirst[RegVal] = JustTimerCmds;
            chip->RegData[RegVal] = Data;
            return true;
        case 0xA4:  // A4-A7 and AC-AF - Frequence Latch
            if((RegVal & 0x03) == 0x03)
                break;

            // FINALLY, I got it to work properly
            // The vgm I tested (Dyna Brothers 2 - 28 - Get Crazy - More Rave.vgz) was
            // successfully tested against the Gens and MAME cores.
            while(GetNextChipCommand())
            {
                if((NxtCmdReg & 0x1FC) == (RegVal & 0x1FC))
                {
                    return false;   // this will be ignored, because the A0 write is missing
                }
                else if((NxtCmdReg & 0x1FF) == (RegVal & 0x1FB))
                {
                    if(chip->RegFirst[RegVal])
                    {
                        chip->RegFirst[RegVal] = JustTimerCmds;
                        chip->RegData[RegVal] = Data;
                        chip->RegFirst[RegVal & 0x1FB] = 0x01;
                        return true;
                    }
                    else if(chip->RegData[RegVal] == Data &&
                            chip->RegData[RegVal & 0x1FB] == NxtCmdVal)
                    {
                        chip->RegFirst[RegVal] = JustTimerCmds;
                        chip->RegData[RegVal] = Data;
                        chip->RegFirst[RegVal & 0x1FB] = JustTimerCmds;
                        return false;
                    }
                    else
                    {
                        chip->RegFirst[RegVal] = JustTimerCmds;
                        chip->RegData[RegVal] = Data;
                        chip->RegFirst[RegVal & 0x1FB] = 0x01;
                        return true;
                    }
                }
            }
            chip->RegData[RegVal] = Data;
            return true;
        }

        if(! chip->RegFirst[RegVal] && Data == chip->RegData[RegVal])
            return false;

        chip->RegFirst[RegVal] = JustTimerCmds;
        chip->RegData[RegVal] = Data;
        break;
    }

    return true;
}

bool ym2151_write(UINT8 Register, UINT8 Data)
{
    YM2151_DATA *chip = &ChDat->YM2151;
    UINT8 Channel;

    switch(Register)
    {
    case 0x08:
        Channel = Data & 0x07;
        Data &= 0xF8;
        if(! chip->MCFirst[Channel] && Data == chip->MCMask[Channel])
            return false;

        chip->MCFirst[Channel] = JustTimerCmds;
        chip->MCMask[Channel] = Data;
        break;
    case 0x10:  // Timer Registers
    case 0x11:
    case 0x12:
        return false;
    case 0x14:
        Data &= 0x80;
        if(! chip->RegFirst[Register] && Data == chip->RegData[Register])
            return false;

        chip->RegFirst[Register] = 0x00;
        chip->RegData[Register] = Data;
        break;
    case 0x19:  // AMD / PMD
        Channel = (Data & 0x80) >> 7;
        Data &= 0x7F;
        if(! chip->MDFirst[Channel] && Data == chip->MDMask[Channel])
            return false;

        chip->MDFirst[Channel] = JustTimerCmds;
        chip->MDMask[Channel] = Data;
        break;
    default:
        if(! chip->RegFirst[Register] && Data == chip->RegData[Register])
            return false;

        chip->RegFirst[Register] = JustTimerCmds;
        chip->RegData[Register] = Data;
        break;
    }

    return true;
}

bool segapcm_mem_write(UINT16 Offset, UINT8 Data)
{
    SEGAPCM_DATA *chip = &ChDat->SegaPCM;
    UINT8 Channel;
    UINT16 RelOffset;

    Offset &= 0x07FF;   // it has 2 KB of RAM, but only 256 Byte are used
    Channel = (Offset >> 3) & 0xF;
    RelOffset = Offset & ~0x78;

    if(RelOffset == 0x04 || RelOffset == 0x05)
        RelOffset |= 0x80;  // patch for the old SegaPCM core
    switch(RelOffset)
    {
    case 0x07:  // Sample Delta Time
        if(! chip->RAMFirst[Offset] && Data == chip->RAMData[Offset])
            return false;

        // if Sample Delta Time is 0x00, it's like a KeyOff
        chip->ChnPrg[Channel] &= ~0x02;
        chip->ChnPrg[Channel] |= Data ? 0x02 : 0x00;
        if(chip->ChnPrg[Channel] == 0x03)
        {
            chip->RAMFirst[(Channel << 3) | 0x84] |= 0x01;
            chip->RAMFirst[(Channel << 3) | 0x85] |= 0x01;
            chip->RAMFirst[(Channel << 3) | 0x86] |= 0x01;
            chip->RAMFirst[(Channel << 3) | 0x04] |= 0x01;
            chip->RAMFirst[(Channel << 3) | 0x05] |= 0x01;
        }

        chip->RAMFirst[Offset] = JustTimerCmds;
        chip->RAMData[Offset] = Data;
        break;
    case 0x84:  // Current Address L
    case 0x85:  // Current Address H
        if(! chip->RAMFirst[Offset] && Data == chip->RAMData[Offset])
            return false;

        // the chip modifies the Current Address while playing,
        // so they must be rewritten of a channel is active
        chip->RAMFirst[Offset] = JustTimerCmds | (chip->ChnPrg[Channel] == 0x03);
        chip->RAMData[Offset] = Data;
        break;
    case 0x86:  // Channel Disable (Bit 0), Loop Disable (Bit 1), Bank
        if(! chip->RAMFirst[Offset] && Data == chip->RAMData[Offset])
            return false;

        chip->ChnPrg[Channel] &= ~0x01;
        chip->ChnPrg[Channel] |= (~Data & 0x01);
        if(chip->ChnPrg[Channel] == 0x03)
        {
            // the Current Address registers must be written the next time
            chip->RAMFirst[(Channel << 3) | 0x84] |= 0x01;
            chip->RAMFirst[(Channel << 3) | 0x85] |= 0x01;
            chip->RAMFirst[(Channel << 3) | 0x04] |= 0x01;
            chip->RAMFirst[(Channel << 3) | 0x05] |= 0x01;
        }

        // like above, the Channel register gets modified by the chip,
        // so the same rules apply
        chip->RAMFirst[Offset] = JustTimerCmds | (chip->ChnPrg[Channel] == 0x03);
        chip->RAMData[Offset] = Data;
        break;
    default:
        if(! chip->RAMFirst[Offset] && Data == chip->RAMData[Offset])
            return false;

        chip->RAMFirst[Offset] = JustTimerCmds;
        chip->RAMData[Offset] = Data;
        break;
    }

    return true;
}

static bool rf_pcm_reg_write(RF5C68_DATA *chip, UINT8 Register, UINT8 Data)
{
    RF5C68_CHANNEL *chan;
    UINT8 OldVal;

    switch(Register)
    {
    case 0x00:  // Envelope
    case 0x01:  // Pan
    case 0x02:  // FD Low / step
    case 0x03:  // FD High / step
    case 0x04:  // Loop Start Low
    case 0x05:  // Loop Start High
    case 0x06:  // Start
        chan = &chip->chan[chip->RegData[RF_CBANK] & 0x07];

        if(chip->RegFirst[RF_CHN_LOOP])
            chip->RegFirst[RF_CHN_LOOP] = 0x00;

        if(Register == 0x06)
        {
            OldVal = chip->RegData[RF_CHN_MASK] & (0x01 << (chip->RegData[RF_CBANK] & 0x07));
            if(! OldVal)
                chan->RegFirst[Register] = 0x01;
        }

        if(! chan->RegFirst[Register] && Data == chan->ChnReg[Register])
            return false;

        chan->RegFirst[Register] = JustTimerCmds;
        chan->ChnReg[Register] = Data;
        break;
    case 0x07:  // Control Register
        OldVal = chip->RegData[RF_ENABLE];
        if(Data & 0x40)
            OldVal |= chip->RegData[RF_CBANK];
        else
            OldVal |= chip->RegData[RF_WBANK];
        if(! chip->RegFirst[RF_ENABLE] && Data == OldVal)
            return false;

        if(/*! chip->RegFirst[RF_ENABLE] &&*/ (Data & 0x40))
        {
            // additional test for 2 Channel Select-Commands after each other
            // that makes first one useless, of course :)
            OldVal = 0x00;
            while(GetNextChipCommand())
            {
                if(NxtCmdReg <= 0x06)
                {
                    OldVal = 0x01;
                    break;
                }
                else if(NxtCmdReg == 0x07 && (NxtCmdVal & 0x40))
                    return false;
            }
            if(! OldVal)
            {
                // see HuC6280 section for notes for this if
                if(! VGM_Loops || (chip->RegFirst[RF_CHN_LOOP] ||
                                   chip->RegData[RF_CHN_LOOP] == 0x80))
                {
                    // when no command follows the Channel Select one, it's useless too
                    return false;
                }
            }
        }

        chip->RegFirst[RF_ENABLE] = JustTimerCmds;
        chip->RegData[RF_ENABLE] = Data & 0x80;
        if(Data & 0x40)
        {
            chip->RegData[RF_CBANK] = Data;
            if(chip->RegFirst[RF_CHN_LOOP])
            {
                // is the Channel Select the first command after the loop?
                chip->RegFirst[RF_CHN_LOOP] = 0x00;
                chip->RegData[RF_CHN_LOOP] = 0x80;  // set to 'ignore'
            }
        }
        else
            chip->RegData[RF_WBANK] = Data;
        break;
    case 0x08:  // Channel On/Off Register
        if(! chip->RegFirst[RF_CHN_MASK] && Data == chip->RegData[RF_CHN_MASK])
            return false;

        chip->RegFirst[RF_CHN_MASK] = JustTimerCmds;
        chip->RegData[RF_CHN_MASK] = Data;
        break;
    }

    return true;
}

bool rf5c68_reg_write(UINT8 Register, UINT8 Data)
{
    RF5C68_DATA *chip = &ChDat->RF5C68;

    return rf_pcm_reg_write(chip, Register, Data);
}

bool ym2203_write(UINT8 Register, UINT8 Data)
{
    YM2203_DATA *chip = &ChDat->YM2203;
    UINT8 Channel;

    /*if ((Register & 0x1F0) == 0x000)
        return ! (Register >= 0x0E && Register <= 0x0F);
    else
        return false;*/
    switch(Register)
    {
    case 0x24:  // Timer Registers
    case 0x25:
    case 0x26:
        return false;
    case 0x27:
        Data &= 0xC0;   // mask out all timer-relevant bits

        if(! chip->RegFirst[Register] && Data == chip->RegData[Register])
            return false;

        chip->RegFirst[Register] = 0x00;
        chip->RegData[Register] = Data;
        break;
    case 0x2D:  // OPN Prescaler Registers
    case 0x2E:
    case 0x2F:
        if(chip->PreSclCmd == Register)
            return false;
        chip->PreSclCmd = Register;
        break;
    case 0x28:
        Channel = Data & 0x03;

        if(! chip->KeyFirst[Channel] && Data == chip->KeyOn[Channel])
            return false;

        chip->KeyFirst[Channel] = JustTimerCmds;
        chip->KeyOn[Channel] = Data;
        break;
    default:
        if((Register & 0x1F0) == 0x000)
        {
            // SSG emulator (AY8910)
            return ay8910_part_write(chip->RegData, chip->RegFirst, Register & 0x0F, Data);
        }

        switch(Register & 0xF4)
        {
        case 0xA0:  // A0-A3 and A8-AB
            if((Register & 0x03) == 0x03)
                break;

            if(! chip->RegFirst[Register] && Data == chip->RegData[Register])
                return false;

            chip->RegFirst[Register] = JustTimerCmds;
            chip->RegData[Register] = Data;
            return true;
        case 0xA4:  // A4-A7 and AC-AF - Frequence Latch
            if((Register & 0x03) == 0x03)
                break;

            while(GetNextChipCommand())
            {
                if((NxtCmdReg & 0xFC) == (Register & 0xFC))
                {
                    return false;   // this will be ignored, because the A0 write is missing
                }
                else if((NxtCmdReg & 0xFF) == (Register & 0xFB))
                {
                    if(chip->RegFirst[Register])
                    {
                        chip->RegFirst[Register] = JustTimerCmds;
                        chip->RegData[Register] = Data;
                        chip->RegFirst[Register & 0xFB] = 0x01;
                        return true;
                    }
                    else if(chip->RegData[Register] == Data &&
                            chip->RegData[Register & 0xFB] == NxtCmdVal)
                    {
                        chip->RegFirst[Register] = JustTimerCmds;
                        chip->RegData[Register] = Data;
                        chip->RegFirst[Register & 0xFB] = JustTimerCmds;
                        return false;
                    }
                    else
                    {
                        chip->RegFirst[Register] = JustTimerCmds;
                        chip->RegData[Register] = Data;
                        chip->RegFirst[Register & 0xFB] = 0x01;
                        return true;
                    }
                }
            }
            chip->RegData[Register] = Data;
            return true;
        }

        if(! chip->RegFirst[Register] && Data == chip->RegData[Register])
            return false;

        chip->RegFirst[Register] = JustTimerCmds;
        chip->RegData[Register] = Data;
        break;
    }

    return true;
}

bool ym2608_write(UINT8 Port, UINT8 Register, UINT8 Data)
{
    YM2608_DATA *chip = &ChDat->YM2608;
    UINT16 RegVal;
    UINT8 Channel;

    RegVal = (Port << 8) | Register;
    switch(RegVal)
    {
    case 0x024: // Timer Registers
    case 0x025:
    case 0x026:
        return false;
    case 0x027:
        Data &= 0xC0;   // mask out all timer-relevant bits

        if(! chip->RegFirst[RegVal] && Data == chip->RegData[RegVal])
            return false;

        chip->RegFirst[RegVal] = 0x00;
        chip->RegData[RegVal] = Data;
        break;
    case 0x02D: // OPN Prescaler Registers
    case 0x02E:
    case 0x02F:
        if(chip->PreSclCmd == Register)
            return false;
        break;
    case 0x028:
        Channel = Data & 0x07;

        if(! chip->KeyFirst[Channel] && Data == chip->KeyOn[Channel])
            return false;

        chip->KeyFirst[Channel] = JustTimerCmds;
        chip->KeyOn[Channel] = Data;
        break;
    case 0x029: // IRQ Mask and 3/6 ch mode
        Data &= 0x80;   // mask out all IRQ-relevant bits

        if(! chip->RegFirst[RegVal] && Data == chip->RegData[RegVal])
            return false;

        chip->RegFirst[RegVal] = 0x00;
        chip->RegData[RegVal] = Data;
        break;
    case 0x110: // IRQ Flag control
        return false;
    default:
        if((RegVal & 0x1F0) == 0x000)
        {
            // SSG emulator (AY8910)
            return ay8910_part_write(chip->RegData, chip->RegFirst, Register & 0x0F, Data);
        }
        else if((RegVal & 0x1F0) == 0x010)  // ADPCM
        {
            return fmadpcm_write(RegVal & 0x0F, Data, &chip->RegData[0x010],
                                 &chip->RegFirst[0x010]);
        }
        else if((RegVal & 0x1F0) == 0x100)  // DELTA-T
        {
            if((RegVal & 0x0F) < 0x0E)  // DAC Data is handled like DAC of YM2612
                return ymdeltat_write(RegVal & 0x0F, Data, &chip->RegData[0x100],
                                      &chip->RegFirst[0x100]);
            else
                return true;
        }

        switch(RegVal & 0xF4)
        {
        case 0xA0:  // A0-A3 and A8-AB
            if((RegVal & 0x03) == 0x03)
                break;

            if(! chip->RegFirst[RegVal] && Data == chip->RegData[RegVal])
                return false;

            chip->RegFirst[RegVal] = JustTimerCmds;
            chip->RegData[RegVal] = Data;
            return true;
        case 0xA4:  // A4-A7 and AC-AF - Frequence Latch
            if((RegVal & 0x03) == 0x03)
                break;

            while(GetNextChipCommand())
            {
                if((NxtCmdReg & 0x1FC) == (RegVal & 0x1FC))
                {
                    return false;   // this will be ignored, because the A0 write is missing
                }
                else if((NxtCmdReg & 0x1FF) == (RegVal & 0x1FB))
                {
                    if(chip->RegFirst[RegVal])
                    {
                        chip->RegFirst[RegVal] = JustTimerCmds;
                        chip->RegData[RegVal] = Data;
                        chip->RegFirst[RegVal & 0x1FB] = 0x01;
                        return true;
                    }
                    else if(chip->RegData[RegVal] == Data &&
                            chip->RegData[RegVal & 0x1FB] == NxtCmdVal)
                    {
                        chip->RegFirst[RegVal] = JustTimerCmds;
                        chip->RegData[RegVal] = Data;
                        chip->RegFirst[RegVal & 0x1FB] = JustTimerCmds;
                        return false;
                    }
                    else
                    {
                        chip->RegFirst[RegVal] = JustTimerCmds;
                        chip->RegData[RegVal] = Data;
                        chip->RegFirst[RegVal & 0x1FB] = 0x01;
                        return true;
                    }
                }
            }
            chip->RegData[RegVal] = Data;
            return true;
        }

        if(! chip->RegFirst[RegVal] && Data == chip->RegData[RegVal])
            return false;

        chip->RegFirst[RegVal] = JustTimerCmds;
        chip->RegData[RegVal] = Data;
        break;
    }

    return true;
}

bool ym2610_write(UINT8 Port, UINT8 Register, UINT8 Data)
{
    YM2610_DATA *chip = &ChDat->YM2610;
    UINT16 RegVal;
    UINT8 Channel;

    RegVal = (Port << 8) | Register;
    switch(RegVal)
    {
    case 0x24:  // Timer Registers
    case 0x25:
    case 0x26:
        return false;
    // no OPN Prescaler Registers for YM2610
    case 0x27:
        Data &= 0xC0;   // mask out all timer-relevant bits

        if(! chip->RegFirst[RegVal] && Data == chip->RegData[RegVal])
            return false;

        chip->RegFirst[RegVal] = 0x00;
        chip->RegData[RegVal] = Data;
        break;
    case 0x28:
        Channel = Data & 0x07;

        if(! chip->KeyFirst[Channel] && Data == chip->KeyOn[Channel])
            return false;

        chip->KeyFirst[Channel] = JustTimerCmds;
        chip->KeyOn[Channel] = Data;
        break;
    default:
        if(RegVal == 0x1C)
            return false;   // controls only Status Bits
        if((RegVal & 0x1F0) == 0x000)
        {
            // SSG emulator (AY8910)
            return ay8910_part_write(chip->RegData, chip->RegFirst, Register & 0x0F, Data);
        }
        else if((RegVal & 0x1F0) == 0x010)  // DELTA-T
        {
            if((RegVal & 0x0F) < 0x0C)
                return ymdeltat_write(RegVal & 0x0F, Data, &chip->RegData[0x010],
                                      &chip->RegFirst[0x010]);
            else if((RegVal & 0x0F) == 0x0C)    // Flag Control
                return false;
            else
                return true;
        }
        else if((RegVal & 0x1F0) >= 0x100 && (RegVal & 0x1F0) < 0x130)  // ADPCM
        {
            return fmadpcm_write(RegVal & 0x3F, Data, &chip->RegData[0x100],
                                 &chip->RegFirst[0x100]);
        }

        switch(RegVal & 0xF4)
        {
        case 0xA0:  // A0-A3 and A8-AB
            if((RegVal & 0x03) == 0x03)
                break;

            if(! chip->RegFirst[RegVal] && Data == chip->RegData[RegVal])
                return false;

            chip->RegFirst[RegVal] = JustTimerCmds;
            chip->RegData[RegVal] = Data;
            return true;
        case 0xA4:  // A4-A7 and AC-AF - Frequence Latch
            if((RegVal & 0x03) == 0x03)
                break;

            while(GetNextChipCommand())
            {
                if((NxtCmdReg & 0x1FC) == (RegVal & 0x1FC))
                {
                    return false;   // this will be ignored, because the A0 write is missing
                }
                else if((NxtCmdReg & 0x1FF) == (RegVal & 0x1FB))
                {
                    if(chip->RegFirst[RegVal])
                    {
                        chip->RegFirst[RegVal] = JustTimerCmds;
                        chip->RegData[RegVal] = Data;
                        chip->RegFirst[RegVal & 0x1FB] = 0x01;
                        return true;
                    }
                    else if(chip->RegData[RegVal] == Data &&
                            chip->RegData[RegVal & 0x1FB] == NxtCmdVal)
                    {
                        chip->RegFirst[RegVal] = JustTimerCmds;
                        chip->RegData[RegVal] = Data;
                        chip->RegFirst[RegVal & 0x1FB] = JustTimerCmds;
                        return false;
                    }
                    else
                    {
                        chip->RegFirst[RegVal] = JustTimerCmds;
                        chip->RegData[RegVal] = Data;
                        chip->RegFirst[RegVal & 0x1FB] = 0x01;
                        return true;
                    }
                }
            }
            chip->RegData[RegVal] = Data;
            return true;
        }

        if(! chip->RegFirst[RegVal] && Data == chip->RegData[RegVal])
            return false;

        chip->RegFirst[RegVal] = JustTimerCmds;
        chip->RegData[RegVal] = Data;
        break;
    }

    return true;
}

bool ym3812_write(UINT8 Register, UINT8 Data)
{
    switch(Register)
    {
    case 0x02:  // IRQ and Timer Registers
    case 0x03:
        return false;
    case 0x04:
        return false;
    /*if (Data & 0x80)
        Data &= 0x80;

    if (! ChDat->YM3812.RegFirst[Register] && Data == ChDat->YM3812.RegData[Register])
        return false;

    ChDat->YM3812.RegFirst[Register] = 0x00;
    ChDat->YM3812.RegData[Register] = Data;
    break;*/
    default:
        if(! ChDat->YM3812.RegFirst[Register] && Data == ChDat->YM3812.RegData[Register])
            return false;


        if(Register == 0x01)
        {
            if((ChDat->YM3812.RegData[Register] ^ Data) & 0x20)
            {
                UINT8 reg;
                for(reg = 0xFF; reg >= 0xE0; reg ++)
                    ChDat->YM3812.RegFirst[Register] = 0x01;
            }
        }

        ChDat->YM3812.RegFirst[Register] = JustTimerCmds;
        ChDat->YM3812.RegData[Register] = Data;
        break;
    }

    return true;
}

bool ym3526_write(UINT8 Register, UINT8 Data)
{
    switch(Register)
    {
    case 0x02:  // IRQ and Timer Registers
    case 0x03:
        return false;
    case 0x04:
        return false;
    /*if (Data & 0x80)
        Data &= 0x80;

    if (! ChDat->YM3526.RegFirst[Register] && Data == ChDat->YM3526.RegData[Register])
        return false;

    ChDat->YM3526.RegFirst[Register] = 0x00;
    ChDat->YM3526.RegData[Register] = Data;
    break;*/
    default:
        if(! ChDat->YM3526.RegFirst[Register] && Data == ChDat->YM3526.RegData[Register])
            return false;

        ChDat->YM3526.RegFirst[Register] = JustTimerCmds;
        ChDat->YM3526.RegData[Register] = Data;
        break;
    }

    return true;
}

bool y8950_write(UINT8 Register, UINT8 Data)
{
    switch(Register)
    {
    case 0x02:  // IRQ and Timer Registers
    case 0x03:
        return false;
    case 0x04:
        return false;
    /*if (Data & 0x80)
        Data &= 0x80;

    if (! ChDat->Y8950.RegFirst[Register] && Data == ChDat->Y8950.RegData[Register])
        return false;

    ChDat->Y8950.RegFirst[Register] = 0x00;
    ChDat->Y8950.RegData[Register] = Data;
    break;*/
    default:
        if(Register >= 0x07 && Register <= 0x12)
            return ymdeltat_write(Register - 0x07, Data, &ChDat->Y8950.RegData[0x07],
                                  &ChDat->Y8950.RegFirst[0x07]);
        if(! ChDat->Y8950.RegFirst[Register] && Data == ChDat->Y8950.RegData[Register])
            return false;

        ChDat->Y8950.RegFirst[Register] = JustTimerCmds;
        ChDat->Y8950.RegData[Register] = Data;
        break;
    }

    return true;
}

bool ymf262_write(UINT8 Port, UINT8 Register, UINT8 Data)
{
    YMF262_DATA *chip = &ChDat->YMF262;
    UINT16 RegVal;

    RegVal = (Port << 8) | Register;
    switch(RegVal)
    {
    case 0x002: // IRQ and Timer Registers
    case 0x003:
        return false;
    case 0x004:
        return false;
    /*if (Data & 0x80)
        Data &= 0x80;

    if (! chip->RegFirst[RegVal] && Data == chip->RegData[RegVal])
        return false;

    chip->RegFirst[RegVal] = 0x00;
    chip->RegData[RegVal] = Data;
    break;*/
    default:
        if(! chip->RegFirst[RegVal] && Data == chip->RegData[RegVal])
            return false;

        chip->RegFirst[RegVal] = JustTimerCmds;
        chip->RegData[RegVal] = Data;
        break;
    }

    return true;
}

bool ymf278b_write(UINT8 Port, UINT8 Register, UINT8 Data)
{
    YMF278B_DATA *chip = &ChDat->YMF278B;
    UINT16 RegVal;

    if(Port < 0x02)
    {
        RegVal = (Port << 8) | Register;
        switch(RegVal)
        {
        case 0x002: // IRQ and Timer Registers
        case 0x003:
            return false;
        case 0x004:
            return false;
        /*if (Data & 0x80)
            Data &= 0x80;

        if (! chip->RegFirst[RegVal] && Data == chip->RegData[RegVal])
            return false;

        chip->RegFirst[RegVal] = 0x00;
        chip->RegData[RegVal] = Data;
        break;*/
        default:
            if(! chip->RegFirst[RegVal] && Data == chip->RegData[RegVal])
                return false;

            chip->RegFirst[RegVal] = JustTimerCmds;
            chip->RegData[RegVal] = Data;
            break;
        }
    }
    else
        return true;

    return true;
}

bool ymz280b_write(UINT8 Register, UINT8 Data)
{
    YMZ280B_DATA *chip = &ChDat->YMZ280B;

    //  // the KeyOn-Register can be sent 2x to stop a sound instantly
    //  if ((Register & 0xE3) == 0x01)
    //      return true;

    if(! chip->RegFirst[Register] && Data == chip->RegData[Register])
        return false;

    chip->RegFirst[Register] = JustTimerCmds;
    chip->RegData[Register] = Data;

    return true;
}

bool rf5c164_reg_write(UINT8 Register, UINT8 Data)
{
    RF5C68_DATA *chip = &ChDat->RF5C164;

    return rf_pcm_reg_write(chip, Register, Data);
}

static bool ay8910_part_write(UINT8 *RegData, UINT8 *RegFirst, UINT8 Register, UINT8 Data)
{
    Register &= 0x0F;

    switch(Register)
    {
    case 0x07:
        Data &= 0x3F;   // mask out Port bits
        break;
    case 0x0D:  // EShape register (always resets some global values)
        return true;
    case 0x0E:  // Port A write
    case 0x0F:  // Port B write
        return false;
    }

    if(! RegFirst[Register] && Data == RegData[Register])
        return false;

    RegFirst[Register] = JustTimerCmds;
    RegData[Register] = Data;
    return true;
}

bool ay8910_write_reg(UINT8 Register, UINT8 Data)
{
    return ay8910_part_write(ChDat->AY8910.RegData, ChDat->AY8910.RegFirst, Register, Data);
}

static bool ymf271_write_fm_reg(YMF271_DATA *chip, UINT8 SlotNum, UINT8 Register, UINT8 Data)
{
    YMF271_SLOT *slot = &chip->slots[SlotNum];

    if(Register == 0x0A)    // Frequency MSB Latch (confirmed/flushed by Register 09)
    {
        // Note: Based on the code for A0/A4 on OPN chips.
        UINT16 RegBase;

        RegBase = (SlotNum % 3) << 0;
        RegBase |= ((SlotNum / 3) & 0x03) << 2;
        RegBase |= (SlotNum / 12) << 8;
        while(GetNextChipCommand())
        {
            if((NxtCmdReg & 0xF0F) != RegBase)
                continue;   // ignore other channels

            if((NxtCmdReg & 0x0F0) == 0x0A0)
            {
                return false;   // this will be ignored, because the 09 (flushing Freq LSB) write is missing
            }
            else if((NxtCmdReg & 0x0F0) == 0x090)
            {
                if(slot->RegFirst[0x0A])
                {
                    slot->RegFirst[0x0A] = JustTimerCmds;
                    slot->RegData[0x0A] = Data;
                    slot->RegFirst[0x09] = 0x01;
                    return true;
                }
                else if(slot->RegData[0x0A] == Data &&
                        slot->RegData[0x09] == NxtCmdVal)
                {
                    slot->RegFirst[0x0A] = JustTimerCmds;
                    slot->RegData[0x0A] = Data;
                    slot->RegFirst[0x09] = JustTimerCmds;
                    return false;
                }
                else
                {
                    slot->RegFirst[0x0A] = JustTimerCmds;
                    slot->RegData[0x0A] = Data;
                    slot->RegFirst[0x09] = 0x01;
                    return true;
                }
            }
        }
        slot->RegData[0x0A] = Data;
        return true;
    }

    if(Register == 0x00 && (Data & 0x01))
        slot->RegFirst[Register] = 0x01;    // a Key On triggers always

    if(! slot->RegFirst[Register] && slot->RegData[Register] == Data)
        return false;

    slot->RegFirst[Register] = JustTimerCmds;
    slot->RegData[Register] = Data;
    return true;
}

static bool ymf271_write_fm(YMF271_DATA *chip, UINT8 Port, UINT8 Register, UINT8 Data)
{
    YMF271_SLOT *slot;
    UINT8 SlotReg;
    UINT8 SlotNum;
    UINT8 SyncMode;
    UINT8 SyncReg;
    UINT8 RetVal;

    (void)slot; // Supress the "never used" warning

    if((Register & 0x03) == 0x03)
        return true;

    SlotNum = ((Register & 0x0F) / 0x04 * 0x03) + (Register & 0x03);
    slot = &chip->slots[12 * Port + SlotNum];
    SlotReg = (Register >> 4) & 0x0F;
    if(SlotNum >= 12 || 12 * Port > 48)
        printf("Error");

    // check if the register is a synchronized register
    switch(SlotReg)
    {
    case  0:
    case  9:
    case 10:
    case 12:
    case 13:
    case 14:
        SyncReg = 1;
        break;
    default:
        SyncReg = 0;
        break;
    }

    // check if the slot is key on slot for synchronizing
    SyncMode = 0;
    switch(chip->groups[SlotNum].sync)
    {
    case 0:     // 4 slot mode
        if(Port == 0)
            SyncMode = 1;
        break;
    case 1:     // 2x 2 slot mode
        if(Port == 0 || Port == 1)
            SyncMode = 1;
        break;
    case 2:     // 3 slot + 1 slot mode
        if(Port == 0)
            SyncMode = 1;
        break;
    default:
        break;
    }

    if(SyncMode && SyncReg)         // key-on slot & synced register
    {
        //RetVal = false;
        switch(chip->groups[SlotNum].sync)
        {
        case 0:     // 4 slot mode
            //  RetVal |= ymf271_write_fm_reg(&chip->slots[(12 * 0) + SlotNum], SlotReg, Data);
            //  RetVal |= ymf271_write_fm_reg(&chip->slots[(12 * 1) + SlotNum], SlotReg, Data);
            //  RetVal |= ymf271_write_fm_reg(&chip->slots[(12 * 2) + SlotNum], SlotReg, Data);
            //  RetVal |= ymf271_write_fm_reg(&chip->slots[(12 * 3) + SlotNum], SlotReg, Data);
            chip->slots[(12 * 1) + SlotNum].RegFirst[SlotReg] = 0x02;
            chip->slots[(12 * 2) + SlotNum].RegFirst[SlotReg] = 0x02;
            chip->slots[(12 * 3) + SlotNum].RegFirst[SlotReg] = 0x02;
            break;
        case 1:     // 2x 2 slot mode
            if(Port == 0)       // Slot 1 - Slot 3
            {
                //      RetVal |= ymf271_write_fm_reg(&chip->slots[(12 * 0) + SlotNum], SlotReg, Data);
                //      RetVal |= ymf271_write_fm_reg(&chip->slots[(12 * 2) + SlotNum], SlotReg, Data);
                chip->slots[(12 * 2) + SlotNum].RegFirst[SlotReg] = 0x02;
            }
            else                // Slot 2 - Slot 4
            {
                //      RetVal |= ymf271_write_fm_reg(&chip->slots[(12 * 1) + SlotNum], SlotReg, Data);
                //      RetVal |= ymf271_write_fm_reg(&chip->slots[(12 * 3) + SlotNum], SlotReg, Data);
                chip->slots[(12 * 3) + SlotNum].RegFirst[SlotReg] = 0x02;
            }
            break;
        case 2:     // 3 slot + 1 slot mode
            // 1 slot is handled normally
            //  RetVal |= ymf271_write_fm_reg(&chip->slots[(12 * 0) + SlotNum], SlotReg, Data);
            //  RetVal |= ymf271_write_fm_reg(&chip->slots[(12 * 1) + SlotNum], SlotReg, Data);
            //  RetVal |= ymf271_write_fm_reg(&chip->slots[(12 * 2) + SlotNum], SlotReg, Data);
            chip->slots[(12 * 1) + SlotNum].RegFirst[SlotReg] = 0x02;
            chip->slots[(12 * 2) + SlotNum].RegFirst[SlotReg] = 0x02;
            break;
        default:
            break;
        }
    }
    /*else*/        // write register normally
    {
        RetVal = ymf271_write_fm_reg(chip, 12 * Port + SlotNum, SlotReg, Data);
    }

    return RetVal;
}

bool ymf271_write(UINT8 Port, UINT8 Register, UINT8 Data)
{
    YMF271_DATA *chip = &ChDat->YMF271;
    YMF271_SLOT *slot;
    YMF271_GROUP *group;
    UINT8 GrpNum;
    UINT8 SlotNum;
    UINT8 Addr;

    (void)Addr; // Supress the "never used" warning

    switch(Port)
    {
    case 0x00:
    case 0x01:
    case 0x02:
    case 0x03:
        return ymf271_write_fm(chip, Port, Register, Data);
    case 0x04:
        if((Register & 0x03) == 0x03)
            return true;

        SlotNum = ((Register & 0x0F) / 0x04 * 0x03) + (Register & 0x03);
        Addr = (Register >> 4) % 3;
        slot = &chip->slots[SlotNum * 4];

        Register = (Register >> 4) & 0x0F;
        if(! slot->PCMRegFirst[Register] && slot->PCMRegData[Register] == Data)
            return false;

        slot->PCMRegFirst[Register] = JustTimerCmds;
        slot->PCMRegData[Register] = Data;

        /*switch((Register >> 4) & 0x0F)
        {
        case 0:
        case 1:
        case 2:
            slot->startaddr[Addr] = Data;
            return true;
        case 3:
        case 4:
        case 5:
            slot->endaddr[Addr] = Data;
            return true;
        case 6:
        case 7:
        case 8:
            slot->loopaddr[Addr] = Data;
            return true;
        case 9:
            if (! slot->sltnfirst && slot->slotnote == Data)
                return false;
            slot->sltnfirst = JustTimerCmds;
            slot->slotnote = Data;
            break;
        }*/
        break;
    case 0x06:
        if(!(Register & 0xF0))
        {
            if((Register & 0x03) == 0x03)
                return true;

            GrpNum = ((Register & 0x0F) / 0x04 * 0x03) + (Register & 0x03);
            group = &chip->groups[GrpNum];

            if(! group->First && group->Data == Data)
                return false;
            group->First = JustTimerCmds;
            group->Data = Data;
            if(group->sync != (Data & 0x03))
            {
                group->sync = Data & 0x03;
                // TODO: enforce rewrite of all registers
                for(SlotNum = 0; SlotNum < 48; SlotNum += 12)
                {
                    slot = &chip->slots[SlotNum + GrpNum];
                    memset(slot->RegFirst, 0x80, 0x10);
                }
            }
        }
        else
        {
            switch(Register)
            {
            case 0x10:  // Timer A LSB
            case 0x11:  // Timer A MSB
            case 0x12:  // Timer B
            case 0x13:  // Timer A/B Load, Timer A/B IRQ Enable, Timer A/B Reset
                return false;
            case 0x14:
                chip->ext_address[0] = Data;
                return true;
            case 0x15:
                chip->ext_address[1] = Data;
                return true;
            case 0x16:
                chip->ext_address[2] = Data;
                chip->ext_read = Data & 0x80;
                //if (! chip->ext_read)
                //  chip->ext_address[2] = (chip->ext_address[2] + 1) & 0x7FFFFF;
                return true;
            case 0x17:
                //chip->ext_address[2] = (chip->ext_address[2] + 1) & 0x7FFFFF;
                return true;
            }
        }
        break;
    }

    return true;
}

bool gameboy_write_reg(UINT8 Register, UINT8 Data)
{
    GBDMG_DATA *chip = &ChDat->GBDMG;

    if(Register >= 0x30)
        return true;    // invalid registers

    if(! chip->RegFirst[Register] && Data == chip->RegData[Register])
        return false;

    if(Register == 0x16)
    {
        if(!(Data & 0x80))
        {
            UINT8 CurReg;

            for(CurReg = 0x00; CurReg < 0x16; CurReg ++)
                chip->RegFirst[CurReg] = 0x01;
        }
        return true;
    }
    if(Register < 0x20 && !(chip->RegData[0x16] & 0x80))
        return false;   // when the chip is off, writes have no effect (except Wave RAM)
    return true;    // disable further optimization until I figure out what doesn't break actual GB hardware

    // uncomment these lines for sample-accurateness
    // (otherwise the squares may change one sample too late)
    // the less accurate way has the advantage of slightly cleaner squares
    /*if ((Register == 0x02) || (Register >= 0x07 && Register <= 0x08))
        chip->RegFirst[Register] = 0x01;    // Channel Initialize
    else*/
    if((Register == 0x04 || Register == 0x09 || Register == 0x0E || Register == 0x13) &&
       (Data & 0x80))
        chip->RegFirst[Register] = 0x01;    // Channel Initialize
    else
        chip->RegFirst[Register] = JustTimerCmds;
    chip->RegData[Register] = Data;

    return true;
}

static bool ymdeltat_write(UINT8 Register, UINT8 Data, UINT8 *RegData, UINT8 *RegFirst)
{
    switch(Register)
    {
    case 0x00:  // start, rec, memory mode, repeat flag copy, reset(bit0)
        if(! RegFirst[Register] && Data == RegData[Register])
            return false;

        RegData[Register] = Data & (0x80 | 0x40 | 0x20 | 0x10 | 0x01);
        if(RegData[Register] & 0x80)
            RegFirst[Register] = 0x01;
        else
            RegFirst[Register] = JustTimerCmds;

        break;
    case 0x01:  // L,R,-,-,SAMPLE,DA/AD,RAMTYPE,ROM
    case 0x02:  // Start Address L
    case 0x03:  // Start Address H
    case 0x04:  // Stop Address L
    case 0x05:  // Stop Address H
    case 0x06:  // Prescale L (ADPCM and Record frq)
    case 0x07:  // Prescale H
        if(! RegFirst[Register] && Data == RegData[Register])
            return false;

        RegData[Register] = Data;
        RegFirst[Register] = JustTimerCmds;
        break;
    case 0x08:  // ADPCM data
        return true;
    /*RegData[Register] = Data;
    RegFirst[Register] = 0x01;
    break;*/
    case 0x09:  // DELTA-N L (ADPCM Playback Prescaler)
    case 0x0A:  // DELTA-N H
    case 0x0B:  // Output level control (volume, linear)
    case 0x0C:  // Limit Address L
    case 0x0D:  // Limit Address H
        if(! RegFirst[Register] && Data == RegData[Register])
            return false;

        RegData[Register] = Data;
        RegFirst[Register] = JustTimerCmds;
        break;
    }

    return true;
}

bool nes_psg_write(UINT8 Register, UINT8 Data)
{
    NESAPU_DATA *chip = &ChDat->NES;
    UINT8 CurChn;
    bool ChnIsOn;

    if(Register >= 0x40)
        return true;    // invalid registers
    //if (Register >= 0x10 && Register <= 0x13)
    //  return false;   // remove all DPCM writes

    ChnIsOn = false;
    if(Register < 0x14)
    {
        if(Register < 0x10 && (Register & 0x03) == 0x03)
        {
            ChnIsOn = true; // reset VBL Length register
        }
        else
        {
            CurChn = Register >> 2;
            if(CurChn <= 0x01 && (Register & 0x03) == 0x02)
                ChnIsOn = true; // reset Frequency register (changed, if Sweep is used)
            else if(CurChn == 0x03 && (Register & 0x03) == 0x02)
                ChnIsOn = true; // freq. write resets noise sample
            else if(CurChn == 0x04 && (Register & 0x03) == 0x02)
                ChnIsOn = true;
            else if(CurChn == 0x04 && (Register & 0x03) == 0x01)
            {
#ifdef REMOVE_NES_DPCM_0
                if(Data == 0x00)
                    return false;
                else
                    printf("Warning: DPCM Write: %02X\n", Data);
                getchar();
#endif
                return true;    // direct DPCM write
            }
        }

        if(ChnIsOn)
        {
            //ChnIsOn = (chip->RegData[0x15] >> CurChn) & 0x01;
            //return true;
            chip->RegFirst[Register] = 0x01;
        }
    }
    else if(Register == 0x15)
    {
        // Channel Enable
        if(Data & 0x10)      // DPCM Enable?
            chip->RegFirst[Register] = 0x01;    // *always* have to rewrite this
    }
    else if(Register == 0x17)   // Frame Counter
    {
        return true;    // writing to this affects envelopes
    }
    else if(Register >= 0x20 && Register < 0x40)
    {
        switch(0x60 + Register)
        {
        case 0x80:  // Volume Envelope (resets Envelope Timer)
        case 0x83:  // Frequency High/Enable (resets Timers)
        case 0x84:  // Modulation Envelope (resets Envelope Timer)
        case 0x85:  // Modulation Position
        case 0x87:  // Modulation Frequency High/Enable (resets Phase)
        case 0x88:  // Modulation Table Write (TODO: can be optimized?)
        case 0x8A:  // Envelope Speed (resets Envelope Timer)
            return true;
        }
    }

    if(! chip->RegFirst[Register] && Data == chip->RegData[Register])
        return false;

    //if (ChnIsOn)
    //  chip->RegFirst[Register] = 0x01;    // Channel Initialize
    //else
    chip->RegFirst[Register] = JustTimerCmds;
    chip->RegData[Register] = Data;

    return true;
}

bool c140_write(UINT8 Port, UINT8 Register, UINT8 Data)
{
    C140_DATA *chip = &ChDat->C140;
    UINT16 RegVal;

    if(Port == 0xFF)
    {
        chip->banking_type = Data;
        return true;
    }

    RegVal = (Port << 8) | Register;
    RegVal &= 0x1FF;

    // mirror the bank registers on the 219, fixes bkrtmaq
    if((RegVal >= 0x1F8) && (chip->banking_type == C140_TYPE_ASIC219))
        RegVal -= 0x008;

    if(! chip->RegFirst[RegVal] && Data == chip->RegData[RegVal])
        return false;

    if(RegVal < 0x180 && (RegVal & 0x0F) == 0x05 && (Data & 0x80))
        chip->RegFirst[RegVal] = 0x01;
    else
        chip->RegFirst[RegVal] = JustTimerCmds;
    chip->RegData[RegVal] = Data;

    return true;
}

bool qsound_write(UINT8 Offset, UINT16 Value)
{
    QSOUND_DATA *chip = &ChDat->QSound;

    if(Offset < 0x80)   // PCM channels
    {
        switch(Offset & 0x07)
        {
        case 0x01:  // Current Address (changed while playing - never ever optimize)
            chip->RegFirst[Offset] = 0x01;
            break;
        case 0x03:  // Current Address, 16 fractional bits
            chip->RegFirst[Offset] = 0x01;
            break;
        default:
            // no special treatment required
            break;
        }
    }
    else if(Offset >= 0xCA && Offset < 0xD6)    // ADPCM channels
    {
        // no special treatment required
    }
    else if(Offset >= 0xD6 && Offset < 0xD9)    // ADPCM trigger
    {
        // writing a non-zero value starts ADPCM playback
        if(Value != 0)
            chip->RegFirst[Offset] = 0x01;
    }
    else if(Offset == 0xE2)     // recalculate delays
        chip->RegFirst[Offset] = 0x01;
    else if(Offset == 0xE3)     // set update routine
    {
        // This may or may not result in a chip reset
        // so I'll just reset the state.
        memset(chip->RegFirst, 0xFF, 0x100);
    }

    if(! chip->RegFirst[Offset] && Value == chip->RegData[Offset])
        return false;

    chip->RegFirst[Offset] = JustTimerCmds;
    chip->RegData[Offset] = Value;

    return true;
}

bool pokey_write(UINT8 Register, UINT8 Data)
{
    POKEY_DATA *chip = &ChDat->Pokey;

    Register &= 0x0F;
    switch(Register)
    {
    case 0x09:  // STIMER_C
    case 0x0A:  // SKREST_C
    case 0x0B:  // POTGO_C
    case 0x0D:  // SEROUT_C
    case 0x0E:  // IRQEN_C
    case 0x0F:  // SKCTL_C
        return false;   // not sure - maybe a break is needed for SKCTL_C
    }

    if(! chip->RegFirst[Register] && Data == chip->RegData[Register])
        return false;

    chip->RegFirst[Register] = JustTimerCmds;
    chip->RegData[Register] = Data;

    return true;
}

bool c6280_write(UINT8 Register, UINT8 Data)
{
    C6280_DATA *chip = &ChDat->C6280;
    C6280_CHANNEL *chan;
    UINT8 ChnReg;

    Register &= 0x0F;
    switch(Register)
    {
    case 0x00:  // Channel Select
        if(! chip->RegFirst[C6280_CHN_SEL] && Data == chip->RegData[C6280_CHN_SEL])
            return false;

        //if (! chip->RegFirst[C6280_CHN_SEL])
        {
            // additional test for 2 Channel Select-Commands after each other
            // that makes first one useless, of course :)
            ChnReg = 0x00;
            while(GetNextChipCommand())
            {
                if(NxtCmdReg == 0x00)
                    return false;
                else if(NxtCmdReg >= 0x02 && NxtCmdReg <= 0x07)
                {
                    ChnReg = 0x01;
                    break;
                }
            }
            if(! ChnReg)
            {
                // A Channel Select at the very end of the VGM is omitted when:
                //  1. The VGM doesn't loop.
                //  2. It's the first ChnSel and no channel command came before.
                //  3. At the loop point, there's a ChnSel before any channel commands.
                if(! VGM_Loops || (chip->RegFirst[C6280_CHN_LOOP] ||
                                   chip->RegData[C6280_CHN_LOOP] == 0x80))
                {
                    // when no command follows the Channel Select one, it's useless too
                    return false;
                }
            }
        }

        chip->RegFirst[C6280_CHN_SEL] = JustTimerCmds;
        chip->RegData[C6280_CHN_SEL] = Data;
        if(chip->RegFirst[C6280_CHN_LOOP])
        {
            // is the Channel Select the first command after the loop?
            chip->RegFirst[C6280_CHN_LOOP] = 0x00;
            chip->RegData[C6280_CHN_LOOP] = 0x80;   // set to 'ignore'
        }

        break;
    case 0x01:  // Global Balance
    case 0x08:  // LFO Frequency
    case 0x09:  // LFO Control (Enable, Mode)
        switch(Register)
        {
        case 0x01:
            ChnReg = C6280_BALANCE;
            break;
        case 0x08:
            ChnReg = C6280_LFO_FRQ;
            break;
        case 0x09:
            ChnReg = C6280_LFO_CTRL;
            break;
        }
        if(! chip->RegFirst[ChnReg] && Data == chip->RegData[ChnReg])
            return false;

        chip->RegFirst[ChnReg] = JustTimerCmds;
        chip->RegData[ChnReg] = Data;
        break;
    case 0x02:  // Channel Frequency (LSB)
    case 0x03:  // Channel frequency (MSB)
    case 0x04:  // Channel Control (KeyOn, DDA Mode, Volume)
    case 0x05:  // Channel Balance
    case 0x06:  // Channel Waveform Data
    case 0x07:  // Noise Control (Enable, Frequency)
        chan = &chip->chan[chip->RegData[C6280_CHN_SEL] & 0x07];
        ChnReg = Register - 0x02;

        if(chip->RegFirst[C6280_CHN_LOOP])
            chip->RegFirst[C6280_CHN_LOOP] = 0x00;

        if(Register == 0x06)
        {
            // increment Wave Index
            return true;
        }

        if(! chan->RegFirst[ChnReg] && Data == chan->ChnReg[ChnReg])
            return false;

        if(Register == 0x04)
        {
            if((chan->ChnReg[ChnReg] & 0x40) && !(Data & 0x40))
            {
                // reset Wave Index
            }
        }

        chan->RegFirst[ChnReg] = JustTimerCmds;
        chan->ChnReg[ChnReg] = Data;
        break;
    }

    return true;
}

static bool fmadpcm_write(UINT8 Register, UINT8 Data, UINT8 *RegData, UINT8 *RegFirst)
{
    UINT8 CurChn;
    UINT8 TempByt;

    switch(Register)
    {
    case 0x00:  // DM,--,C5,C4,C3,C2,C1,C0
        if(!(Data & 0x3F))      // none of the channel bits set
            return JustTimerCmds;

        if(!(Data & 0x80))
        {
            // Key On
            for(CurChn = 0; CurChn < 6; CurChn ++)
            {
                if((Data >> CurChn) & 0x01)
                {
                    // Sounds are restarted in any case
                    RegData[Register] |= (1 << CurChn);
                    RegFirst[Register] |= (1 << CurChn);
                }
            }

            return true;
        }
        else
        {
            // Key Off
            TempByt = RegData[Register];
            for(CurChn = 0; CurChn < 6; CurChn ++)
            {
                if((Data >> CurChn) & 0x01)
                {
                    TempByt &= ~(1 << CurChn);
                    if(RegFirst[Register] & (1 << CurChn))
                    {
                        if(! JustTimerCmds)
                            RegFirst[Register] &= ~(1 << CurChn);
                        TempByt |= 0x80;
                    }
                }
            }

            if(TempByt == RegData[Register])
                return false;

            TempByt &= 0x3F;
            RegData[Register] = TempByt;
            /*RegFirst[Register] &= 0x3F;
            RegFirst[Register] |= JustTimerCmds * 0x3F;*/
        }
        break;
    case 0x01:  // B0-5 = Total Level
        Data &= 0x3F;

        if(! RegFirst[Register] && Data == RegData[Register])
            return false;

        RegData[Register] = Data;
        RegFirst[Register] = JustTimerCmds;
        break;
    default:
        //CurChn = Register & 0x07;
        /*switch(Register & 0x38)
        {
        case 0x08:  // B7 = Left, B6 = Right, B4-0 = Channel Volume
        case 0x10:  // Start Address L
        case 0x18:  // Start Address H
        case 0x20:  // End Address L
        case 0x28:  // End Address H
        default:    // unused*/
        if(! RegFirst[Register] && Data == RegData[Register])
            return false;

        RegData[Register] = Data;
        RegFirst[Register] = JustTimerCmds;
        /*  break;
        }*/
        break;
    }

    return true;
}

bool k054539_write(UINT8 Port, UINT8 Register, UINT8 Data)
{
    K054539_DATA *chip = &ChDat->K054539;
    UINT16 RegVal;
    UINT8 TempByt;
    //UINT8 latch;

    RegVal = (Port << 8) | Register;
    if(RegVal >= 0x230)
        return true;

    //latch = (chip->k054539_flags & K054539_UPDATE_AT_KEYON) && (chip->RegData[0x22F] & 0x01);

    switch(RegVal)
    {
    case 0x214:
    case 0x215:
        return true;
    // The Key On/Off Register gets modifed during playback, so this doesn't work.
    /*case 0x214:   // Key On
        if (chip->RegData[0x22F] & 0x80)
            return true;

        if (! chip->RegFirst[RegVal])
            return false;
        TempByt = chip->RegData[0x22C] | Data;
        if (TempByt == chip->RegData[0x022C])
            return false;

        chip->RegData[0x22C] = TempByt;
        //chip->RegFirst[RegVal] = JustTimerCmds;
        //chip->RegData[RegVal] = Data;
        break;
    case 0x215: // Key Off
        if (chip->RegData[0x22F] & 0x80)
            return true;

        if (! chip->RegFirst[RegVal])
            return false;
        TempByt = chip->RegData[0x22C] & ~Data;
        if (TempByt == chip->RegData[0x022C])
            return false;

        chip->RegData[0x22C] = TempByt;
        //chip->RegFirst[RegVal] = JustTimerCmds;
        //chip->RegData[RegVal] = Data;
        break;*/
    case 0x22D: // RAM Pointer Advance
        return true;
    /*case 0x22E:   // Set ROM Bank (can use default handler)
        info->cur_zone =    data == 0x80 ? info->ram :
                            info->rom + 0x20000*data;
        info->cur_limit = data == 0x80 ? 0x4000 : 0x20000;
        info->cur_ptr = 0;
        break;*/
    default:
        if(RegVal < 0x100)
        {
            TempByt = (Register & 0x1F);
            if(TempByt >= 0x0C && TempByt <= 0x0E)
            {
                // latch writes to the position index registers
                //info->k054539_posreg_latch[ch][offs] = data;
                return true;
            }
        }
        if(! chip->RegFirst[RegVal] && Data == chip->RegData[RegVal])
            return false;

        //chip->RegFirst[RegVal] = JustTimerCmds;
        //chip->RegData[RegVal] = Data;
        break;
    }
    chip->RegFirst[RegVal] = JustTimerCmds;
    chip->RegData[RegVal] = Data;

    return true;
}

bool k051649_write(UINT8 Port, UINT8 Register, UINT8 Data)
{
    K051649_DATA *chip = &ChDat->K051649;
    UINT8 *DataFirst;
    UINT8 *DataPtr;

    switch(Port)
    {
    case 0x00:  // k051649_waveform_w
        // Channel Mask: 0xE0, Waveform Mask: 0x1F
        /*if ((Register >> 5) >= 4)
            return true;
        DataFirst = &chip->WaveFirst[Register];
        DataPtr = &chip->WaveData[Register];
        break;*/
        return true;
    case 0x01:  // k051649_frequency_w
        // Channel Mask: 0x0E, Frequency MSB/LSB Mask: 0x01 (0 - MSB, 1 - LSB)
        if((Register >> 1) >= 5)
            return true;
        // !!! Important Note: Frequency Writes actually reset the Freq Counter !!!
        // (so maybe it would be a good idea to not remove it)
        DataFirst = &chip->FreqFirst[Register];
        DataPtr = &chip->FreqData[Register];
        break;
    case 0x02:  // k051649_volume_w
        // Channel Mask: 0xE0, Waveform Mask: 0x1F
        if((Register & 0x07) >= 5)
            return true;
        DataFirst = &chip->VolFirst[Register];
        DataPtr = &chip->VolData[Register];
        break;
    case 0x03:  // k051649_keyonoff_w
        DataFirst = &chip->KeyFirst;
        DataPtr = &chip->KeyOn;
        break;
    case 0x04:  // k052539_waveform_w
        // Channel Mask: 0xE0, Waveform Mask: 0x1F
        /*if ((Register >> 5) >= 5)
            return true;
        DataFirst = &chip->WaveFirst[Register];
        DataPtr = &chip->WaveData[Register];
        break;*/
        return true;
    case 0x05:  // k051649_test_w
        return true;
    default:
        return true;
    }

    if(! *DataFirst && Data == *DataPtr)
        return false;

    *DataFirst = JustTimerCmds;
    *DataPtr = Data;

    return true;
}

bool okim6295_write(UINT8 Port, UINT8 Data)
{
    OKIM6295_DATA *chip = &ChDat->OKIM6295;
    //UINT8 CurChn;

    if(Port & 0x80)
    {
        chip->RegData[Port & 0x7F] = Data;
        chip->RegFirst[Port & 0x7F] = 0x00;
        return true;
    }

    switch(Port)
    {
    case 0x00:  // okim6295_write_command
        return true;
    // possible TODO: remove redundant STOP commands
    /*if (CacheOKI6295[ChpCur].Command != 0xFF)
    {
        // start channel
        for (CurChn = 0x00; CurChn < 0x04; CurChn ++)
        {
            if (Data & (0x10 << CurChn))
                ChnEnable[CurChn] = 0x01;
        }

        CacheOKI6295[ChpCur].Command = 0xFF;
    }
    else if (Data & 0x80)
    {
        // play sample on channels
        CacheOKI6295[ChpCur].Command = Data & 0x7F;
    }
    else
    {
        // stop channel
        for (CurChn = 0x00; CurChn < 0x04; CurChn ++)
        {
            if (Data & (0x08 << CurChn))
                ChnEn[CurChn] = 0x00;
        }
    }
    break;*/
    case 0x08:  // Master Clock 000000dd
    case 0x09:  // Master Clock 0000dd00
    case 0x0A:  // Master Clock 00dd0000
        if(! chip->RegFirst[Port] && Data == chip->RegData[Port])
            return false;

        chip->RegData[Port] = Data;
        chip->RegFirst[Port] = JustTimerCmds;
        chip->RegFirst[0x0B] = 0x01;    // force Clock rewrite
        return true;
    case 0x0B:  // Master Clock dd000000
        Data &= 0x7F;   // fix a bug in MAME VGM logs
    case 0x0C:  // Clock Divider
    case 0x0E:  // NMK112 Bank Enable
    case 0x0F:  // Set Bank
    case 0x10:  // Set NMK Bank 0
    case 0x11:  // Set NMK Bank 1
    case 0x12:  // Set NMK Bank 2
    case 0x13:  // Set NMK Bank 3
        break;
    }

    //if (Port >= 0x14)
    //  return true;

    if(! chip->RegFirst[Port] && Data == chip->RegData[Port])
        return false;

    chip->RegData[Port] = Data;
    chip->RegFirst[Port] = JustTimerCmds;

    return true;
}

bool scsp_write(UINT8 Port, UINT8 Register, UINT8 /*Data*/)
{
    //SCSP_DATA* chip = &ChDat->SCSP;

    if(Port == 0x04 && (Register >= 0x1A && Register <= 0x29))
        return false;
    if(Port == 0x04 && Register == 0x08)
        return false;   // TODO: mainly used for reading?

    return true;
}

bool upd7759_write(UINT8 Port, UINT8 Data)
{
    UPD7759_DATA *chip = &ChDat->UPD7759;

    if(Port == 0x02)
        return true;    // write FIFO
    else if(Port >= 0x04)
        return true;    // unknown write

    if(! chip->RegFirst[Port] && Data == chip->RegData[Port])
        return false;

    chip->RegData[Port] = Data;
    chip->RegFirst[Port] = JustTimerCmds;
    return true;
}

bool okim6258_write(UINT8 Port, UINT8 Data)
{
    OKIM6258_DATA *chip = &ChDat->OKIM6258;
    bool RetVal;

    if(Port & 0x80)
    {
        chip->RegData[Port & 0x0F] = Data;
        chip->RegFirst[Port & 0x0F] = 0x00;
        return true;
    }

    switch(Port)
    {
    case 0x00:  // Start/Stop
    case 0x01:  // Data
        return true;
    case 0x02:  // Pan
        if(! DoOKI6258)     // if (pre opt_oki)
            return true;
        if(! chip->RegFirst[Port] && Data == chip->RegData[Port])
            return false;

        chip->RegData[Port] = Data;
        chip->RegFirst[Port] = JustTimerCmds;
        break;
    case 0x08:  // Master Clock 000000dd
    case 0x09:  // Master Clock 0000dd00
    case 0x0A:  // Master Clock 00dd0000
        if(/*! chip->RegFirst[Port] &&*/ Data == chip->RegData[Port])
            return false;

        chip->RegData[Port] = Data;
        chip->RegFirst[Port] = 0x00;
        chip->RegFirst[0x0B] = 0x01;    // force Clock rewrite
        break;
    case 0x0B:  // Master Clock dd000000
        if(! chip->RegFirst[Port] && Data == chip->RegData[Port])
            return false;

        chip->RegData[Port] = Data;
        chip->RegFirst[Port] = 0x00;
        printf("Master Clock Change!\n");
        getchar();
        break;
    case 0x0C:  // Clock Divider
        if(! chip->RegFirst[Port] && Data == chip->RegData[Port])
            return false;

        if(Port == 0x0C && Data == chip->RegData[Port])
        {
            do
            {
                RetVal = GetNextChipCommand();
            }
            while(RetVal && NxtCmdReg != 0x0C);
            if(! RetVal)
                return false;   // It's the only Clock Divider change til EOF and it's the same as pre-loop.
        }
        chip->RegData[Port] = Data;
        chip->RegFirst[Port] = 0x00;
        //printf("Clock Divider Change!\n");
        //getchar();
        break;
    }

    return true;
}

bool c352_write(UINT16 offset, UINT16 val)
{
    C352_DATA *chip = &ChDat->C352;
    UINT16 ChnBase;

    if(offset >= 0x208)
        return true;

    ChnBase = offset & 0x0F8;
    if(offset < 0x100 && (chip->RegData[ChnBase | 3] & C352_FLG_LINKLOOP) == C352_FLG_LINKLOOP)
    {
        switch(offset & 7)
        {
        case 3: // flags
            if((val & C352_FLG_LINKLOOP) != C352_FLG_LINKLOOP)
            {
                chip->RegFirst[ChnBase | 4] = 1;    // Force rewrites of address registers
                chip->RegFirst[ChnBase | 5] = 1;    // after end of a linked sample.
                chip->RegFirst[ChnBase | 6] = 1;
                chip->RegFirst[ChnBase | 7] = 1;
            }
            break;
        case 4: // bank addr
        case 5: // start addr
            //case 6:   // end addr
            //case 7:   // loop addr
            chip->RegFirst[offset] = 0x01;  // leave these registers alone
            break;
        default:
            break;
        }
    }

    if(! chip->RegFirst[offset] && val == chip->RegData[offset])
        return false;

    chip->RegData[offset] = val;
    chip->RegFirst[offset] = JustTimerCmds;
    if(offset < 0x100)
        chip->RegFirst[0x202] = 0x01;   // enforce rewrite of Refresh register
    return true;
}

bool x1_010_write(UINT16 offset, UINT8 val)
{
    X1_010_DATA *chip = &ChDat->X1_010;

    // Key on without loop flag set: chip will clear the key on flag once playback is finished
    if(offset < 0x80 && (offset & 0x07) == 0x00 && val & 0x01 && !(val & 0x04))
        chip->RegFirst[offset] = 1;

    if(offset >= 0x2000)
        return false;

    if(! chip->RegFirst[offset] && val == chip->RegData[offset])
        return false;

    chip->RegData[offset] = val;
    chip->RegFirst[offset] = JustTimerCmds;
    return true;
}

bool es5503_write(UINT8 Register, UINT8 Data)
{
    ES5503_DATA *chip = &ChDat->ES5503;

    if(Register >= 0xE2)
        return true;
    if((Register & 0xE0) == 0xA0)
        return true;    // don't strip Control register (can be changed by sound chip itself)

    if(! chip->RegFirst[Register] && Data == chip->RegData[Register])
        return false;

    chip->RegFirst[Register] = JustTimerCmds;
    chip->RegData[Register] = Data;
    return true;
}

bool vsu_write(UINT16 Register, UINT8 Data)
{
    VSU_DATA *chip = &ChDat->VSU;
    UINT8 CurChn;
    UINT16 ChnBaseReg;

    if(Register == 0x160)
    {
        // All Channels Off register
        for(CurChn = 0; CurChn < 6; CurChn ++)
            chip->RegFirst[CurChn] = 0x01;
        return true;
    }
    else if(Register >= 0x160)
        return true;
    else if(Register >= 0x100)
    {
        CurChn = (Register >> 4) & 0x07;
        ChnBaseReg = Register & ~0x00F;
        switch(Register & 0x0F)
        {
        case 0x00:  // Mode
            if(Data & 0x80)     // Key On
                chip->RegFirst[Register] = 0x01;
            break;
        case 0x01:  // Volume
            break;
        case 0x02:  // Frequency LSB
        case 0x03:  // Frequency MSB
            if(chip->RegData[ChnBaseReg | 0x05] & 0x40)     // Sweeo On
                chip->RegFirst[Register] = 0x01;
            break;
        case 0x04:  // Envelope
            if(chip->RegData[ChnBaseReg | 0x05] & 0x01)     // Volume Envelope On
                chip->RegFirst[Register] = 0x01;
            break;
        case 0x05:  // Envelope Control
            if(Data & 0x01)     // Volume Envelope On
            {
                chip->RegFirst[ChnBaseReg | 0x04] = 0x01;
                chip->RegFirst[Register] = 0x01;
            }
            if(Data & 0x40)     // Sweep On
            {
                chip->RegFirst[ChnBaseReg | 0x02] = 0x01;
                chip->RegFirst[ChnBaseReg | 0x03] = 0x01;
                chip->RegFirst[Register] = 0x01;
            }
            break;
        }
    }

    if(! chip->RegFirst[Register] && Data == chip->RegData[Register])
        return false;

    chip->RegFirst[Register] = JustTimerCmds;
    chip->RegData[Register] = Data;
    return true;
}


} // namespace VgmCMP
