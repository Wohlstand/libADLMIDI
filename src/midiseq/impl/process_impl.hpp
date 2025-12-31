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
#ifndef BW_MIDISEQ_PROCESS_IMPL_HPP
#define BW_MIDISEQ_PROCESS_IMPL_HPP

#include <cstring>

#include "../midi_sequencer.hpp"
#include "common.hpp"



void BW_MidiSequencer::handleEvent(size_t track, const BW_MidiSequencer::MidiEvent &evt, int32_t &status)
{
    size_t length, midCh, loopStackLevel;
    const uint8_t *datau;
    const char *data;
    int loopsNum;
    DuratedNote *note;
    LoopStackEntry *loopEntryP;
    LoopState *loop = NULL;
    bool loopHasId;
    MidiTrackState &tk = m_trackState[track];

    if(m_deviceMask != Device_ANY && (m_deviceMask & tk.deviceMask) == 0)
        return; // Ignore this track completely

    if(track == 0 && m_smfFormat < 2 && evt.type == MidiEvent::T_SPECIAL &&
       (evt.subtype == MidiEvent::ST_TEMPOCHANGE || evt.subtype == MidiEvent::ST_TIMESIGNATURE))
    {
        /* never reject track 0 timing events on SMF format != 2 */
    }
    else
    {
        if(m_trackSolo != ~static_cast<size_t>(0) && track != m_trackSolo)
            return;

        if(tk.disabled)
            return;
    }

    if(m_interface->onEvent && evt.type < 0x100 && evt.subtype < 0x100)
    {
        // Only standard MIDI events will be reported, built-in events (>=0x100) will remain private

        if(evt.data_block.size > 0)
            m_interface->onEvent(m_interface->onEvent_userData,
                                 evt.type, evt.subtype, evt.channel,
                                 getData(evt.data_block), evt.data_block.size);
        else
            m_interface->onEvent(m_interface->onEvent_userData,
                                 evt.type, evt.subtype, evt.channel,
                                 evt.data_loc, evt.data_loc_size);
    }

    if(evt.type == MidiEvent::T_SYSCOMSNGSEL || evt.type == MidiEvent::T_SYSCOMSPOSPTR)
        return;

    midCh = evt.channel;

    if(evt.type < 0x10)
    {
        if(m_interface->rt_currentDevice)
            midCh += m_interface->rt_currentDevice(m_interface->rtUserData, track);

        status = evt.type;
    }

    if(tk.state.track_channel != midCh)
        tk.state.track_channel = midCh; // Remember track's current channel if changed

    switch(evt.type)
    {
    case MidiEvent::T_SYSEX:
    case MidiEvent::T_SYSEX2: // Handle SysEx
        if(m_interface->rt_systemExclusive)
            m_interface->rt_systemExclusive(m_interface->rtUserData, getData(evt.data_block), evt.data_block.size);
        return;

    case MidiEvent::T_SPECIAL:
    {
        if(m_interface->rt_metaEvent && evt.subtype < 0x100) // Meta event hook
        {
            if(evt.data_loc_size > 0)
                m_interface->rt_metaEvent(m_interface->rtUserData, evt.subtype, evt.data_loc, evt.data_loc_size);

            if(evt.data_block.size > 0)
                m_interface->rt_metaEvent(m_interface->rtUserData, evt.subtype, getData(evt.data_block), evt.data_block.size);
        }

        switch(evt.subtype)
        {
        case MidiEvent::ST_ENDTRACK:
            status = -1;
            return;
        case MidiEvent::ST_TEMPOCHANGE:
            tempo_mul(&m_tempo, &m_invDeltaTicks, readBEint(evt.data_loc, evt.data_loc_size));
            return;

        case MidiEvent::ST_DEVICESWITCH:
            length = evt.data_block.size > 0 ? evt.data_block.size : static_cast<size_t>(evt.data_loc_size);
            datau = evt.data_block.size > 0 ? getData(evt.data_block) : evt.data_loc;
            data = (length ? reinterpret_cast<const char *>(datau) : "\0\0\0\0\0\0\0\0");

            if(m_interface->onDebugMessage)
                m_interface->onDebugMessage(m_interface->onDebugMessage_userData, "Switching another device: %.*s", length, data);

            if(m_interface->rt_deviceSwitch)
                m_interface->rt_deviceSwitch(m_interface->rtUserData, track, data, length);
            return;

        case MidiEvent::ST_MARKER:
        case MidiEvent::ST_TEXT:
        case MidiEvent::ST_LYRICS:
        case MidiEvent::ST_TIMESIGNATURE:
        case MidiEvent::ST_SMPTEOFFSET:
            // Do nothing! :-P
            return;


        // NON-STANDARD EVENTS (>= 0x100)


        case MidiEvent::ST_CALLBACK_TRIGGER:
#if 0 /* Print all callback triggers events */
            if(m_interface->onDebugMessage)
                m_interface->onDebugMessage(m_interface->onDebugMessage_userData, "Callback Trigger: %02X", evt.data[0]);
#endif
            if(m_triggerHandler)
                m_triggerHandler(m_triggerUserData, static_cast<unsigned>(evt.data_loc[0]), track);
            return;

        case MidiEvent::ST_RAWOPL:
            if(m_interface->rt_rawOPL)
                m_interface->rt_rawOPL(m_interface->rtUserData, evt.data_loc[0], evt.data_loc[1]);
            return;


        case MidiEvent::ST_SONG_BEGIN_HOOK:
            if(m_interface->onSongStart)
                m_interface->onSongStart(m_interface->onSongStart_userData);
            return;


        case MidiEvent::ST_LOOPSTART:
            if(m_loopEnabled && !m_loop.invalidLoop)
                m_loop.caughtStart = true;
            return;

        case MidiEvent::ST_LOOPEND:
            if(m_loopEnabled && !m_loop.invalidLoop)
                m_loop.caughtEnd = true;
            return;


        case MidiEvent::ST_TRACK_LOOPSTACK_BEGIN:
        case MidiEvent::ST_TRACK_LOOPSTACK_BEGIN_ID:
            loop = &tk.loop;
            /* fallthrough */
        case MidiEvent::ST_LOOPSTACK_BEGIN:
        case MidiEvent::ST_LOOPSTACK_BEGIN_ID:
            if(!loop)
                loop = &m_loop;

            if(m_loopEnabled && !loop->invalidLoop)
            {
                if(loop->skipStackStart)
                {
                    loop->skipStackStart = false;
                    return;
                }

                loopHasId = evt.subtype == MidiEvent::ST_LOOPSTACK_BEGIN_ID || evt.subtype == MidiEvent::ST_TRACK_LOOPSTACK_BEGIN_ID;

                loopsNum = evt.data_loc[0];
                loopStackLevel = static_cast<size_t>(loop->stackLevel + 1);

                while(loopStackLevel >= loop->stackDepth && loop->stackDepth < LoopState::stackDepthMax - 1)
                {
                    loopEntryP = &loop->stack[loop->stackDepth++];
                    loopEntryP->loops = loopsNum;
                    loopEntryP->infinity = (loopsNum == 0);
                    loopEntryP->start = 0;
                    loopEntryP->end = 0;
                    loopEntryP->id = LOOP_STACK_NO_ID;
                }

                loopEntryP = &loop->stack[loopStackLevel];
                loopEntryP->loops = static_cast<int>(loopsNum);
                loopEntryP->infinity = (loopsNum == 0);
                loopEntryP->id = loopHasId ? evt.data_loc[1] : LOOP_STACK_NO_ID;
                loop->caughtStackStart = true;
            }
            return;


        case MidiEvent::ST_TRACK_LOOPSTACK_END_ID:
        case MidiEvent::ST_TRACK_LOOPSTACK_END:
            loop = &tk.loop;
            /* fallthrough */
        case MidiEvent::ST_LOOPSTACK_END:
        case MidiEvent::ST_LOOPSTACK_END_ID:
            if(!loop)
                loop = &m_loop;

            if(m_loopEnabled && !loop->invalidLoop)
            {
                loopHasId = evt.subtype == MidiEvent::ST_LOOPSTACK_END_ID || evt.subtype == MidiEvent::ST_TRACK_LOOPSTACK_END_ID;
                loop->caughtStackEnd = true;
                loop->dstLoopStackId = loopHasId ? evt.data_loc[0] : LOOP_STACK_NO_ID;
            }
            return;


        case MidiEvent::ST_TRACK_LOOPSTACK_BREAK:
            loop = &tk.loop;
            /* fallthrough */
        case MidiEvent::ST_LOOPSTACK_BREAK:
            if(!loop)
                loop = &m_loop;

            if(m_loopEnabled && !loop->invalidLoop)
                loop->caughtStackBreak = true;
            return;


        case MidiEvent::ST_TRACK_BRANCH_TO:
            loop = &tk.loop;
            /* fallthrough */
        case MidiEvent::ST_BRANCH_TO:
            if(!loop)
                loop = &m_loop;

            if(m_loopEnabled && !loop->invalidLoop)
            {
                loop->caughtBranchJump = true;
                loop->dstBranchId = readLEint16(evt.data_loc, evt.data_loc_size);
            }
            return;

        case MidiEvent::ST_ENABLE_RESTORE_CC_ON_LOOP:
            tk.state.cc_to_restore[evt.data_loc[0]] = 1;
            m_stateRestoreSetup |= TRACK_RESTORE_CC;
            break;

        case MidiEvent::ST_DISABLE_RESTORE_CC_ON_LOOP:
            tk.state.cc_to_restore[evt.data_loc[0]] = 0;
            break;

        case MidiEvent::ST_ENABLE_NOTEOFF_ON_LOOP:
            m_stateRestoreSetup |= TRACK_RESTORE_NOTEOFFS;
            break;

        case MidiEvent::ST_DISABLE_NOTEOFF_ON_LOOP:
            m_stateRestoreSetup &= ~TRACK_RESTORE_NOTEOFFS;
            break;

        case MidiEvent::ST_ENABLE_RESTORE_PATCH_ON_LOOP:
            m_stateRestoreSetup |= TRACK_RESTORE_PATCH;
            break;

        case MidiEvent::ST_DISABLE_RESTORE_PATCH_ON_LOOP:
            m_stateRestoreSetup &= ~TRACK_RESTORE_PATCH;
            break;

        case MidiEvent::ST_ENABLE_RESTORE_WHEEL_ON_LOOP:
            m_stateRestoreSetup |= TRACK_RESTORE_WHEEL;
            break;

        case MidiEvent::ST_DISABLE_RESTORE_WHEEL_ON_LOOP:
            m_stateRestoreSetup &= ~TRACK_RESTORE_WHEEL;
            break;

        case MidiEvent::ST_ENABLE_RESTORE_NOTEAFTERTOUCH_ON_LOOP:
            m_stateRestoreSetup |= TRACK_RESTORE_NOTE_ATT;
            break;

        case MidiEvent::ST_DISABLE_RESTORE_NOTEAFTERTOUCH_ON_LOOP:
            m_stateRestoreSetup &= ~TRACK_RESTORE_NOTE_ATT;
            break;

        case MidiEvent::ST_ENABLE_RESTORE_CHANAFTERTOUCH_ON_LOOP:
            m_stateRestoreSetup |= TRACK_RESTORE_CHAN_ATT;
            break;

        case MidiEvent::ST_DISABLE_RESTORE_CHANAFTERTOUCH_ON_LOOP:
            m_stateRestoreSetup &= ~TRACK_RESTORE_CHAN_ATT;
            break;

        case MidiEvent::ST_ENABLE_RESTORE_ALL_CC_ON_LOOP:
            m_stateRestoreSetup |= TRACK_RESTORE_ALL_CC;
            break;

        case MidiEvent::ST_DISABLE_RESTORE_ALL_CC_ON_LOOP:
            m_stateRestoreSetup &= ~TRACK_RESTORE_ALL_CC;
            break;
        }

        return;
    }

    case MidiEvent::T_NOTEOFF: // Note off
        if(evt.channel < 16 && m_channelDisable[evt.channel])
            return; // Disabled channel

        if(m_interface->rt_noteOff)
            m_interface->rt_noteOff(m_interface->rtUserData, static_cast<uint8_t>(midCh), evt.data_loc[0]);

        if(m_interface->rt_noteOffVel)
            m_interface->rt_noteOffVel(m_interface->rtUserData, static_cast<uint8_t>(midCh), evt.data_loc[0], evt.data_loc[1]);

        return;

    case MidiEvent::T_NOTEON:  // Note on
        if(evt.channel < 16 && m_channelDisable[evt.channel])
            return; // Disabled channel
        m_interface->rt_noteOn(m_interface->rtUserData, static_cast<uint8_t>(midCh), evt.data_loc[0], evt.data_loc[1]);
        return;

    case MidiEvent::T_NOTEON_DURATED: // Note on with duration
        if(duratedNoteAlloc(track, &note)) // Do call true Note ON only when note OFF is successfully added into the list!
        {
            note->channel = evt.channel;
            note->note = evt.data_loc[0];
            note->velocity = evt.data_loc[1];
            note->ttl = readBEint(evt.data_loc + 2, 3);
            m_interface->rt_noteOn(m_interface->rtUserData, static_cast<uint8_t>(midCh), evt.data_loc[0], evt.data_loc[1]);
        }
        return;

    case MidiEvent::T_NOTETOUCH: // Note touch
        m_interface->rt_noteAfterTouch(m_interface->rtUserData, static_cast<uint8_t>(midCh), evt.data_loc[0], evt.data_loc[1]);
        tk.state.reserve_note_att[evt.data_loc[0]] = evt.data_loc[1];
        return;

    case MidiEvent::T_CTRLCHANGE: // Controller change
        m_interface->rt_controllerChange(m_interface->rtUserData, static_cast<uint8_t>(midCh), evt.data_loc[0], evt.data_loc[1]);
        if(evt.data_loc[0] < 102)
            tk.state.cc_values[evt.data_loc[0]] = evt.data_loc[1];
        return;

    case MidiEvent::T_PATCHCHANGE: // Patch change
        m_interface->rt_patchChange(m_interface->rtUserData, static_cast<uint8_t>(midCh), evt.data_loc[0]);
        tk.state.reserve_patch = evt.data_loc[0];
        return;

    case MidiEvent::T_CHANAFTTOUCH: // Channel after-touch
        m_interface->rt_channelAfterTouch(m_interface->rtUserData, static_cast<uint8_t>(midCh), evt.data_loc[0]);
        tk.state.reserve_channel_att = evt.data_loc[0];
        return;

    case MidiEvent::T_WHEEL: // Wheel/pitch bend
        m_interface->rt_pitchBend(m_interface->rtUserData, static_cast<uint8_t>(midCh), evt.data_loc[1], evt.data_loc[0]);
        tk.state.reserve_wheel[0] = evt.data_loc[0];
        tk.state.reserve_wheel[1] = evt.data_loc[1];
        return;

    default:
        return;
    }
}

void BW_MidiSequencer::processDuratedNotes(size_t track, int32_t &status)
{
    DuratedNotesCache &cache = m_trackState[track].duratedNotes;

    if(cache.notes_count == 0)
        return; // Nothing to do!

    for(size_t i = 0; i < cache.notes_count; )
    {
        if(cache.notes[i].ttl <= 0)
        {
            DuratedNote *n = &cache.notes[i];

            if(m_interface->rt_noteOff)
                m_interface->rt_noteOff(m_interface->rtUserData, n->channel, n->note);

            if(m_interface->rt_noteOffVel)
                m_interface->rt_noteOffVel(m_interface->rtUserData, n->channel, n->note, n->velocity);

            status = MidiEvent::T_NOTEOFF;

            duratedNotePop(track, i);
        }
        else
            ++i;
    }
}

void BW_MidiSequencer::handleLoopStart(LoopRuntimeState &state, LoopState &loop, Position::TrackInfo &tk, bool glob)
{
    if(loop.caughtStackStart)
    {
        if(glob && m_interface->onloopStart && (m_loopStartTime >= tk.pos->time)) // Loop Start hook
            m_interface->onloopStart(m_interface->onloopStart_userData);

        state.numStackLoopStarts++;
        loop.caughtStackStart = false;
    }

    if(loop.caughtStackBreak)
    {
        state.numStackLoopBreaks++;
        loop.caughtStackBreak = false;
    }
}

bool BW_MidiSequencer::handleLoopEnd(LoopRuntimeState &state, LoopState &loop, Position::TrackInfo &tk, bool glob)
{
    if(loop.caughtBranchJump)
    {
        ++state.numBranchJumps;
        loop.caughtBranchJump = false;

        if(glob)
            state.doLoopJump = true;

        return true;
    }

    if((glob && loop.caughtEnd) || loop.isStackEnd())
    {
        if(loop.caughtStackEnd)
        {
            loop.caughtStackEnd = false;
            state.numStackLoopEnds++;
            state.stackLoopEndsTime = tk.pos->time;
        }

        if(glob)
            state.doLoopJump = true;

        return true; // Stop event handling on catching loopEnd event!
    }

    return false;
}

bool BW_MidiSequencer::processLoopPoints(LoopRuntimeState &state, LoopState &loop, bool glob, size_t tk, const Position &pos)
{
    if(state.numStackLoopStarts > 0)
    {
        while(state.numStackLoopStarts > 0)
        {
            loop.stackUp();
            LoopStackEntry &s = loop.getCurStack();

            if(glob)
                s.startPosition = pos;
            else // Store info for only one track
                s.startPosition.assignOneTrack(&pos, tk);

            state.numStackLoopStarts--;
        }

        return true;
    }

    if(state.numStackLoopBreaks > 0)
    {
        while(state.numStackLoopBreaks > 0)
        {
            LoopStackEntry &s = loop.getCurStack();
            s.loops = 0;
            s.infinity = false;
            // Quit the loop
            loop.stackDown();
            state.numStackLoopBreaks--;
        }
    }

    if(state.numBranchJumps > 0)
        return jumpToBranch(glob ? BRANCH_GLOBAL_TRACK : tk, loop.dstBranchId);

    if(state.numStackLoopEnds > 0)
    {
        while(state.numStackLoopEnds > 0)
        {
            LoopStackEntry *s = &loop.getCurStack();

            // We should change stack if ID is not matching
            if(loop.dstLoopStackId != LOOP_STACK_NO_ID && s->id != loop.dstLoopStackId)
            {
                do
                {
                    loop.stackDown();
                    s = &loop.getCurStack();
                } while(loop.stackLevel >= 0 && s->id != loop.dstLoopStackId);
            }

            if(s->infinity)
            {
                if(glob && m_interface->onloopEnd && (m_loopEndTime >= state.stackLoopEndsTime)) // Loop End hook
                {
                    m_interface->onloopEnd(m_interface->onloopEnd_userData);
                    if(m_loopHooksOnly) // Stop song on reaching loop end
                    {
                        m_atEnd = true; // Don't handle events anymore
                        m_currentPosition.wait += m_postSongWaitDelay; // One second delay until stop playing
                    }
                }

                jumpToPosition(glob ? BRANCH_GLOBAL_TRACK : tk, &s->startPosition);
                loop.skipStackStart = true;
                return true;
            }
            else
            if(s->loops >= 0)
            {
                s->loops--;
                if(s->loops > 0)
                {
                    jumpToPosition(glob ? BRANCH_GLOBAL_TRACK : tk, &s->startPosition);
                    loop.skipStackStart = true;
                    return true;
                }
                else
                {
                    // Quit the loop
                    loop.stackDown();
                }
            }
            else
            {
                // Quit the loop
                loop.stackDown();
            }

            state.numStackLoopEnds--;
        }
    }

    return false;
}

void BW_MidiSequencer::restoreSongState()
{
    for(size_t track = 0; track < m_tracksCount; track++)
    {
        std::memcpy(&m_trackState[track].state, &m_currentPosition.track[track].state, sizeof(TrackStateSaved));
        restoreTrackState(track);
    }
}

void BW_MidiSequencer::restoreTrackState(size_t track)
{
    MidiTrackState &state = m_trackState[track];
    uint8_t chan = state.state.track_channel;

    if((m_stateRestoreSetup & TRACK_RESTORE_NOTEOFFS) != 0)
    {
        if((m_format == Format_MIDI && m_smfFormat == 0) || m_format == Format_XMIDI)
        {
            for(uint8_t c = 0; c < 16; c++)
                m_interface->rt_controllerChange(m_interface->rtUserData, c, 123, 0);
        }
        else if(chan != 0xFF)
            m_interface->rt_controllerChange(m_interface->rtUserData, chan, 123, 0);

        state.duratedNotes.notes_count = 0;
    }

    if((m_stateRestoreSetup & TRACK_RESTORE_ALL_CC) != 0)
    {
        for(size_t i = 0; i < 102; ++i)
        {
            if(state.state.cc_values[i] <= 127)
                m_interface->rt_controllerChange(m_interface->rtUserData, chan, i, state.state.cc_values[i]);
        }
    }
    else if((m_stateRestoreSetup & TRACK_RESTORE_CC) != 0)
    {
        for(size_t i = 0; i < 102; ++i)
        {
            if(state.state.cc_to_restore[i] && state.state.cc_values[i] <= 127)
                m_interface->rt_controllerChange(m_interface->rtUserData, chan, i, state.state.cc_values[i]);
        }
    }

    if((m_stateRestoreSetup & TRACK_RESTORE_PATCH) != 0 && state.state.reserve_patch <= 127)
        m_interface->rt_patchChange(m_interface->rtUserData, chan, state.state.reserve_patch);

    if((m_stateRestoreSetup & TRACK_RESTORE_WHEEL) != 0 && state.state.reserve_wheel[0] <= 127)
        m_interface->rt_pitchBend(m_interface->rtUserData, chan, state.state.reserve_wheel[1], state.state.reserve_wheel[0]);

    if((m_stateRestoreSetup & TRACK_RESTORE_NOTE_ATT) != 0)
    {
        for(size_t i = 0; i < 128; ++i)
        {
            if(state.state.reserve_note_att[i] <= 127)
                m_interface->rt_noteAfterTouch(m_interface->rtUserData, chan, i, state.state.reserve_note_att[i]);
        }
    }

    if((m_stateRestoreSetup & TRACK_RESTORE_CHAN_ATT) != 0 && state.state.reserve_channel_att <= 127)
        m_interface->rt_channelAfterTouch(m_interface->rtUserData, chan, state.state.reserve_channel_att);
}

void BW_MidiSequencer::jumpToPosition(size_t track, const Position *pos)
{
    if(track == BRANCH_GLOBAL_TRACK)
    {
        m_currentPosition = *pos;
        restoreSongState();
    }
    else
    {
        m_currentPosition.track[track] = pos->track[0];
        // Reset the time (lesser evil than time going to infinite!)
        m_currentPosition.absTickPosition = pos->absTickPosition;
        m_currentPosition.absTimePosition = pos->absTimePosition;
        std::memcpy(&m_trackState[track].state, &m_currentPosition.track[track].state, sizeof(TrackStateSaved));
        restoreTrackState(track);
    }
}

bool BW_MidiSequencer::jumpToBranch(uint32_t dstTrack, uint16_t dstBranch)
{
    if(dstTrack != BRANCH_GLOBAL_TRACK && dstTrack >= m_currentPosition.track.size())
        return false; // Invalid query!

    for(std::vector<BranchEntry>::iterator it = m_branches.begin(); it != m_branches.end(); ++it)
    {
        BranchEntry &e = *it;
        if(e.id == dstBranch && e.track == dstTrack)
        {
            jumpToPosition(dstTrack, &e.offset);
            return true;
        }
    }

    return false;
}

bool BW_MidiSequencer::processEvents(bool isSeek)
{
    if(m_currentPosition.track.size() == 0)
        m_atEnd = true; // No MIDI track data to play

    if(m_atEnd)
        return false;   // No more events in the queue

    m_loop.caughtEnd = false;
    const size_t        trackCount = m_currentPosition.track.size();
    const Position      rowBeginPosition(m_currentPosition);
    LoopRuntimeState    loopState, loopStateLoc;
    Tempo_t t;

    std::memset(&loopState, 0, sizeof(loopState));

#ifdef DEBUG_TIME_CALCULATION
    double maxTime = 0.0;
#endif

    for(size_t tk = 0; tk < trackCount; ++tk)
    {
        Position::TrackInfo &track = m_currentPosition.track[tk];
        MidiTrackQueue::iterator end = m_trackData[tk].end();
        MidiTrackState &trackState = m_trackState[tk];
        LoopState &trackLoop = trackState.loop;

        std::memset(&loopStateLoc, 0, sizeof(loopStateLoc));

        // Process note-OFFs
        processDuratedNotes(tk, track.lastHandledEvent);

        if((track.lastHandledEvent >= 0) && (track.delay <= 0))
        {
            // Check is an end of track has been reached
            if(track.pos == end)
            {
                track.lastHandledEvent = -1;
                break;
            }

            // Handle event
            for(size_t i = track.pos->events_begin; i < track.pos->events_end; ++i)
            {
                const MidiEvent &evt = m_eventBank[i];
#ifdef ENABLE_BEGIN_SILENCE_SKIPPING
                if(!m_currentPosition.began && (evt.type == MidiEvent::T_NOTEON))
                    m_currentPosition.began = true;
#endif
                if(isSeek && (evt.type == MidiEvent::T_NOTEON || evt.type == MidiEvent::T_NOTEON_DURATED))
                    continue;

                handleEvent(tk, evt, track.lastHandledEvent);

                // Global non-stacked loop start
                if(m_loop.caughtStart)
                {
                    if(m_interface->onloopStart) // Loop Start hook
                        m_interface->onloopStart(m_interface->onloopStart_userData);

                    ++loopState.numGlobLoopStarts;
                    m_loop.caughtStart = false;
                }

                // Global stacked loop start
                handleLoopStart(loopState, m_loop, track, true);
                // Local stacked loop start
                handleLoopStart(loopStateLoc, trackLoop, track, false);

                if(handleLoopEnd(loopStateLoc, trackLoop, track, false))
                    break;

                if(handleLoopEnd(loopState, m_loop, track, true))
                    break;
            }

#ifdef DEBUG_TIME_CALCULATION
            if(maxTime < track.pos->time)
                maxTime = track.pos->time;
#endif
            // Read next event time (unless the track just ended)
            if(track.lastHandledEvent >= 0)
            {
                track.delay += track.pos->delay;
                ++track.pos;
            }

            // Register global loop start position
            if(loopState.numGlobLoopStarts > 0 && m_loopBeginPosition.absTimePosition <= 0.0)
                m_loopBeginPosition = rowBeginPosition;

            // Process local loop
            if(processLoopPoints(loopStateLoc, trackLoop, false, tk, rowBeginPosition))
                continue; // Done with this track for now

            if(loopState.doLoopJump)
                break;
        }
    }

#ifdef DEBUG_TIME_CALCULATION
    std::fprintf(stdout, "                              \r");
    std::fprintf(stdout, "Time: %10f; Audio: %10f\r", maxTime, m_currentPosition.absTimePosition);
    std::fflush(stdout);
#endif

    // Find a shortest delay from all track
    uint64_t shortestDelay = 0;
    bool     shortestDelayNotFound = true;

    for(size_t tk = 0; tk < trackCount; ++tk)
    {
        Position::TrackInfo &track = m_currentPosition.track[tk];
        DuratedNotesCache &timedNotes = m_trackState[tk].duratedNotes;

        // Normal events
        if((track.lastHandledEvent >= 0) && (shortestDelayNotFound || track.delay < shortestDelay))
        {
            shortestDelay = track.delay;
            shortestDelayNotFound = false;
        }

        // Note events with duration
        for(size_t i = 0; i < timedNotes.notes_count; ++i)
        {
            DuratedNote &n = timedNotes.notes[i];
            if(n.ttl <= 0)
            {
                shortestDelay = 0; // Just zero!
                shortestDelayNotFound = false;
            }
            else if(shortestDelayNotFound || static_cast<uint64_t>(n.ttl) < shortestDelay)
            {
                shortestDelay = n.ttl; // Extra tick
                shortestDelayNotFound = false;
            }
        }
    }

    // Schedule the next playevent to be processed after that delay
    for(size_t tk = 0; tk < trackCount; ++tk)
    {
        m_currentPosition.track[tk].delay -= shortestDelay;
        duratedNoteTick(tk, shortestDelay);
    }

    tempo_mul(&t, &m_tempo, shortestDelay);

#ifdef ENABLE_BEGIN_SILENCE_SKIPPING
    if(m_currentPosition.began)
#endif
    {
        m_currentPosition.wait += tempo_get(&t);
        m_currentPosition.absTickPosition += shortestDelay;
    }

    if(loopState.numGlobLoopStarts > 0 && m_loopBeginPosition.absTimePosition <= 0.0)
        m_loopBeginPosition = rowBeginPosition;

    if(processLoopPoints(loopState, m_loop, true, 0, rowBeginPosition))
        return true; // When loop jump happen, quit the function

    if(shortestDelayNotFound || m_loop.caughtEnd)
    {
        if(m_interface->onloopEnd) // Loop End hook
            m_interface->onloopEnd(m_interface->onloopEnd_userData);

        // Loop if song end or loop end point has reached
        m_loop.caughtEnd         = false;

        if(!m_loopEnabled || (shortestDelayNotFound && m_loop.loopsCount >= 0 && m_loop.loopsLeft < 1) || m_loopHooksOnly)
        {
            m_atEnd = true; // Don't handle events anymore
            m_currentPosition.wait += m_postSongWaitDelay; // One second delay until stop playing
            return true; // We have caugh end here!
        }

        if(m_loop.temporaryBroken)
        {
            jumpToPosition(BRANCH_GLOBAL_TRACK, &m_trackBeginPosition);
            m_loop.temporaryBroken = false;
        }
        else if(m_loop.loopsCount < 0 || m_loop.loopsLeft >= 1)
        {
            jumpToPosition(BRANCH_GLOBAL_TRACK, &m_loopBeginPosition);
            if(m_loop.loopsCount >= 1)
                m_loop.loopsLeft--;
        }
    }

    return true; // Has events in queue
}

#endif /* BW_MIDISEQ_PROCESS_IMPL_HPP */
