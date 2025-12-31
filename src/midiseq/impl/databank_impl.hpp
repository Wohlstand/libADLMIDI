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
#ifndef BW_MIDISEQ_DATA_BANK_IMPL_HPP
#define BW_MIDISEQ_DATA_BANK_IMPL_HPP

#include <iterator>  // std::back_inserter
#include <algorithm> // std::copy

#include "../midi_sequencer.hpp"

void BW_MidiSequencer::insertDataToBank(BW_MidiSequencer::MidiEvent &evt, std::vector<uint8_t> &bank, const uint8_t *data, size_t length)
{
    evt.data_block.offset = bank.size();
    std::copy(data, data + length, std::back_inserter(bank));
    evt.data_block.size = bank.size() - evt.data_block.offset;
}

void BW_MidiSequencer::insertDataToBank(BW_MidiSequencer::MidiEvent &evt, std::vector<uint8_t> &bank, FileAndMemReader &fr, size_t length)
{
    evt.data_block.offset = bank.size();
    bank.resize(bank.size() + length);
    fr.read(bank.data() + evt.data_block.offset, 1, length);
    evt.data_block.size = bank.size() - evt.data_block.offset;
}

void BW_MidiSequencer::insertDataToBankWithByte(BW_MidiSequencer::MidiEvent &evt, std::vector<uint8_t> &bank, uint8_t begin_byte, const uint8_t *data, size_t length)
{
    evt.data_block.offset = bank.size();
    bank.push_back(begin_byte);
    std::copy(data, data + length, std::back_inserter(bank));
    evt.data_block.size = bank.size() - evt.data_block.offset;
}

void BW_MidiSequencer::insertDataToBankWithByte(BW_MidiSequencer::MidiEvent &evt, std::vector<uint8_t> &bank, uint8_t begin_byte, FileAndMemReader &fr, size_t length)
{
    evt.data_block.offset = bank.size();
    bank.push_back(begin_byte);
    bank.resize(bank.size() + length);
    fr.read(bank.data() + evt.data_block.offset, 1, length);
    evt.data_block.size = bank.size() - evt.data_block.offset;
}

void BW_MidiSequencer::insertDataToBankWithTerm(BW_MidiSequencer::MidiEvent &evt, std::vector<uint8_t> &bank, const uint8_t *data, size_t length)
{
    evt.data_block.offset = bank.size();
    std::copy(data, data + length, std::back_inserter(bank));
    bank.push_back(0);
    bank.push_back(0); /* Second terminator is an ending fix for UTF16 strings */
    evt.data_block.size = bank.size() - evt.data_block.offset;
}

void BW_MidiSequencer::insertDataToBankWithTerm(BW_MidiSequencer::MidiEvent &evt, std::vector<uint8_t> &bank, FileAndMemReader &fr, size_t length)
{
    evt.data_block.offset = bank.size();
    bank.resize(bank.size() + length);
    fr.read(bank.data() + evt.data_block.offset, 1, length);
    bank.push_back(0);
    bank.push_back(0); /* Second terminator is an ending fix for UTF16 strings */
    evt.data_block.size = bank.size() - evt.data_block.offset;
}

void BW_MidiSequencer::addEventToBank(BW_MidiSequencer::MidiTrackRow &row, const MidiEvent &evt)
{
    if(row.events_begin == row.events_end)
        row.events_begin = m_eventBank.size();

    m_eventBank.push_back(evt);
    row.events_end = m_eventBank.size();
}

#endif /* BW_MIDISEQ_DATA_BANK_IMPL_HPP */
