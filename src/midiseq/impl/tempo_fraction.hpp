/*
 * BW_Midi_Sequencer - MIDI Sequencer for C++
 *
 * Copyright (c) 2015-2026 Vitaly Novichkov <admin@wohlnet.ru>
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

#pragma once
#ifndef BW_MIDISEQ_TEMPO_FRACTION_HPP
#define BW_MIDISEQ_TEMPO_FRACTION_HPP

#include "../midi_sequencer.hpp"

void BW_MidiSequencer::tempo_optimize(Tempo_t *tempo)
{
    uint64_t n1, n2, tmp;

    if(!tempo->nom)
    {
        tempo->denom = 1;
        return;
    }

    if(tempo->nom < tempo->denom)
    {
        n1 = tempo->nom;
        n2 = tempo->denom;
    }
    else
    {
        n1 = tempo->denom;
        n2 = tempo->nom;
    }

    tmp = n2 % n1;

    while(tmp)
    {
        n2 = n1;
        n1 = tmp;
        tmp = n2 % n1;
    };

    tempo->nom /= n1;
    tempo->denom /= n1;
}

void BW_MidiSequencer::tempo_mul(Tempo_t *out, const Tempo_t *val1, uint64_t val2)
{
    out->nom = val1->nom * val2;
    out->denom = val1->denom;
    tempo_optimize(out);
}

#endif /* BW_MIDISEQ_TEMPO_FRACTION_HPP */
