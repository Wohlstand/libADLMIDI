
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

#include "common.hpp"
#include "../midi_sequencer.hpp"


BW_MidiSequencer::LoopStackEntry::LoopStackEntry() :
    infinity(false),
    loops(0),
    start(0),
    end(0),
    id(LOOP_STACK_NO_ID)
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
    caughtBranchJump(false),
    dstBranchId(0),
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
    caughtBranchJump = false;
    dstBranchId = 0;
}

void BW_MidiSequencer::LoopState::fullReset()
{
    loopsCount = -1;
    reset();
    invalidLoop = false;
    temporaryBroken = false;
    stackDepth = 0;
    stackLevel = -1;
    caughtBranchJump = false;
    dstBranchId = 0;
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
    {
        if(stackLevel >= static_cast<int>(LoopState::stackDepthMax))
            return stack[LoopState::stackDepthMax - 1];
        else
            return stack[static_cast<size_t>(stackLevel)];
    }

    if(stackDepth == 0)
    {
        LoopStackEntry d;
        d.loops = 0;
        d.infinity = 0;
        d.start = 0;
        d.end = 0;
        d.id = LOOP_STACK_NO_ID;
        stack[stackDepth++] = d;
    }

    return stack[0];
}

void BW_MidiSequencer::installLoop(BW_MidiSequencer::LoopPointParseState &loopState)
{
    Position scanPosition, rowBegin;
    bool found = false;
    bool gotGlobStart = false;
    bool gotBranchId = false;
    uint64_t minDelay;
    BranchEntry branch;
    LoopRuntimeState rtLoopState;
    Tempo_t t, curTempo = m_tempo;

    std::memset(&rtLoopState, 0, sizeof(rtLoopState));

    if(loopState.gotLoopStart && !loopState.gotLoopEnd)
    {
        loopState.gotLoopEnd = true;
        loopState.loopEndTicks = loopState.ticksSongLength;
    }

    // loopStart must be located before loopEnd!
    if(loopState.loopStartTicks != 0 && loopState.loopEndTicks != 0 && loopState.loopStartTicks >= loopState.loopEndTicks)
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

    // Find loop points and branches
    scanPosition = m_trackBeginPosition;

    // Ensure the list of branches is clear!
    m_branches.clear();

    do
    {
        if(scanPosition.track.empty())
            break; // Nothing to do!

        rowBegin = scanPosition;

        for(size_t tk = 0; tk < m_tracksCount; ++tk)
        {
            Position::TrackInfo &track = scanPosition.track[tk];
            MidiTrackQueue::iterator end = m_trackData[tk].end();

            if((track.lastHandledEvent >= 0) && (track.delay <= 0))
            {
                // Check is an end of track has been reached
                if(track.pos == end)
                {
                    track.lastHandledEvent = -1;
                    break;
                }

                for(size_t i = track.pos->events_begin; i < track.pos->events_end; ++i)
                {
                    const MidiEvent &evt = m_eventBank[i];
                    track.lastHandledEvent = evt.type;

                    if(evt.type == MidiEvent::T_SPECIAL)
                    {
                        switch(evt.subtype)
                        {
                        case MidiEvent::ST_TEMPOCHANGE:
                            tempo_mul(&curTempo, &m_invDeltaTicks, readBEint(evt.data_loc, evt.data_loc_size));
                            break;
                        case MidiEvent::ST_LOOPSTART:
                            gotGlobStart = true;
                            break;
                        case MidiEvent::ST_BRANCH_LOCATION:
                        case MidiEvent::ST_TRACK_BRANCH_LOCATION:
                            gotBranchId = true;
                            break;
                        case MidiEvent::ST_ENDTRACK:
                            track.lastHandledEvent = -1;
                            break;
                        }
                    }
                    else
                    {
                        if(track.state.track_channel != evt.channel)
                            track.state.track_channel = evt.channel;

                        switch(evt.type)
                        {
                        case MidiEvent::T_CTRLCHANGE:
                            if(evt.data_loc[0] < 102)
                                track.state.cc_values[evt.data_loc[0]] = evt.data_loc[1];
                            break;
                        case MidiEvent::T_PATCHCHANGE:
                            track.state.reserve_patch = evt.data_loc[0];
                            break;
                        case MidiEvent::T_WHEEL:
                            track.state.reserve_wheel[0] = evt.data_loc[0];
                            track.state.reserve_wheel[1] = evt.data_loc[1];
                            break;
                        case MidiEvent::T_CHANAFTTOUCH:
                            track.state.reserve_channel_att = evt.data_loc[0];
                            break;
                        case MidiEvent::T_NOTETOUCH:
                            track.state.reserve_note_att[evt.data_loc[0] & 0x7F] = evt.data_loc[1];
                            break;
                        }
                    }

                    if(gotBranchId)
                    {
                        bool duplicate = false;

                        branch.id = readLEint16(evt.data_loc, evt.data_loc_size);
                        branch.tick = scanPosition.absTickPosition;
                        branch.init = true;

                        if(evt.subtype == MidiEvent::ST_TRACK_BRANCH_LOCATION)
                        {
                            branch.track = tk;
                            branch.offset.assignOneTrack(&rowBegin, tk);
                        }
                        else
                        {
                            branch.track = BRANCH_GLOBAL_TRACK;
                            branch.offset = rowBegin;
                        }

                        for(std::vector<BranchEntry>::iterator it = m_branches.begin(); it != m_branches.end(); ++it)
                        {
                            BranchEntry &e = *it;
                            if(e.id == branch.id && e.track == branch.track)
                            {
                                duplicate = true;
                                break;
                            }
                        }

                        if(!duplicate)
                            m_branches.push_back(branch);

                        gotBranchId = false;
                    }

                    if(gotGlobStart)
                    {
                        ++rtLoopState.numStackLoopStarts;
                        gotGlobStart = false;
                    }

                    if(track.lastHandledEvent < 0)
                        break;
                }

                // Read next event time (unless the track just ended)
                if(track.lastHandledEvent >= 0)
                {
                    track.delay += track.pos->delay;
                    ++track.pos;
                }
            }
        }

        found = false;
        minDelay = 0;

        for(size_t tk = 0; tk < m_tracksCount; ++tk)
        {
            Position::TrackInfo &track = scanPosition.track[tk];
            // Normal events
            if((track.lastHandledEvent >= 0) && (!found || track.delay < minDelay))
            {
                minDelay = track.delay;
                found = true;
            }
        }

        // Schedule the next playevent to be processed after that delay
        for(size_t tk = 0; tk < m_tracksCount; ++tk)
            scanPosition.track[tk].delay -= minDelay;

        tempo_mul(&t, &curTempo, minDelay);
        scanPosition.absTickPosition += minDelay;
        scanPosition.absTimePosition += tempo_get(&t);

        if(rtLoopState.numGlobLoopStarts > 0 && m_loopBeginPosition.absTimePosition <= 0.0)
            m_loopBeginPosition = rowBegin;

    } while(found);
}

void BW_MidiSequencer::setLoopStackStart(LoopPointParseState &loopState, LoopState *dstLoop, const MidiEvent &event, uint64_t abs_position, unsigned int type)
{
    LoopStackEntry *loopEntryP;

    if(!dstLoop)
        return; // Nothing to do!

    if(!loopState.gotStackLoopStart)
    {
        if(!loopState.gotLoopStart)
            loopState.loopStartTicks = abs_position;
        loopState.gotStackLoopStart = true;
    }

    dstLoop->stackUp();
    if(dstLoop->stackLevel >= static_cast<int>(dstLoop->stackDepth))
    {
        if(dstLoop->stackDepth >= LoopState::stackDepthMax)
        {
            dstLoop->invalidLoop = true;
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
            loopEntryP = &dstLoop->stack[dstLoop->stackDepth++];
            loopEntryP->loops = event.data_loc[0];
            loopEntryP->infinity = (event.data_loc[0] == 0);
            loopEntryP->start = abs_position;
            if(event.subtype == MidiEvent::ST_LOOPSTACK_BEGIN_ID || event.subtype == MidiEvent::ST_TRACK_LOOPSTACK_BEGIN_ID)
                loopEntryP->id = event.data_loc[1];
            else
                loopEntryP->id = LOOP_STACK_NO_ID;
            loopEntryP->end = abs_position;
        }
    }

    // In this row we got loop event, register this!
    loopState.gotLoopEventsInThisRow |= type;
}

void BW_MidiSequencer::setLoopStackEnd(LoopPointParseState &loopState, LoopState *dstLoop, uint64_t abs_position, unsigned int type)
{
    if(!dstLoop)
        return; // Nothing to do!

    if(dstLoop->stackLevel <= -1)
    {
        dstLoop->invalidLoop = true; // Caught loop end without of loop start!
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

        if(dstLoop->dstLoopStackId != LOOP_STACK_NO_ID)
        {
            while(dstLoop->stackLevel >= 0 && dstLoop->getCurStack().id != dstLoop->dstLoopStackId)
            {
                dstLoop->getCurStack().end = abs_position;
                dstLoop->stackDown();
            }

            if(dstLoop->stackLevel < 0)
            {
                dstLoop->invalidLoop = true; // Loop ID is not exists!
                if(m_interface->onDebugMessage)
                {
                    m_interface->onDebugMessage(
                        m_interface->onDebugMessage_userData,
                        "== Invalid loop detected! [Caught loop end ID tag with invalid loop end!] =="
                    );
                }
            }
        }

        dstLoop->getCurStack().end = abs_position;
        dstLoop->stackDown();
    }

    // In this row we got loop event, register this!
    loopState.gotLoopEventsInThisRow |= type;
}

void BW_MidiSequencer::analyseLoopEvent(LoopPointParseState &loopState, const MidiEvent &event, uint64_t abs_position, LoopState *trackLoop)
{
    if(m_loop.invalidLoop)
        return; // Nothing to do

    switch(event.subtype)
    {
    case MidiEvent::ST_LOOPSTART: // Global loop start
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
        break;

    case MidiEvent::ST_LOOPEND: // Global loop end
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
        break;

    case MidiEvent::ST_LOOPSTACK_BEGIN_ID:
        m_loop.dstLoopStackId = event.data_loc[1];
        setLoopStackStart(loopState, &m_loop, event, abs_position, GLOBAL_LOOPSTACK);
        break;
    case MidiEvent::ST_LOOPSTACK_BEGIN:
        m_loop.dstLoopStackId = LOOP_STACK_NO_ID;
        setLoopStackStart(loopState, &m_loop, event, abs_position, GLOBAL_LOOPSTACK);
        break;

    case MidiEvent::ST_LOOPSTACK_END_ID:
        m_loop.dstLoopStackId = event.data_loc[0];
        setLoopStackEnd(loopState, &m_loop, abs_position, GLOBAL_LOOPSTACK);
        break;
    case MidiEvent::ST_LOOPSTACK_END:
    case MidiEvent::ST_LOOPSTACK_BREAK:
        m_loop.dstLoopStackId = LOOP_STACK_NO_ID;
        setLoopStackEnd(loopState, &m_loop, abs_position, GLOBAL_LOOPSTACK);
        break;

    case MidiEvent::ST_TRACK_LOOPSTACK_BEGIN_ID:
        trackLoop->dstLoopStackId = event.data_loc[1];
        setLoopStackStart(loopState, trackLoop, event, abs_position, LOCAL_LOOPSTACK);
        break;
    case MidiEvent::ST_TRACK_LOOPSTACK_BEGIN:
        trackLoop->dstLoopStackId = LOOP_STACK_NO_ID;
        setLoopStackStart(loopState, trackLoop, event, abs_position, LOCAL_LOOPSTACK);
        break;

    case MidiEvent::ST_TRACK_LOOPSTACK_END_ID:
        trackLoop->dstLoopStackId = event.data_loc[0];
        setLoopStackEnd(loopState, trackLoop, abs_position, LOCAL_LOOPSTACK);
        break;
    case MidiEvent::ST_TRACK_LOOPSTACK_END:
    case MidiEvent::ST_TRACK_LOOPSTACK_BREAK:
        trackLoop->dstLoopStackId = LOOP_STACK_NO_ID;
        setLoopStackEnd(loopState, trackLoop, abs_position, LOCAL_LOOPSTACK);
        break;

    default:
        break;
    }
}

#endif /* BW_MIDISEQ_LOOP_IMPL_HPP */
