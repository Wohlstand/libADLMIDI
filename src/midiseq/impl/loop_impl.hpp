
/*
 * BW_Midi_Sequencer - MIDI Sequencer for C++
 *
 * Copyright (c) 2015-2025 Vitaly Novichkov <admin@wohlnet.ru>
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
#ifndef BW_MIDISEQ_LOOP_IMPL_HPP
#define BW_MIDISEQ_LOOP_IMPL_HPP

#include <cstring>
#include <assert.h>

#include "../midi_sequencer.hpp"


BW_MidiSequencer::LoopStackEntry::LoopStackEntry() :
    infinity(false),
    loops(0),
    start(0),
    end(0)
{}

BW_MidiSequencer::LoopState::LoopState() :
    caughtStart(false),
    caughtEnd(false),
    caughtStackStart(false),
    caughtStackEnd(false),
    caughtStackBreak(false),
    skipStackStart(false),
    invalidLoop(false),
    temporaryBroken(false),
    loopsCount(-1),
    loopsLeft(0),
    stackDepth(0),
    stackLevel(-1)
{}

void BW_MidiSequencer::LoopState::reset()
{
    caughtStart = false;
    caughtEnd = false;
    caughtStackStart = false;
    caughtStackEnd = false;
    caughtStackBreak = false;
    skipStackStart = false;
    loopsLeft = loopsCount;
}

void BW_MidiSequencer::LoopState::fullReset()
{
    loopsCount = -1;
    reset();
    invalidLoop = false;
    temporaryBroken = false;
    stackDepth = 0;
    stackLevel = -1;
}

bool BW_MidiSequencer::LoopState::isStackEnd()
{
    if(caughtStackEnd && (stackLevel >= 0) && (stackLevel < static_cast<int>(stackDepth)))
    {
        const LoopStackEntry &e = stack[static_cast<size_t>(stackLevel)];
        if(e.infinity || (!e.infinity && e.loops > 0))
            return true;
    }

    return false;
}

void BW_MidiSequencer::LoopState::stackUp(int count)
{
    stackLevel += count;
}

void BW_MidiSequencer::LoopState::stackDown(int count)
{
    stackLevel -= count;
}

BW_MidiSequencer::LoopStackEntry &BW_MidiSequencer::LoopState::getCurStack()
{
    if((stackLevel >= 0) && (stackLevel < static_cast<int>(stackDepth)))
        return stack[static_cast<size_t>(stackLevel)];

    if(stackDepth == 0)
    {
        LoopStackEntry d;
        d.loops = 0;
        d.infinity = 0;
        d.start = 0;
        d.end = 0;
        stack[stackDepth++] = d;
    }

    return stack[0];
}

void BW_MidiSequencer::installLoop(BW_MidiSequencer::LoopPointParseState &loopState)
{
    if(loopState.gotLoopStart && !loopState.gotLoopEnd)
    {
        loopState.gotLoopEnd = true;
        loopState.loopEndTicks = loopState.ticksSongLength;
    }

    // loopStart must be located before loopEnd!
    if(loopState.loopStartTicks >= loopState.loopEndTicks)
    {
        m_loop.invalidLoop = true;
        if(m_interface->onDebugMessage && (loopState.gotLoopStart || loopState.gotLoopEnd))
        {
            m_interface->onDebugMessage(
                m_interface->onDebugMessage_userData,
                "== Invalid loop detected! [loopEnd is going before loopStart] =="
            );
        }
    }
}

void BW_MidiSequencer::analyseLoopEvent(LoopPointParseState &loopState, const MidiEvent &event, uint64_t abs_position)
{
    LoopStackEntry *loopEntryP;

    if(!m_loop.invalidLoop && (event.subtype == MidiEvent::ST_LOOPSTART))
    {
        /*
         * loopStart is invalid when:
         * - starts together with loopEnd
         * - appears more than one time in same MIDI file
         */
        if(loopState.gotLoopStart || (loopState.gotLoopEventsInThisRow & GLOBAL_LOOP) != 0)
            m_loop.invalidLoop = true;
        else
        {
            loopState.gotLoopStart = true;
            loopState.loopStartTicks = abs_position;
        }
        // In this row we got loop event, register this!
        loopState.gotLoopEventsInThisRow |= GLOBAL_LOOP;
    }
    else if(!m_loop.invalidLoop && (event.subtype == MidiEvent::ST_LOOPEND))
    {
        /*
         * loopEnd is invalid when:
         * - starts before loopStart
         * - starts together with loopStart
         * - appars more than one time in same MIDI file
         */
        if(loopState.gotLoopEnd || (loopState.gotLoopEventsInThisRow & GLOBAL_LOOP) != 0)
        {
            m_loop.invalidLoop = true;
            if(m_interface->onDebugMessage)
            {
                m_interface->onDebugMessage(
                    m_interface->onDebugMessage_userData,
                    "== Invalid loop detected! %s %s ==",
                    (loopState.gotLoopEnd ? "[Caught more than 1 loopEnd!]" : ""),
                    (loopState.gotLoopEventsInThisRow ? "[loopEnd in same row as loopStart!]" : "")
                );
            }
        }
        else
        {
            loopState.gotLoopEnd = true;
            loopState.loopEndTicks = abs_position;
        }
        // In this row we got loop event, register this!
        loopState.gotLoopEventsInThisRow |= GLOBAL_LOOP;
    }
    else if(!m_loop.invalidLoop && (event.subtype == MidiEvent::ST_LOOPSTACK_BEGIN))
    {
        if(!loopState.gotStackLoopStart)
        {
            if(!loopState.gotLoopStart)
                loopState.loopStartTicks = abs_position;
            loopState.gotStackLoopStart = true;
        }

        m_loop.stackUp();
        if(m_loop.stackLevel >= static_cast<int>(m_loop.stackDepth))
        {
            if(m_loop.stackDepth >= LoopState::stackDepthMax)
            {
                m_loop.invalidLoop = true;
                if(m_interface->onDebugMessage)
                {
                    m_interface->onDebugMessage(
                        m_interface->onDebugMessage_userData,
                        "== Invalid loop detected! [Nested loops depth is overflown! (Maximum 127)] =="
                    );
                }
            }
            else
            {
                loopEntryP = &m_loop.stack[m_loop.stackDepth++];
                loopEntryP->loops = event.data_loc[0];
                loopEntryP->infinity = (event.data_loc[0] == 0);
                loopEntryP->start = abs_position;
                loopEntryP->end = abs_position;
            }
        }

        // In this row we got loop event, register this!
        loopState.gotLoopEventsInThisRow |= GLOBAL_LOOPSTACK;
    }
    else if(!m_loop.invalidLoop &&
        ((event.subtype == MidiEvent::ST_LOOPSTACK_END) ||
         (event.subtype == MidiEvent::ST_LOOPSTACK_BREAK))
    )
    {
        if(m_loop.stackLevel <= -1)
        {
            m_loop.invalidLoop = true; // Caught loop end without of loop start!
            if(m_interface->onDebugMessage)
            {
                m_interface->onDebugMessage(
                    m_interface->onDebugMessage_userData,
                    "== Invalid loop detected! [Caught loop end without of loop start] =="
                );
            }
        }
        else
        {
            if(loopState.loopEndTicks < abs_position)
                loopState.loopEndTicks = abs_position;
            m_loop.getCurStack().end = abs_position;
            m_loop.stackDown();
        }

        // In this row we got loop event, register this!
        loopState.gotLoopEventsInThisRow |= GLOBAL_LOOPSTACK;
    }
}

#endif /* BW_MIDISEQ_LOOP_IMPL_HPP */
