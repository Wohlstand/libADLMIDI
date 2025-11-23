/*
 * WOPL dumper tool - prints the content of OPL2/OPL3 bank of WOPL format as
 * a text data with all the descriptions attached.
 *
 * Copyright (c) 2025-2025 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "../../src/wopl/wopl_file.h"


static void dump_instrument(const WOPLInstrument *inst, uint16_t i, int is_drum)
{
    uint16_t o, o_max;
    const WOPLOperator *op;

    static const char* op_name[] =
    {
        "CAR1",
        "MOD1",
        "CAR2",
        "MOD2"
    };

    printf("INS=%u: ", i);

    if((inst->inst_flags & WOPL_Ins_IsBlank) != 0)
    {
        printf("-- Blank instrument --\n");
        return;
    }

    o_max = (inst->inst_flags & (WOPL_Ins_4op|WOPL_Ins_Pseudo4op)) != 0 ? 4 : 2;

    printf("NoteOff1=%d", inst->note_offset1);

    if(is_drum)
        printf(" PercKey=%d", inst->percussion_key_number);

    if((inst->inst_flags & WOPL_Ins_Pseudo4op) != 0)
    {
        printf(" NoteOff2=%d", inst->note_offset2);
        printf(" Detune=%d", inst->second_voice_detune);
        printf(" DoubleVoice");
    }

    printf(" VelOff=%d", inst->midi_velocity_offset);

    if(o_max == 4)
    {
        printf(" FB1=%u", (inst->fb_conn1_C0 >> 1) & 0x07);
        printf(" Conn1=%u", inst->fb_conn1_C0 & 0x01);
        printf(" FB2=%u", (inst->fb_conn2_C0 >> 1) & 0x07);
        printf(" Conn2=%u", inst->fb_conn2_C0 & 0x01);
        printf(" 4-OP");
    }
    else
    {
        printf(" FB=%u", (inst->fb_conn1_C0 >> 1) & 0x07);
        printf(" Conn=%u", inst->fb_conn1_C0 & 0x01);
        printf(" 2-OP");
    }

    printf(" Operators:\n");

    for(o = 0; o < o_max; ++o)
    {
        op = &inst->operators[o];
        printf("  - Op%u=%s [", o, op_name[o]);
        printf("Attack=%2u", ((op->atdec_60 >> 4) & 0x0F));
        printf(" Decay=%2u", (op->atdec_60 & 0x0F));

        printf(" Sustain=%2u", ((op->susrel_80 >> 4) & 0x0F));
        printf(" Release=%2u", (op->susrel_80 & 0x0F));

        printf(" KSL=%u", ((op->ksl_l_40 >> 6) & 0x03));
        printf(" TL=%2u", op->ksl_l_40 & 0x3F);

        printf(" AM=%u", ((op->avekf_20 >> 7) & 0x01));
        printf(" VIB=%u", ((op->avekf_20 >> 6) & 0x01));
        printf(" EG=%u", ((op->avekf_20 >> 5) & 0x01));
        printf(" KSR=%u", ((op->avekf_20 >> 4) & 0x01));
        printf(" FMult=%2u", op->avekf_20 & 0x0F);
        printf(" WaveForm=%u", (op->waveform_E0 & 0x0F));
        printf("]\n");
    }

    printf("\n");
}

int main(int argc, char **argv)
{
    FILE *wopl_bank = NULL;
    size_t wopl_size = 0;
    unsigned char* wopl_dump = NULL;
    WOPLFile *wopl = NULL;
    WOPLBank *bank = NULL;
    int wopl_err = 0;
    uint16_t i, b;

    if(argc < 2)
    {
        printf("Syntax:\n\n"
               "%s <WOPL-bank path>\n\n", argv[0]);
        return 2;
    }

    wopl_bank = fopen(argv[1], "rb");
    if(!wopl_bank)
    {
        printf("Cant' open the file %s\n", argv[1]);
        return 1;
    }

    fseek(wopl_bank, 0, SEEK_END);
    wopl_size = ftell(wopl_bank);
    fseek(wopl_bank, 0, SEEK_SET);

    wopl_dump = (unsigned char*)malloc(wopl_size);
    if(!wopl_dump)
    {
        printf("Can't allocate memory to load the file %s\n", argv[1]);
        fclose(wopl_bank);
        return 1;
    }

    if(fread(wopl_dump, 1, wopl_size, wopl_bank) != wopl_size)
    {
        printf("Failed to read data of the file %s\n", argv[1]);
        free(wopl_dump);
        fclose(wopl_bank);
        return 1;
    }

    fclose(wopl_bank);

    wopl = WOPL_LoadBankFromMem(wopl_dump, wopl_size, &wopl_err);
    if(!wopl)
    {
        printf("Failed load WOPL from data %s: Error = %d\n", argv[1], wopl_err);
        free(wopl_dump);
        return 1;
    }

    printf("\n"
           "====================================================================================\n");
    printf("Content of the %s file.\n", argv[1]);
    printf("====================================================================================\n");
    printf("All values shown exactly in the native OPL format without rescales or inversions as\n"
           "they are injected per-bit and sent to the chip (For example, the \"Total Level\" value in\n"
           "native chip's format means 0 the loudest and 63 is a sielence. In this list, they shown in\n"
           "exactly this format. Programs like OPL3 Bank Editor or SBTimbre usually show these\n"
           "values (Sustain Rate and the Total Level) in the inverted format for the human\n"
           "convenience to work with these values).\n");
    printf("\n\n");
    printf("====================================================================================\n");
    printf("Legend:\n\n");
    printf("- INS=x - The instrument entry. The x is a MIDI patch number or drum key note.\n");
    printf("- NoteOff1= - The MIDI note offset for the instrument, 0 is centre.\n");
    printf("- NoteOff2= - The Second MIDI note offset for the instrument, only for double-voice 2-OP mode.\n");
    printf("- PercKey=x - The MIDI tone to play the drum instrument.\n");
    printf("- Detune=x - The detune tiny bend offset for the second voice.\n");
    printf("- DoubleVoice - The instrument is the second voice.\n");
    printf("- VelOff=x - The MIDI velocity offset.\n");
    printf("- FB    - The Feedback value for two-operator voice (from 0 to 7).\n");
    printf("- Conn  - The operators connection type (0=FM, 1=AM)\n");
    printf("- FB1   - The Feedback value for the first operators pair (from 0 to 7).\n");
    printf("- Conn1 - The first pair of operators connection type (0=FM, 1=AM)\n");
    printf("- FB2   - The Feedback value for the second operators pair (from 0 to 7).\n");
    printf("- Conn2 - The second pair of operators connection type (0=FM, 1=AM)\n");
    printf("\n");
    printf("- Opx=y - The operator declaration: x - number from 0 to 3, y is the name of operator:\n");
    printf("  - CARx  - The operator of \"carrier\" role. x = 1 or 2.\n");
    printf("  - MODx  - The operator \"modulator\" role. x = 1 or 2.\n");
    printf("- Attack=x - The operator's attack rate (from 0 to 15, 0 is silence, 15 is instant attack).\n");
    printf("- Decay=x - The operator's decay rate (from 0 to 15, 0 - don't decay, 15 is instant decay).\n");
    printf("- Sustain=x - The operator's sustane position (from 0 to 15, 0 - the loudest sustain, 15 is silence).\n");
    printf("- Release=x - The operator's release rate (from 0 to 15, 0 - the infinite releasing, 15 is instant releasing).\n");
    printf("- KSL=x - The operator's key scale level (From 0 to 3, 0 - don't scale, 3 - widest scale level).\n");
    printf("- TL=x - The operator's total level (From 0 to 63, 0 - loudest level, 63 - the silence).\n");
    printf("- AM=x - The operator's amplitude amodulation \"tremolo\" flag by LFO (0 - disable, 1 enable).\n");
    printf("- VIB=x - The operator's frequency amodulation \"vibrato\" flag by LFO (0 - disable, 1 enable).\n");
    printf("- EG=x - Enable voice sustain for operator flag (0 - disable sustain, 1 enable sustain).\n");
    printf("- KSR=x - Enable envelope scale voice by tone (0 - envelope is equal, 1 higher notes shorter than lower).\n");
    printf("- FMult=x - The operator's frequency multiplication factor (from 0 to 15, and when 0 is 0.5x, 1 is 1x, 2 is 2x, etc.).\n");
    printf("- WaveForm=x - The form of operator's wave:\n");
    printf("  - 0 - Sine\n");
    printf("  - 1 - Half-Sine\n");
    printf("  - 2 - Absolute of Sine\n");
    printf("  - 3 - Pulse Sine\n");
    printf("  - 4 - Only even period of Sine (OPL3-only)\n");
    printf("  - 5 - Only even period of Absolute of Sine (OPL3-only)\n");
    printf("  - 6 - Square Wave (OPL3-only)\n");
    printf("  - 7 - Derived Square Wave (OPL3-only)\n");
    printf("====================================================================================\n");
    printf("\n\n");

    for(b = 0; b < wopl->banks_count_melodic; ++b)
    {
        bank = &wopl->banks_melodic[b];
        printf("== Melodic bank #%u (Name=%s, LSB=%u, MSB=%u) ==\n", b, bank->bank_name, bank->bank_midi_msb, bank->bank_midi_lsb);
        for(i = 0; i < 127; ++i)
            dump_instrument(&bank->ins[i], i, 0);

        printf("\n\n");
    }

    for(b = 0; b < wopl->banks_count_percussion; ++b)
    {
        bank = &wopl->banks_percussive[b];
        printf("== Percussion bank #%u (Name=%s, LSB=%u, MSB=%u) ==\n", b, bank->bank_name, bank->bank_midi_msb, bank->bank_midi_lsb);
        for(i = 0; i < 127; ++i)
            dump_instrument(&bank->ins[i], i, 1);

        printf("\n\n");
    }

    fflush(stdout);

    WOPL_Free(wopl);
    free(wopl_dump);

    return 0;
}
