#define DELAY_60HZ  735
#define DELAY_50HZ  882

namespace VgmCMP
{

// checks if Dly is in range from (From + 0x00) to (From + Range)
#define DELAY_CHECK(Dly, From, Range)   ((Dly >= (From)) && (Dly <= (From + Range)))

static void VGMLib_WriteDelay(UINT8 *DstData, UINT32 *Pos, UINT32 Delay, bool *WroteCmd80)
{
    UINT32 DstPos;
    UINT16 CurDly;
    UINT32 TempLng;
    bool WrtCmdPCM;

    DstPos = *Pos;
    WrtCmdPCM = (WroteCmd80 == NULL) ? false : *WroteCmd80;

    while(Delay)
    {
        if(Delay <= 0xFFFF)
            CurDly = (UINT16)Delay;
        else
            CurDly = 0xFFFF;

        if(WrtCmdPCM)
        {
            // highest delay compression - Example:
            // Delay   39 -> 8F 7F 77
            // Delay 1485 -> 8F 62 62 (instead of 80 61 CD 05)
            // Delay  910 -> 8F 63 7D (instead of 80 61 8E 03)
            if(DELAY_CHECK(CurDly, 0x20, 0x0F))                 // 8F 7x
                CurDly -= 0x10;
            else if(DELAY_CHECK(CurDly, DELAY_60HZ, 0x1F))      // 8F 62 [7x]
                CurDly -= DELAY_60HZ;
            else if(DELAY_CHECK(CurDly, 2 * DELAY_60HZ, 0x0F))  // 8F 62 62
                CurDly -= 2 * DELAY_60HZ;
            else if(DELAY_CHECK(CurDly, DELAY_50HZ, 0x1F))      // 8F 63 [7x]
                CurDly -= DELAY_50HZ;
            else if(DELAY_CHECK(CurDly, 2 * DELAY_50HZ, 0x0F))  // 8F 63 63
                CurDly -= 2 * DELAY_50HZ;
            else if(DELAY_CHECK(CurDly,                         // 8F 62 63
                                DELAY_60HZ + DELAY_50HZ, 0x0F))
                CurDly -= DELAY_60HZ + DELAY_50HZ;

            /*if (CurDly >= 0x10 && CurDly <= 0x1F)
                CurDly = 0x0F;
            else if (CurDly >= 0x20)
                CurDly = 0x00;*/
            if(CurDly >= 0x10)
                CurDly = 0x0F;
            DstData[DstPos - 1] |= CurDly;
            *WroteCmd80 = WrtCmdPCM = false;
        }
        else if(! CurDly)
        {
            // don't do anything - I just want to be safe
        }
        else if(CurDly <= 0x20)
        {
            if(CurDly <= 0x10)
            {
                DstData[DstPos] = 0x70 | (CurDly - 0x01);
                DstPos ++;
            }
            else
            {
                DstData[DstPos] = 0x7F;
                DstPos ++;
                DstData[DstPos] = 0x70 | (CurDly - 0x11);
                DstPos ++;
            }
        }
        else if(DELAY_CHECK(CurDly, DELAY_60HZ, 0x10) ||
                CurDly == 2 * DELAY_60HZ)
        {
            TempLng = CurDly;
            while(TempLng >= DELAY_60HZ)
            {
                DstData[DstPos] = 0x62;
                DstPos ++;
                TempLng -= DELAY_60HZ;
            }
            CurDly -= (UINT16)TempLng;
        }
        else if(DELAY_CHECK(CurDly, DELAY_50HZ, 0x10) ||
                CurDly == 2 * DELAY_50HZ)
        {
            TempLng = CurDly;
            while(TempLng >= DELAY_50HZ)
            {
                DstData[DstPos] = 0x63;
                DstPos ++;
                TempLng -= DELAY_50HZ;
            }
            CurDly -= (UINT16)TempLng;
        }
        else if(CurDly == (DELAY_60HZ + DELAY_50HZ))
        {
            DstData[DstPos] = 0x63;
            DstPos ++;
            DstData[DstPos] = 0x62;
            DstPos ++;
        }
        else
        {
            DstData[DstPos + 0x00] = 0x61;
            DstData[DstPos + 0x01] = (CurDly & 0x00FF) >> 0;
            DstData[DstPos + 0x02] = (CurDly & 0xFF00) >> 8;
            DstPos += 0x03;
        }
        Delay -= CurDly;
    }

    *Pos = DstPos;
    return;
}

} // namespace VgmCMP
