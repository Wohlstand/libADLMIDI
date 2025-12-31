
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
#ifndef BW_MIDISEQ_IO_IMPL_HPP
#define BW_MIDISEQ_IO_IMPL_HPP

#include <cstring>
#include <assert.h>

#include "../midi_sequencer.hpp"


double BW_MidiSequencer::Tick(double s, double granularity)
{
    assert(m_interface); // MIDI output interface must be defined!

    s *= m_tempoMultiplier;
#ifdef ENABLE_BEGIN_SILENCE_SKIPPING
    if(CurrentPositionNew.began)
#endif
        m_currentPosition.wait -= s;
    m_currentPosition.absTimePosition += s;

    int antiFreezeCounter = 10000; // Limit 10000 loops to avoid freezing
    while((m_currentPosition.wait <= granularity * 0.5) && (antiFreezeCounter > 0))
    {
        if(!processEvents())
            break;
        if(m_currentPosition.wait <= 0.0)
            antiFreezeCounter--;
    }

    if(antiFreezeCounter <= 0)
        m_currentPosition.wait += 1.0; /* Add extra 1 second when over 10000 events
                                          with zero delay are been detected */

    if(m_currentPosition.wait < 0.0) // Avoid negative delay value!
        return 0.0;

    return m_currentPosition.wait;
}


int BW_MidiSequencer::playStream(uint8_t *stream, size_t length)
{
    int count = 0;
    size_t samples = static_cast<size_t>(length / static_cast<size_t>(m_time.frameSize));
    size_t left = samples;
    size_t periodSize = 0;
    uint8_t *stream_pos = stream;

    assert(m_interface->onPcmRender);

    while(left > 0)
    {
        const double leftDelay = left / double(m_time.sampleRate);
        const double maxDelay = m_time.timeRest < leftDelay ? m_time.timeRest : leftDelay;
        if((positionAtEnd()) && (m_time.delay <= 0.0))
            break; // Stop to fetch samples at reaching the song end with disabled loop

        m_time.timeRest -= maxDelay;
        periodSize = static_cast<size_t>(static_cast<double>(m_time.sampleRate) * maxDelay);

        if(stream)
        {
            size_t generateSize = periodSize > left ? static_cast<size_t>(left) : static_cast<size_t>(periodSize);
            m_interface->onPcmRender(m_interface->onPcmRender_userData, stream_pos, generateSize * m_time.frameSize);
            stream_pos += generateSize * m_time.frameSize;
            count += generateSize;
            left -= generateSize;
            assert(left <= samples);
        }

        if(m_time.timeRest <= 0.0)
        {
            m_time.delay = Tick(m_time.delay, m_time.minDelay);
            m_time.timeRest += m_time.delay;
        }
    }

    return count * static_cast<int>(m_time.frameSize);
}

double BW_MidiSequencer::seek(double seconds, const double granularity)
{
    if(seconds < 0.0)
        return 0.0; // Seeking negative position is forbidden! :-P
    const double granualityHalf = granularity * 0.5,
                 s = seconds; // m_setup.delay < m_setup.maxdelay ? m_setup.delay : m_setup.maxdelay;

    /* Attempt to go away out of song end must rewind position to begin */
    if(seconds > m_fullSongTimeLength)
    {
        this->rewind();
        return 0.0;
    }

    bool loopFlagState = m_loopEnabled;
    // Turn loop pooints off because it causes wrong position rememberin on a quick seek
    m_loopEnabled = false;

    /*
     * Seeking search is similar to regular ticking, except of next things:
     * - We don't processsing arpeggio and vibrato
     * - To keep correctness of the state after seek, begin every search from begin
     * - All sustaining notes must be killed
     * - Ignore Note-On events
     */
    this->rewind();

    /*
     * Set "loop Start" to false to prevent overwrite of loopStart position with
     * seek destinition position
     *
     * TODO: Detect & set loopStart position on load time to don't break loop while seeking
     */
    m_loop.caughtStart   = false;

    m_loop.temporaryBroken = (seconds >= m_loopEndTime);

    while((m_currentPosition.absTimePosition < seconds) &&
          (m_currentPosition.absTimePosition < m_fullSongTimeLength))
    {
        m_currentPosition.wait -= s;
        m_currentPosition.absTimePosition += s;
        int antiFreezeCounter = 10000; // Limit 10000 loops to avoid freezing
        double dstWait = m_currentPosition.wait + granualityHalf;
        while((m_currentPosition.wait <= granualityHalf)/*&& (antiFreezeCounter > 0)*/)
        {
            // std::fprintf(stderr, "wait = %g...\n", CurrentPosition.wait);
            if(!processEvents(true))
                break;
            // Avoid freeze because of no waiting increasing in more than 10000 cycles
            if(m_currentPosition.wait <= dstWait)
                antiFreezeCounter--;
            else
            {
                dstWait = m_currentPosition.wait + granualityHalf;
                antiFreezeCounter = 10000;
            }
        }
        if(antiFreezeCounter <= 0)
            m_currentPosition.wait += 1.0;/* Add extra 1 second when over 10000 events
                                             with zero delay are been detected */
    }

    if(m_currentPosition.wait < 0.0)
        m_currentPosition.wait = 0.0;

    if(m_atEnd)
    {
        this->rewind();
        m_loopEnabled = loopFlagState;
        return 0.0;
    }

    m_time.reset();
    m_time.delay = m_currentPosition.wait;

    m_loopEnabled = loopFlagState;
    return m_currentPosition.wait;
}

double BW_MidiSequencer::tell()
{
    return m_currentPosition.absTimePosition;
}

double BW_MidiSequencer::timeLength()
{
    return m_fullSongTimeLength;
}

double BW_MidiSequencer::getLoopStart()
{
    return m_loopStartTime;
}

double BW_MidiSequencer::getLoopEnd()
{
    return m_loopEndTime;
}

void BW_MidiSequencer::rewind()
{
    m_currentPosition   = m_trackBeginPosition;
    m_atEnd             = false;

    m_loop.loopsCount = m_loopCount;
    m_loop.reset();
    m_loop.caughtStart  = true;
    m_loop.temporaryBroken = false;

    for(size_t tk = 0; tk < m_tracksCount; ++tk)
    {
        LoopState &loop = m_trackState[tk].loop;
        loop.reset();
        loop.temporaryBroken = false;
    }

    m_time.reset();

    // Clear any hanging timed notes
    duratedNoteClear();
    restoreSongState();
}

bool BW_MidiSequencer::positionAtEnd()
{
    return m_atEnd;
}

double BW_MidiSequencer::getTempoMultiplier()
{
    return m_tempoMultiplier;
}

void BW_MidiSequencer::setTempo(double tempo)
{
    m_tempoMultiplier = tempo;
}

#endif /* BW_MIDISEQ_IO_IMPL_HPP */
