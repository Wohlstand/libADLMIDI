// vgm_cmp.c - VGM Compressor
//
// TODO: fix ResetAllChips

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "vgm_stdtype.h"
#include "stdbool.h"
#include "VGMFile.h"
#include "vgm_lib.h"
#include "common.h"

#include "vgm_cmp.h"

namespace VgmCMP
{

static bool OpenVGMFile(const char *FileName);
static void WriteVGMFile(const char *FileName, bool makeVgz);
static void CompressVGMData(void);
bool GetNextChipCommand(void);

// Function Prototypes from chip_cmp.c
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
bool ay8910_write_reg(UINT8 Register, UINT8 Data);
bool ymf271_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool gameboy_write_reg(UINT8 Register, UINT8 Data);
bool nes_psg_write(UINT8 Register, UINT8 Data);
bool c140_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool qsound_write(UINT8 Offset, UINT16 Value);
bool pokey_write(UINT8 Register, UINT8 Data);
bool c6280_write(UINT8 Register, UINT8 Data);
bool k054539_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool k051649_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool scsp_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool okim6295_write(UINT8 Port, UINT8 Data);
bool upd7759_write(UINT8 Port, UINT8 Data);
bool okim6258_write(UINT8 Port, UINT8 Data);
bool c352_write(UINT16 Register, UINT16 Data);
bool x1_010_write(UINT16 Offset, UINT8 Value);
bool es5503_write(UINT8 Register, UINT8 Data);
bool vsu_write(UINT16 Register, UINT8 Data);

static VGM_HEADER VGMHead;
static UINT32 VGMDataLen;
static UINT8 *VGMData;
static UINT32 VGMPos;
static INT32 VGMSmplPos;
static UINT8 *DstData;
static UINT32 DstDataLen;
static char FileBase[0x100];
static UINT32 DataSizeA;
static UINT32 DataSizeB;

UINT32 NxtCmdPos;
UINT8 NxtCmdCommand;
UINT16 NxtCmdReg;
UINT8 NxtCmdVal;

bool JustTimerCmds;
bool DoOKI6258;

int vgm_cmp_main(const std::string &in_file, bool makeVgz, bool justtmr, bool do6258)
{
//    int argbase;
    int ErrVal;
    const char *FileName = in_file.c_str();
    UINT32 SrcDataSize = 0;
    UINT16 PassNo;

    printf("\nCompacting VGM file...");
    fflush(stdout);

    ErrVal = 0;
    JustTimerCmds = justtmr;
    DoOKI6258 = do6258;

//    argbase = 1;
//    while(argbase < argc && argv[argbase][0] == '-')
//    {
//        if(!stricmp(argv[argbase], "-justtmr"))
//        {
//            JustTimerCmds = true;
//            argbase ++;
//        }
//        else if(!stricmp(argv[argbase], "-do6258"))
//        {
//            DoOKI6258 = true;
//            argbase ++;
//        }
//        else
//            break;
//    }

//    printf("File Name:\t");
//    if(argc <= argbase + 0)
//    {
////        ReadFilename(FileName, sizeof(FileName));
//        return 1;
//    }
//    else
//    {
//        strcpy(FileName, argv[argbase + 0]);
//        printf("%s\n", FileName);
//    }

//    if(!strlen(FileName))
//        return 0;

    if(!OpenVGMFile(FileName))
    {
        printf("Error opening the file!\n");
        fflush(stdout);
        ErrVal = 1;
        goto EndProgram;
    }
    printf("\n");

    PassNo = 0x00;
    do
    {
        printf("Pass #%u ...\n", PassNo + 1);
        fflush(stdout);
        CompressVGMData();
        if(! PassNo)
            SrcDataSize = DataSizeA;
        printf("    Data Compression: %u -> %u (%.1f %%)\n",
               DataSizeA, DataSizeB, 100.0 * DataSizeB / (float)DataSizeA);
        fflush(stdout);
        if(DataSizeB < DataSizeA)
        {
            free(VGMData);
            VGMDataLen = DstDataLen;
            VGMData = DstData;
            DstDataLen = 0x00;
            DstData = NULL;
        }
        PassNo++;
    }
    while(DataSizeB < DataSizeA);
    printf("Data Compression Total: %u -> %u (%.1f %%)\n",
           SrcDataSize, DataSizeB, 100.0 * DataSizeB / (float)SrcDataSize);
    fflush(stdout);

    if(DataSizeB < SrcDataSize)
    {
//        if(argc > argbase + 1)
//            strcpy(FileName, argv[argbase + 1]);
//        else
//            strcpy(FileName, "");
//        if(FileName[0] == '\0')
//        {
//            strcpy(FileName, FileBase);
//            strcat(FileName, "_optimized.vgm");
//        }
        WriteVGMFile(FileName, makeVgz);
    }

    free(VGMData);
    free(DstData);

EndProgram:
//    DblClickWait(argv[0]);

    printf("\n");
    fflush(stdout);

    return ErrVal;
}

static bool OpenVGMFile(const char *FileName)
{
    gzFile hFile;
    UINT32 CurPos;
    UINT32 TempLng;
    char *TempPnt;

    hFile = gzopen(FileName, "rb");
    if(hFile == NULL)
        return false;

    gzseek(hFile, 0x00, SEEK_SET);
    gzread(hFile, &TempLng, 0x04);
    if(TempLng != FCC_VGM)
        goto OpenErr;

    gzseek(hFile, 0x00, SEEK_SET);
    gzread(hFile, &VGMHead, sizeof(VGM_HEADER));
    ZLIB_SEEKBUG_CHECK(VGMHead)

    // Header preperations
    if(VGMHead.lngVersion < 0x00000101)
        VGMHead.lngRate = 0;
    if(VGMHead.lngVersion < 0x00000110)
    {
        VGMHead.shtPSG_Feedback = 0x0000;
        VGMHead.bytPSG_SRWidth = 0x00;
        VGMHead.lngHzYM2612 = VGMHead.lngHzYM2413;
        VGMHead.lngHzYM2151 = VGMHead.lngHzYM2413;
    }
    if(VGMHead.lngVersion < 0x00000150)
        VGMHead.lngDataOffset = 0x00000000;
    if(VGMHead.lngVersion < 0x00000151)
    {
        VGMHead.lngHzSPCM = 0x0000;
        VGMHead.lngSPCMIntf = 0x00000000;
        // all others are zeroed by memset
    }
    // relative -> absolute addresses
    VGMHead.lngEOFOffset += 0x00000004;
    if(VGMHead.lngGD3Offset)
        VGMHead.lngGD3Offset += 0x00000014;
    if(VGMHead.lngLoopOffset)
        VGMHead.lngLoopOffset += 0x0000001C;
    if(! VGMHead.lngDataOffset)
        VGMHead.lngDataOffset = 0x0000000C;
    VGMHead.lngDataOffset += 0x00000034;

    CurPos = VGMHead.lngDataOffset;
    if(VGMHead.lngVersion < 0x00000150)
        CurPos = 0x40;
    TempLng = sizeof(VGM_HEADER);
    if(TempLng > CurPos)
        TempLng -= CurPos;
    else
        TempLng = 0x00;
    memset((UINT8 *)&VGMHead + CurPos, 0x00, TempLng);

    // Read Data
    VGMDataLen = VGMHead.lngEOFOffset;
    VGMData = (UINT8 *)malloc(VGMDataLen);
    if(VGMData == NULL)
        goto OpenErr;
    gzseek(hFile, 0x00, SEEK_SET);
    gzread(hFile, VGMData, VGMDataLen);

    gzclose(hFile);

    strcpy(FileBase, FileName);
    TempPnt = strrchr(FileBase, '.');
    if(TempPnt != NULL)
        *TempPnt = 0x00;

    return true;

OpenErr:

    gzclose(hFile);
    return false;
}

static void WriteVGMFile(const char *FileName, bool makeVgz)
{
    if(makeVgz)
    {
        gzFile hFile;
        hFile = gzopen(FileName, "wb");
        gzwrite(hFile, DstData, DstDataLen);
        gzclose(hFile);
    }
    else
    {
        FILE *hFile;
        hFile = fopen(FileName, "wb");
        fwrite(DstData, 0x01, DstDataLen, hFile);
        fclose(hFile);
    }

    printf("File written.\n");
    fflush(stdout);

    return;
}

static void CompressVGMData(void)
{
    UINT32 DstPos;
    UINT8 ChipID;
    UINT8 Command;
    UINT32 CmdDelay;
    UINT32 AllDelay;
    UINT8 TempByt;
    UINT16 TempSht;
    UINT32 TempLng;
    //UINT32 DataStart;
    //UINT32 DataLen;
#ifdef WIN32
    UINT32 ROMSize;
    UINT32 CmdTimer;
    char TempStr[0x80];
    char MinSecStr[0x80];
#endif
    UINT32 CmdLen;
    bool StopVGM;
    bool WriteEvent;
    UINT32 NewLoopS;
    bool WroteCmd80;
    const UINT8 *VGMPnt;

    DstData = (UINT8 *)malloc(VGMDataLen + 0x100);
    AllDelay = 0;
    VGMPos = VGMHead.lngDataOffset;
    DstPos = VGMHead.lngDataOffset;
    VGMSmplPos = 0;
    NewLoopS = 0x00;
    memcpy(DstData, VGMData, VGMPos);   // Copy Header

#ifdef WIN32
    CmdTimer = 0;
#endif
    InitAllChips();
    if(VGMHead.lngHzOKIM6258)
    {
        SetChipSet(0x00);
        okim6258_write(0x88, (VGMHead.lngHzOKIM6258 >>  0) & 0xFF);
        okim6258_write(0x89, (VGMHead.lngHzOKIM6258 >>  8) & 0xFF);
        okim6258_write(0x8A, (VGMHead.lngHzOKIM6258 >> 16) & 0xFF);
        okim6258_write(0x8B, (VGMHead.lngHzOKIM6258 >> 24) & 0xFF);
        okim6258_write(0x8C, VGMHead.bytOKI6258Flags & 0x03);
    }
    if(VGMHead.lngHzOKIM6295)
    {
        TempLng = VGMHead.lngHzOKIM6295 & 0x3FFFFFFF;

        SetChipSet(0x00);
        okim6295_write(0x88, (TempLng >>  0) & 0xFF);
        okim6295_write(0x89, (TempLng >>  8) & 0xFF);
        okim6295_write(0x8A, (TempLng >> 16) & 0xFF);
        okim6295_write(0x8B, (TempLng >> 24) & 0xFF);
        okim6295_write(0x8C, VGMHead.lngHzOKIM6295 >> 31);
        if(VGMHead.lngHzOKIM6295 & 0x40000000)
        {
            SetChipSet(0x01);
            okim6295_write(0x88, (TempLng >>  0) & 0xFF);
            okim6295_write(0x89, (TempLng >>  8) & 0xFF);
            okim6295_write(0x8A, (TempLng >> 16) & 0xFF);
            okim6295_write(0x8B, (TempLng >> 24) & 0xFF);
            okim6295_write(0x8C, VGMHead.lngHzOKIM6295 >> 31);
        }
    }
    /*if (VGMHead.lngHzK054539)
    {
        SetChipSet(0x00);
        k054539_write(0xFF, 0x00, VGMHead.bytK054539Flags);
        if (VGMHead.lngHzK054539 & 0x40000000)
        {
            SetChipSet(0x01);
            k054539_write(0xFF, 0x00, VGMHead.bytK054539Flags);
        }
    }*/
    if(VGMHead.lngHzC140)
    {
        SetChipSet(0x00);
        c140_write(0xFF, 0x00, VGMHead.bytC140Type);
        if(VGMHead.lngHzC140 & 0x40000000)
        {
            SetChipSet(0x01);
            c140_write(0xFF, 0x00, VGMHead.bytC140Type);
        }
    }

    StopVGM = false;
    WroteCmd80 = false;
    while(VGMPos < VGMHead.lngEOFOffset)
    {
        if(VGMPos == VGMHead.lngLoopOffset)
            ResetAllChips();    // Force resend of all commands after loopback
        CmdDelay = 0;
        CmdLen = 0x00;
        Command = VGMData[VGMPos + 0x00];
        WriteEvent = true;

        if(Command >= 0x70 && Command <= 0x8F)
        {
            switch(Command & 0xF0)
            {
            case 0x70:
                TempSht = (Command & 0x0F) + 0x01;
                VGMSmplPos += TempSht;
                CmdDelay = TempSht;
                WriteEvent = false;
                break;
            case 0x80:
                // Handling is done at WriteEvent
                //WriteEvent = true;    // I still need to write it
                break;
            }
            CmdLen = 0x01;
        }
        else
        {
            VGMPnt = &VGMData[VGMPos];

            // Cheat Mode (to use 2 instances of 1 chip)
            ChipID = 0x00;
            switch(Command)
            {
            case 0x30:
                if(VGMHead.lngHzPSG & 0x40000000)
                {
                    Command += 0x20;
                    ChipID = 0x01;
                }
                break;
            case 0x3F:
                if(VGMHead.lngHzPSG & 0x40000000)
                {
                    Command += 0x10;
                    ChipID = 0x01;
                }
                break;
            case 0xA1:
                if(VGMHead.lngHzYM2413 & 0x40000000)
                {
                    Command -= 0x50;
                    ChipID = 0x01;
                }
                break;
            case 0xA2:
            case 0xA3:
                if(VGMHead.lngHzYM2612 & 0x40000000)
                {
                    Command -= 0x50;
                    ChipID = 0x01;
                }
                break;
            case 0xA4:
                if(VGMHead.lngHzYM2151 & 0x40000000)
                {
                    Command -= 0x50;
                    ChipID = 0x01;
                }
                break;
            case 0xA5:
                if(VGMHead.lngHzYM2203 & 0x40000000)
                {
                    Command -= 0x50;
                    ChipID = 0x01;
                }
                break;
            case 0xA6:
            case 0xA7:
                if(VGMHead.lngHzYM2608 & 0x40000000)
                {
                    Command -= 0x50;
                    ChipID = 0x01;
                }
                break;
            case 0xA8:
            case 0xA9:
                if(VGMHead.lngHzYM2610 & 0x40000000)
                {
                    Command -= 0x50;
                    ChipID = 0x01;
                }
                break;
            case 0xAA:
                if(VGMHead.lngHzYM3812 & 0x40000000)
                {
                    Command -= 0x50;
                    ChipID = 0x01;
                }
                break;
            case 0xAB:
                if(VGMHead.lngHzYM3526 & 0x40000000)
                {
                    Command -= 0x50;
                    ChipID = 0x01;
                }
                break;
            case 0xAC:
                if(VGMHead.lngHzY8950 & 0x40000000)
                {
                    Command -= 0x50;
                    ChipID = 0x01;
                }
                break;
            case 0xAE:
            case 0xAF:
                if(VGMHead.lngHzYMF262 & 0x40000000)
                {
                    Command -= 0x50;
                    ChipID = 0x01;
                }
                break;
            case 0xAD:
                if(VGMHead.lngHzYMZ280B & 0x40000000)
                {
                    Command -= 0x50;
                    ChipID = 0x01;
                }
                break;
            }
            SetChipSet(ChipID);

            NxtCmdPos = VGMPos;
            NxtCmdCommand = Command;
            switch(Command)
            {
            case 0x66:  // End Of File
                CmdLen = 0x01;
                StopVGM = true;
                break;
            case 0x62:  // 1/60s delay
                TempSht = 735;
                VGMSmplPos += TempSht;
                CmdDelay = TempSht;
                CmdLen = 0x01;
                WriteEvent = false;
                break;
            case 0x63:  // 1/50s delay
                TempSht = 882;
                VGMSmplPos += TempSht;
                CmdDelay = TempSht;
                CmdLen = 0x01;
                WriteEvent = false;
                break;
            case 0x61:  // xx Sample Delay
                memcpy(&TempSht, &VGMPnt[0x01], 0x02);
                VGMSmplPos += TempSht;
                CmdDelay = TempSht;
                CmdLen = 0x03;
                WriteEvent = false;
                break;
            case 0x50:  // SN76496 write
                WriteEvent = sn76496_write(VGMPnt[0x01]);
                CmdLen = 0x02;
                break;
            case 0xBD:  // SAA1099 write
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7); // fallthrough
            case 0x51:  // YM2413 write
                WriteEvent = ym2413_write(VGMPnt[0x01], VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0x52:  // YM2612 write port 0
            case 0x53:  // YM2612 write port 1
                TempByt = Command & 0x01;
                WriteEvent = ym2612_write(TempByt, VGMPnt[0x01], VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0x67:  // PCM Data Stream
                TempByt = VGMPnt[0x02];
                (void)TempByt; // Supres the "never used" warning
                memcpy(&TempLng, &VGMPnt[0x03], 0x04);

                ChipID = (TempLng & 0x80000000) >> 31;
                TempLng &= 0x7FFFFFFF;
                //if (TempByt == 0xC2)
                //  WriteEvent = false;
                /*SetChipSet(ChipID);

                switch(TempByt & 0xC0)
                {
                case 0x00:  // Database Block
                case 0x40:  // Comrpessed Database Block
                    DataLen = TempLng;
                    break;
                case 0x80:  // ROM/RAM Dump
                    memcpy(&ROMSize, &VGMPnt[0x07], 0x04);
                    memcpy(&DataStart, &VGMPnt[0x0B], 0x04);
                    DataLen = TempLng - 0x08;
                    break;
                case 0xC0:  // RAM Write
                    memcpy(&TempSht, &VGMPnt[0x07], 0x02);
                    DataLen = TempLng - 0x02;
                    break;
                }*/

                CmdLen = 0x07 + TempLng;
                break;
            case 0xE0:  // Seek to PCM Data Bank Pos
                memcpy(&TempLng, &VGMPnt[0x01], 0x04);
                CmdLen = 0x05;
                break;
            case 0x4F:  // GG Stereo
                WriteEvent = GGStereo(VGMPnt[0x01]);
                CmdLen = 0x02;
                break;
            case 0x54:  // YM2151 write
                WriteEvent = ym2151_write(VGMPnt[0x01], VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0xC0:  // Sega PCM memory write
                memcpy(&TempSht, &VGMPnt[0x01], 0x02);
                WriteEvent = segapcm_mem_write(TempSht, VGMPnt[0x03]);
                CmdLen = 0x04;
                break;
            case 0xB0:  // RF5C68 register write
                WriteEvent = rf5c68_reg_write(VGMPnt[0x01], VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0xC1:  // RF5C68 memory write
                //memcpy(&TempSht, &VGMPnt[0x01], 0x02);
                //WriteEvent = rf5c68_mem_write(TempSht, VGMPnt[0x03]);
                WriteEvent = true;  // OptVgmRF works a lot better this way
                CmdLen = 0x04;
                break;
            case 0x55:  // YM2203
                WriteEvent = ym2203_write(VGMPnt[0x01], VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0x56:  // YM2608 write port 0
            case 0x57:  // YM2608 write port 1
                TempByt = Command & 0x01;
                WriteEvent = ym2608_write(TempByt, VGMPnt[0x01], VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0x58:  // YM2610 write port 0
            case 0x59:  // YM2610 write port 1
                TempByt = Command & 0x01;
                WriteEvent = ym2610_write(TempByt, VGMPnt[0x01], VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0x5A:  // YM3812 write
                WriteEvent = ym3812_write(VGMPnt[0x01], VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0x5B:  // YM3526 write
                WriteEvent = ym3526_write(VGMPnt[0x01], VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0x5C:  // Y8950 write
                WriteEvent = y8950_write(VGMPnt[0x01], VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0x5E:  // YMF262 write port 0
            case 0x5F:  // YMF262 write port 1
                TempByt = Command & 0x01;
                WriteEvent = ymf262_write(TempByt, VGMPnt[0x01], VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0x5D:  // YMZ280B write
                WriteEvent = ymz280b_write(VGMPnt[0x01], VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0xD0:  // YMF278B write
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                TempByt = VGMPnt[0x01] & 0x7F;
                WriteEvent = ymf278b_write(TempByt, VGMPnt[0x02], VGMPnt[0x03]);
                CmdLen = 0x04;
                break;
            case 0xD1:  // YMF271 write
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                WriteEvent = ymf271_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02], VGMPnt[0x03]);
                CmdLen = 0x04;
                break;
            case 0xB1:  // RF5C164 register write
                WriteEvent = rf5c164_reg_write(VGMPnt[0x01], VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0xC2:  // RF5C164 memory write
                WriteEvent = true;  // OptVgmRF works a lot better this way
                CmdLen = 0x04;
                break;
            case 0x68:  // PCM RAM write
                CmdLen = 0x0C;
                break;
            case 0xA0:  // AY8910 register write
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                WriteEvent = ay8910_write_reg(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0x90:  // DAC Ctrl: Setup Chip
                dac_stream_control_freq(VGMPnt[0x01], (UINT32) - 1); // reset frequency state (due to re-initialization)
                CmdLen = 0x05;
                break;
            case 0x91:  // DAC Ctrl: Set Data
                CmdLen = 0x05;
                break;
            case 0x92:  // DAC Ctrl: Set Freq
                memcpy(&TempLng, &VGMPnt[0x02], 0x04);
                WriteEvent = dac_stream_control_freq(VGMPnt[0x01], TempLng);
                CmdLen = 0x06;
                break;
            case 0x93:  // DAC Ctrl: Play from Start Pos
                CmdLen = 0x0B;
                break;
            case 0x94:  // DAC Ctrl: Stop immediately
                CmdLen = 0x02;
                break;
            case 0x95:  // DAC Ctrl: Play Block (small)
                CmdLen = 0x05;
                break;
            case 0xB3:  // GameBoy DMG write
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                WriteEvent = gameboy_write_reg(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0xB4:  // NES APU write
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                WriteEvent = nes_psg_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0xB5:  // MultiPCM write
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                //  WriteEvent = multipcm_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0xC3:  // MultiPCM memory write
                WriteEvent = true;
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                //  memcpy(&TempSht, &VGMPnt[0x02], 0x02);
                //  WriteEvent = multipcm_bank_write(VGMPnt[0x01] & 0x7F, TempSht);
                CmdLen = 0x04;
                break;
            case 0xB6:  // UPD7759 write
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                WriteEvent = upd7759_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0xB7:  // OKIM6258 write
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                WriteEvent = okim6258_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0xB8:  // OKIM6295 write
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                WriteEvent = okim6295_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0xD2:  // SCC1 write
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                WriteEvent = k051649_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02], VGMPnt[0x03]);
                CmdLen = 0x04;
                break;
            case 0xD3:  // K054539 write
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                WriteEvent = k054539_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02], VGMPnt[0x03]);
                CmdLen = 0x04;
                break;
            case 0xB9:  // HuC6280 write
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                WriteEvent = c6280_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0xD4:  // C140 write
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                WriteEvent = c140_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02], VGMPnt[0x03]);
                CmdLen = 0x04;
                break;
            case 0xBA:  // K053260 write
                WriteEvent = true;
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                //  WriteEvent = chip_reg_write(0x1D, CurChip, 0x00, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0xBB:  // Pokey write
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                WriteEvent = pokey_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0xC4:  // Q-Sound write
                //SetChipSet(0x00);
                WriteEvent = qsound_write(VGMPnt[0x03], (VGMPnt[0x01] << 8) | (VGMPnt[0x02] << 0));
                CmdLen = 0x04;
                break;
            case 0xC5:  // SCSP write
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                WriteEvent = scsp_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02], VGMPnt[0x03]);
                CmdLen = 0x04;
                break;
            case 0xBC:  // WonderSwan write
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                if(VGMPnt[0x01] == 0x0E)
                    WriteEvent = true;
                else
                    WriteEvent = ym3812_write(0x20 + VGMPnt[0x01], VGMPnt[0x02]);
                CmdLen = 0x03;
                break;
            case 0xC6:  // WonderSwan memory write
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                WriteEvent = true;
                CmdLen = 0x04;
                break;
            case 0xC7:  // WonderSwan memory write
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                TempSht = ((VGMPnt[0x01] & 0x7F) << 8) | (VGMPnt[0x02] << 0);
                WriteEvent = vsu_write(TempSht, VGMPnt[0x03]);
                CmdLen = 0x04;
                break;
            case 0xC8:  // X1-010 write
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                TempSht = ((VGMPnt[0x01] & 0x7F) << 8) | (VGMPnt[0x02] << 0);
                WriteEvent = x1_010_write(TempSht, VGMPnt[0x03]);
                CmdLen = 0x04;
                break;
            case 0xE1:  // C352
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                TempSht = ((VGMPnt[0x01] & 0x7F) << 8) | (VGMPnt[0x02] << 0);
                WriteEvent = c352_write(TempSht, (VGMPnt[0x03] << 8) | VGMPnt[0x04]);
                CmdLen = 0x05;
                break;
            case 0xD5:  // ES5503 write
                SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
                WriteEvent = es5503_write(VGMPnt[0x02], VGMPnt[0x03]);
                CmdLen = 0x04;
                break;
            default:
                switch(Command & 0xF0)
                {
                case 0x30:
                case 0x40:
                    CmdLen = 0x02;
                    break;
                case 0x50:
                case 0xA0:
                case 0xB0:
                    CmdLen = 0x03;
                    break;
                case 0xC0:
                case 0xD0:
                    CmdLen = 0x04;
                    break;
                case 0xE0:
                case 0xF0:
                    CmdLen = 0x05;
                    break;
                default:
                    printf("Unknown Command: %X\n", Command);
                    CmdLen = 0x01;
                    //StopVGM = true;
                    break;
                }
                break;
            }
        }

        if(WriteEvent || VGMPos == VGMHead.lngLoopOffset)
        {
            if(VGMPos == VGMHead.lngLoopOffset)
            {
                VGMLib_WriteDelay(DstData, &DstPos, AllDelay, &WroteCmd80);
                AllDelay = CmdDelay;
            }
            else
            {
                VGMLib_WriteDelay(DstData, &DstPos, AllDelay + CmdDelay, &WroteCmd80);
                AllDelay = 0x00;
            }
            CmdDelay = 0x00;

            /*if (VGMPos != VGMHead.lngLoopOffset)
            {
                AllDelay += CmdDelay;
                CmdDelay = 0x00;
            }
            VGMLib_WriteDelay(DstData, &DstPos, AllDelay, &WroteCmd80);
            AllDelay = CmdDelay;
            CmdDelay = 0x00;*/

            if(VGMPos == VGMHead.lngLoopOffset)
                NewLoopS = DstPos;

            if(WriteEvent)
            {
                // Write Event
                WroteCmd80 = ((Command & 0xF0) == 0x80);
                if(WroteCmd80)
                {
                    AllDelay += Command & 0x0F;
                    Command &= 0x80;
                }
                if(CmdLen != 0x01)
                    memcpy(&DstData[DstPos], &VGMData[VGMPos], CmdLen);
                else
                    DstData[DstPos] = Command;  // write the 0x80-command correctly
                DstPos += CmdLen;
            }
        }
        else
            AllDelay += CmdDelay;
        VGMPos += CmdLen;
        if(StopVGM)
            break;

#ifdef WIN32
        if(CmdTimer < GetTickCount())
        {
            PrintMinSec(VGMSmplPos, MinSecStr);
            PrintMinSec(VGMHead.lngTotalSamples, TempStr);
            TempLng = VGMPos - VGMHead.lngDataOffset;
            ROMSize = VGMHead.lngEOFOffset - VGMHead.lngDataOffset;
            printf("%04.3f %% - %s / %s (%08X / %08X) ...\r", (float)TempLng / ROMSize * 100,
                   MinSecStr, TempStr, VGMPos, VGMHead.lngEOFOffset);
            CmdTimer = GetTickCount() + 200;
        }
#endif
    }
    DataSizeA = VGMPos - VGMHead.lngDataOffset;
    DataSizeB = DstPos - VGMHead.lngDataOffset;
    if(VGMHead.lngLoopOffset)
    {
        VGMHead.lngLoopOffset = NewLoopS;
        if(! NewLoopS)
            printf("Error! Failed to relocate Loop Point!\n");
        else
            NewLoopS -= 0x1C;
        memcpy(&DstData[0x1C], &NewLoopS, 0x04);
    }
    printf("\t\t\t\t\t\t\t\t\r");

    if(VGMHead.lngGD3Offset && VGMHead.lngGD3Offset + 0x0B < VGMHead.lngEOFOffset)
    {
        VGMPos = VGMHead.lngGD3Offset;
        memcpy(&TempLng, &VGMData[VGMPos + 0x00], 0x04);
        if(TempLng == FCC_GD3)
        {
            memcpy(&CmdLen, &VGMData[VGMPos + 0x08], 0x04);
            CmdLen += 0x0C;

            VGMHead.lngGD3Offset = DstPos;
            TempLng = DstPos - 0x14;
            memcpy(&DstData[0x14], &TempLng, 0x04);
            memcpy(&DstData[DstPos], &VGMData[VGMPos], CmdLen);
            DstPos += CmdLen;
        }
    }
    DstDataLen = DstPos;
    TempLng = DstDataLen - 0x04;
    memcpy(&DstData[0x04], &TempLng, 0x04);

    FreeAllChips();

    return;
}

bool GetNextChipCommand(void)
{
    UINT32 CurPos;
    UINT8 Command;
    //UINT8 TempByt;
    //UINT16 TempSht;
    UINT32 TempLng;
    UINT32 CmdLen;
    bool ReturnData;
    bool CmdIsPort;
    bool FirstCmd;

    CurPos = NxtCmdPos;
    FirstCmd = true;
    while(CurPos < VGMHead.lngEOFOffset)
    {
        CmdLen = 0x00;
        Command = VGMData[CurPos + 0x00];

        if(Command >= 0x70 && Command <= 0x8F)
            CmdLen = 0x01;
        else
        {
            switch(Command)
            {
            case 0x66:  // End Of File
                NxtCmdPos = CurPos;
                CmdLen = 0x01;
                return false;
            //break;
            case 0x62:  // 1/60s delay
                CmdLen = 0x01;
                break;
            case 0x63:  // 1/50s delay
                CmdLen = 0x01;
                break;
            case 0x61:  // xx Sample Delay
                CmdLen = 0x03;
                break;
            case 0x67:  // PCM Data Stream
                memcpy(&TempLng, &VGMData[CurPos + 0x03], 0x04);
                TempLng &= 0x7FFFFFFF;
                CmdLen = 0x07 + TempLng;
                break;
            case 0x68:  // PCM RAM write
                CmdLen = 0x0C;
                break;
            case 0x50:  // SN76496 write
            case 0x30:
                if(NxtCmdCommand == 0x50)
                    ReturnData = true;
                CmdLen = 0x02;
                break;
            case 0x51:  // YM2413 write
            case 0xA1:
                if(NxtCmdCommand == 0x51)
                    ReturnData = true;
                CmdLen = 0x03;
                break;
            case 0x52:  // YM2612 write port 0
            case 0x53:  // YM2612 write port 1
            case 0xA2:
            case 0xA3:
                if((NxtCmdCommand & ~0x01) == 0x52)
                {
                    ReturnData = true;
                    CmdIsPort = true;
                }
                CmdLen = 0x03;
                break;
            case 0xE0:  // Seek to PCM Data Bank Pos
                CmdLen = 0x05;
                break;
            case 0x4F:  // GG Stereo
                if(NxtCmdCommand == 0x4F)
                    ReturnData = true;
                CmdLen = 0x02;
                break;
            case 0x54:  // YM2151 write
            case 0xA4:
                if(NxtCmdCommand == 0x54)
                    ReturnData = true;
                CmdLen = 0x03;
                break;
            case 0xC0:  // Sega PCM memory write
                if(NxtCmdCommand == 0xC0)
                    ReturnData = true;
                CmdLen = 0x04;
                break;
            case 0xB0:  // RF5C68 register write
                if(NxtCmdCommand == 0xB0)
                    ReturnData = true;
                CmdLen = 0x03;
                break;
            case 0xC1:  // RF5C68 memory write
                if(NxtCmdCommand == 0xC1)
                    ReturnData = true;
                CmdLen = 0x04;
                break;
            case 0x55:  // YM2203
            case 0xA5:
                if(NxtCmdCommand == 0x55)
                    ReturnData = true;
                CmdLen = 0x03;
                break;
            case 0x56:  // YM2608 write port 0
            case 0x57:  // YM2608 write port 1
            case 0xA6:
            case 0xA7:
                if((NxtCmdCommand & ~0x01) == 0x56)
                {
                    ReturnData = true;
                    CmdIsPort = true;
                }
                CmdLen = 0x03;
                break;
            case 0x58:  // YM2610 write port 0
            case 0x59:  // YM2610 write port 1
            case 0xA8:
            case 0xA9:
                if((NxtCmdCommand & ~0x01) == 0x58)
                {
                    ReturnData = true;
                    CmdIsPort = true;
                }
                CmdLen = 0x03;
                break;
            case 0x5A:  // YM3812 write
            case 0xAA:
                if(NxtCmdCommand == 0x5A)
                    ReturnData = true;
                CmdLen = 0x03;
                break;
            case 0x5B:  // YM3526 write
            case 0xAB:
                if(NxtCmdCommand == 0x5B)
                    ReturnData = true;
                CmdLen = 0x03;
                break;
            case 0x5C:  // Y8950 write
            case 0xAC:
                if(NxtCmdCommand == 0x5C)
                    ReturnData = true;
                CmdLen = 0x03;
                break;
            case 0x5E:  // YMF262 write port 0
            case 0x5F:  // YMF262 write port 1
            case 0xAE:
            case 0xAF:
                if((NxtCmdCommand & ~0x01) == 0x5E)
                {
                    ReturnData = true;
                    CmdIsPort = true;
                }
                CmdLen = 0x03;
                break;
            case 0x5D:  // YMZ280B write
            case 0xAD:
                if(NxtCmdCommand == 0x5D)
                    ReturnData = true;
                CmdLen = 0x03;
                break;
            case 0xB1:  // RF5C164 register write
                if(NxtCmdCommand == 0xB1)
                    ReturnData = true;
                CmdLen = 0x03;
                break;
            case 0xC2:  // RF5C164 memory write
                if(NxtCmdCommand == 0xC2)
                    ReturnData = true;
                CmdLen = 0x04;
                break;
            case 0xA0:  // AY8910 register write
                if(NxtCmdCommand == 0xA0)
                    ReturnData = true;
                CmdLen = 0x03;
                break;
            case 0x90:  // DAC Ctrl: Setup Chip
                CmdLen = 0x05;
                break;
            case 0x91:  // DAC Ctrl: Set Data
                CmdLen = 0x05;
                break;
            case 0x92:  // DAC Ctrl: Set Freq
                CmdLen = 0x06;
                break;
            case 0x93:  // DAC Ctrl: Play from Start Pos
                CmdLen = 0x0B;
                break;
            case 0x94:  // DAC Ctrl: Stop immediately
                CmdLen = 0x02;
                break;
            case 0x95:  // DAC Ctrl: Play Block (small)
                CmdLen = 0x05;
                break;
            default:
                switch(Command & 0xF0)
                {
                case 0x30:
                case 0x40:
                    if(NxtCmdCommand == Command)
                        ReturnData = true;
                    CmdLen = 0x02;
                    break;
                case 0x50:
                case 0xA0:
                case 0xB0:
                    if(NxtCmdCommand == Command)
                        ReturnData = true;
                    CmdLen = 0x03;
                    break;
                case 0xC0:
                case 0xD0:
                    if(NxtCmdCommand == Command)
                        ReturnData = true;
                    CmdLen = 0x04;
                    break;
                case 0xE0:
                case 0xF0:
                    CmdLen = 0x05;
                    break;
                default:
                    CmdLen = 0x01;
                    break;
                }
                break;
            }
        }
        if(FirstCmd)
        {
            // the first command is already read and must be skipped
            FirstCmd = false;
            ReturnData = false;
            CmdIsPort = false;
        }
        if(ReturnData)
        {
            switch(CmdLen)
            {
            case 0x02:
                NxtCmdReg = 0x00;
                NxtCmdVal = VGMData[CurPos + 0x01];
                break;
            case 0x03:
                NxtCmdReg = VGMData[CurPos + 0x01];
                if(CmdIsPort)
                    NxtCmdReg |= (Command & 0x01) << 8;
                NxtCmdVal = VGMData[CurPos + 0x02];
                break;
            case 0x04:
                NxtCmdReg = (VGMData[CurPos + 0x01] << 8) | (VGMData[CurPos + 0x02] << 0);
                NxtCmdVal = VGMData[CurPos + 0x03];
                break;
            default:
                NxtCmdReg = 0x00;
                NxtCmdVal = 0x00;
                break;
            }
            NxtCmdPos = CurPos; // support consecutive searches
            return true;
        }

        CurPos += CmdLen;
    }

    return false;
}


}// namespace VgmCMP
