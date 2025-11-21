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
#ifndef BW_MIDISEQ_READ_HMI_IMPL_HPP
#define BW_MIDISEQ_READ_HMI_IMPL_HPP

#include <cstring>

#include "../midi_sequencer.hpp"
#include "common.hpp"

#define HMI_OFFSET_DIVISION         0xD4
#define HMI_OFFSET_TRACKS_COUNT     0xE4
#define HMI_OFFSET_TRACK_DIR        0xE8

#define HMI_OFFSET_TRACK_LEN            0x04
#define HMI_OFFSET_TRACK_DATA_OFFSET    0x57
#define HMI_OFFSET_TRACK_DEVICES        0x99

#define HMI_SIZE_TRACK_DIR_HEAD     4

#define HMI_MAX_TRACK_DEVICES    8

#define HMP_MAX_TRACK_DEVICES    5
#define HMP_OFFSET_TRACK_DATA   12

static const uint8_t s_hmi_evtSize[16] =
{
//  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
    0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 1, 1, 2, 0
};

static const uint8_t s_hmi_evtSizeCtrl[16] =
{
//  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
    0, 1, 2, 1, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2
};

/**
 * @brief HMI-specific meta-event that is placed instead of special controllers like loop points and branches
 */
enum HMIEventTypes
{
    S_HMI_SPECIAL = 0xFE
};

/**
 * @brief Sub-types for HMI-specific meta-events
 */
enum HMIEventSubTypes
{
    //! Place the tag of the track-local or song-global branch with ID attached
    ST_HMI_NEW_BRANCH           = 0x10,
    /****************************************************************************
     * Data format for the new branch event:                                    *
     * = Has dynamic size, minimum size is 7 bytes =                            *
     ****************************************************************************
     * - 2 bytes:                                                               *
     *   - [x]yyy yyyy zzzz zzzz - 16-bit little-endian number                  *
     *     HMI SOS' Internal branch ID value built with next parts              *
     *     x = 0 - local branch, 1 - global branch                              *
     *     y = ID of the branch                                                 *
     *     z = 0 is normal branch, 1 a branch spawned by the loop start point   *
     * - 1 byte:                                                                *
     *   - The "N" value of length of extra data                                *
     * - N bytes                                                                *
     *   - Extra data                                                           *
     * - 4 bytes                                                                *
     *   - Tail of extra data (Possibly a track offset of this event)           *
     ****************************************************************************/

    //! Jump to track-local branch of ID
    ST_HMI_JUMP_TO_LOC_BRANCH   = 0x11,
    /****************************************************************************
     * Data format for the jump to track-local branch of ID event:              *
     * = Totally always 6 bytes =                                               *
     ****************************************************************************
     * - 2 bytes:                                                               *
     *   - [x]yyy yyyy zzzz zzzz - 16-bit little-endian number                  *
     *     HMI SOS' Internal branch ID value built with next parts              *
     *     x = 0 - local branch, 1 - global branch                              *
     *     y = ID of the branch                                                 *
     *     z = 0 is normal branch, 1 a branch spawned by the loop start point   *
     * - 4 bytes                                                                *
     *   - Extra data (Possibly a track offset of this event)                   *
     ****************************************************************************/

    //! Track-local Loop start point
    ST_HMI_TRACK_LOOP_START     = 0x12,
    /****************************************************************************
     * Track-local loop start point                                             *
     * = Totally always 2 bytes =                                               *
     ****************************************************************************
     * - 1 byte:                                                                *
     *   - Number of loops without 1 (12 means 13 loops, 0xFF means infinite)   *
     * - 1 byte:                                                                *
     *   - Duplicate of previous byte                                           *
     ****************************************************************************/

    //! Track local loop end point
    ST_HMI_TRACK_LOOP_END       = 0x13,
    /****************************************************************************
     * Track-local loop end point                                               *
     * = Totally always 6 bytes =                                               *
     ****************************************************************************
     * - 2 bytes:                                                               *
     *   - The ID of the branch spawned by the related loop start event         *
     * - 4 byte:                                                                *
     *   - Extra data (Possibly a track offset of this event)                   *
     ****************************************************************************/

    //! Soong-global loop start point
    ST_HMI_GLOB_LOOP_START      = 0x14,
    /****************************************************************************
     * Song-global loop start point                                             *
     * = Totally always 2 bytes =                                               *
     ****************************************************************************
     * - 1 byte:                                                                *
     *   - Number of loops without 1 (12 means 13 loops, 0xFF means infinite)   *
     * - 1 byte:                                                                *
     *   - Duplicate of previous byte                                           *
     ****************************************************************************/

    //! Soong-global loop end point
    ST_HMI_GLOB_LOOP_END        = 0x15,
    /****************************************************************************
     * Song-global loop end point                                               *
     * = Totally always 6 bytes =                                               *
     ****************************************************************************
     * - 2 bytes:                                                               *
     *   - The ID of the branch spawned by the related loop start event         *
     * - 4 byte:                                                                *
     *   - Extra data (Possibly a track offset of this event)                   *
     ****************************************************************************/

    //! Jump to song-global branch of ID
    ST_HMI_JUMP_TO_GLOB_BRANCH  = 0x16
    /****************************************************************************
     * Data format for the jump to song-global branch of ID event:              *
     * = Totally always 2 bytes =                                               *
     ****************************************************************************
     * - 2 bytes:                                                               *
     *   - [x]yyy yyyy zzzz zzzz - 16-bit little-endian number                  *
     *     HMI SOS' Internal branch ID value built with next parts              *
     *     x = 0 - local branch, 1 - global branch                              *
     *     y = ID of the branch                                                 *
     *     z = 0 is normal branch, 1 a branch spawned by the loop start point   *
     ****************************************************************************/
};

/**
 * @brief The HMI specific MIDI controllers
 */
enum HMIController
{
    //! Enable controller restaration when branching and looping
    HMI_CC_RESTORE_ENABLE       = 103,
    //! Disable controller restaration when branching and looping
    HMI_CC_RESTORE_DISABLE      = 104,

    //! Lock/unlock the channel from stealing
    HMI_CC_LOCK_CHANNEL         = 106,
    //! Set the channel priority
    HMI_CC_SET_CH_PRIORITY      = 107,

    //! Sets the location of local branch of ID
    HMI_CC_SET_LOCAL_BRANCH     = 108,
    //! Jump to local branch location of ID
    HMI_CC_JUMP_TO_LOC_BRANCH   = 109,

    //! Global loop start point
    HMI_CC_GLOB_LOOP_START      = 110,
    //! Global loop end
    HMI_CC_GLOB_LOOP_END        = 111,

    //! Sets the location of global branch of ID
    HMI_CC_SET_GLOBAL_BRANCH    = 113,
    //! Jump to global branch location of ID
    HMI_CC_JUMP_TO_GLOB_BRANCH  = 114,

    //! Local loop start point (loop inside the same track)
    HMI_CC_LOCAL_LOOP_START     = 116,
    //! Local loop end point (loop inside the same track)
    HMI_CC_LOCAL_LOOP_END       = 117,

    //! Callback Trigger
    HMI_CC_CALLBACK_TRIGGER     = 119
};

/**
 * @brief Controllers specific to old HMP format
 */
enum HMPController
{
    //! Disable controller restore
    HMP_CC_DISABLE_CC_RESTORE   = 103,
    //! Enable controller restore
    HMP_CC_ENABLE_CC_RESTORE    = 104,
    //! Set the loops count to branch by taking 6'th byte at begin of controller entry
    HMP_CC_GLOB_LOOP_START      = 109,
    //! Dummy controller that holds loops count number
    HMP_CC_GLOB_LOOPS_COUNT     = 110,

    HMP_CC_GLOB_LOOP_END        = 111,
    HMP_CC_GLOB_LOOP_END2       = 112,

    HMP_CC_SET_GLOBAL_BRANCH    = 113,
    HMP_CC_JUMP_TO_GLOB_BRANCH  = 114,

    //! Set the loops count to branch by taking 6'th byte at begin of controller entry
    HMP_CC_LOC_LOOP_START       = 115,
    //! Dummy controller that holds loops count number
    HMP_CC_LOC_LOOPS_COUNT      = 116,

    //! Local loop end
    HMP_CC_LOCAL_LOOP_END       = 117,
    HMP_CC_LOCAL_LOOP_END2      = 118,

    //! Callback Trigger
    HMP_CC_CALLBACK_TRIGGER     = 119,
    //! (UNSURE) Set local branch location
    HMP_CC_SET_LOCAL_BRANCH     = 120,
    //! Jump to local branch
    HMP_CC_JUMP_TO_LOCLBRANCH   = 121
};

/**
 * @brief Sub-type of the on-loop state restore setup event
 */
enum HMISaveRestore
{
    //! Restore CC of ID on loop
    HMI_ONLOOP_RESTORE_CC       = 102,
    //! Execute note offs on loop
    HMI_ONLOOP_NOTEOFFS         = 103,
    //! Restore patch program on loop
    HMI_ONLOOP_RESTORE_PATCH    = 104,
    //! Restore pitch bend state on loop
    HMI_ONLOOP_RESTORE_WHEEL    = 105,
    //! Restore Note After-Touch state on loop
    HMI_ONLOOP_RESTORE_NOTEATT  = 106,
    //! Restore Channel After-Touch on loop
    HMI_ONLOOP_RESTORE_CHANATT  = 107,
    //! Restore all CC controllers state on loop
    HMI_ONLOOP_RESTORE_ALL_CC   = 115
};

bool BW_MidiSequencer::hmi_cc_restore_on_cvt(MidiEvent *event)
{
    event->type = MidiEvent::T_SPECIAL;
    event->data_loc_size = 0;

    switch(event->data_loc[1])
    {
    default:
        if(event->data_loc[1] > HMI_ONLOOP_RESTORE_CC)
            return false;

        event->subtype = MidiEvent::ST_ENABLE_RESTORE_CC_ON_LOOP;
        event->data_loc[0] = event->data_loc[1];
        event->data_loc_size = 1;
        break;
    case HMI_ONLOOP_NOTEOFFS:
        event->subtype = MidiEvent::ST_ENABLE_NOTEOFF_ON_LOOP;
        break;
    case HMI_ONLOOP_RESTORE_PATCH:
        event->subtype = MidiEvent::ST_ENABLE_RESTORE_PATCH_ON_LOOP;
        break;
    case HMI_ONLOOP_RESTORE_WHEEL:
        event->subtype = MidiEvent::ST_ENABLE_RESTORE_WHEEL_ON_LOOP;
        break;
    case HMI_ONLOOP_RESTORE_NOTEATT:
        event->subtype = MidiEvent::ST_ENABLE_RESTORE_NOTEAFTERTOUCH_ON_LOOP;
        break;
    case HMI_ONLOOP_RESTORE_CHANATT:
        event->subtype = MidiEvent::ST_ENABLE_RESTORE_CHANAFTERTOUCH_ON_LOOP;
        break;
    case HMI_ONLOOP_RESTORE_ALL_CC:
        event->subtype = MidiEvent::ST_ENABLE_RESTORE_ALL_CC_ON_LOOP;
        break;
    }

    return true;
}

bool BW_MidiSequencer::hmi_cc_restore_off_cvt(MidiEvent *event)
{
    event->type = MidiEvent::T_SPECIAL;
    event->data_loc_size = 0;

    switch(event->data_loc[1])
    {
    default:
        if(event->data_loc[1] > HMI_ONLOOP_RESTORE_CC)
            return false;

        event->subtype = MidiEvent::ST_DISABLE_RESTORE_CC_ON_LOOP;
        event->data_loc[0] = event->data_loc[1];
        event->data_loc_size = 1;
        break;
    case HMI_ONLOOP_NOTEOFFS:
        event->subtype = MidiEvent::ST_DISABLE_NOTEOFF_ON_LOOP;
        break;
    case HMI_ONLOOP_RESTORE_PATCH:
        event->subtype = MidiEvent::ST_DISABLE_RESTORE_PATCH_ON_LOOP;
        break;
    case HMI_ONLOOP_RESTORE_WHEEL:
        event->subtype = MidiEvent::ST_DISABLE_RESTORE_WHEEL_ON_LOOP;
        break;
    case HMI_ONLOOP_RESTORE_NOTEATT:
        event->subtype = MidiEvent::ST_DISABLE_RESTORE_NOTEAFTERTOUCH_ON_LOOP;
        break;
    case HMI_ONLOOP_RESTORE_CHANATT:
        event->subtype = MidiEvent::ST_DISABLE_RESTORE_CHANAFTERTOUCH_ON_LOOP;
        break;
    case HMI_ONLOOP_RESTORE_ALL_CC:
        event->subtype = MidiEvent::ST_DISABLE_RESTORE_ALL_CC_ON_LOOP;
        break;
    }

    return true;
}


bool BW_MidiSequencer::hmi_parseEvent(const HMIData &hmi_data, const HMITrackDir &d, FileAndMemReader &fr, MidiEvent &event, int &status)
{
    uint8_t byte, midCh, evType, subType, skipSize;
    size_t locSize;
    uint64_t length, duration;
    bool ok = false;

    std::memset(&event, 0, sizeof(event));
    event.isValid = 1;

    if(fr.tell() + 1 > d.end)
    {
        // When track doesn't ends on the middle of event data, it's must be fine
        event.type = MidiEvent::T_SPECIAL;
        event.subtype = MidiEvent::ST_ENDTRACK;
        status = -1;
        return true;
    }

    if(fr.read(&byte, 1, 1) != 1)
    {
        m_errorString.set("HMI/HMP: Failed to read first byte of the event\n");
        return false;
    }

    if(byte == MidiEvent::T_SYSEX || byte == MidiEvent::T_SYSEX2)
    {
        length = hmi_data.fReadVarLen(fr, d.end, ok);
        if(!ok || (fr.tell() + length > d.end))
        {
            m_errorString.append("HMI/HMP: Can't read SysEx event - Unexpected end of track data.\n");
            return false;
        }

#ifdef BWMIDI_DEBUG_HMI_PARSE
        printf("-- SysEx event\n");
        fflush(stdout);
#endif

        event.type = MidiEvent::T_SYSEX;
        insertDataToBankWithByte(event, m_dataBank, byte, fr, length);
    }
    else if(byte == MidiEvent::T_SPECIAL) // Special event FF
    {
        if(fr.read(&subType, 1, 1) != 1)
        {
            m_errorString.append("HMI/HMP: Failed to read event type!\n");
            event.isValid = 0;
            return false;
        }

#ifdef BWMIDI_DEBUG_HMI_PARSE
        printf("-- Special event 0x%02X\n", subType);
        fflush(stdout);
#endif

        if(subType == MidiEvent::ST_ENDTRACK)
        {
            ok = true;
            length = 0;
        }
        else
            length = hmi_data.fReadVarLen(fr, d.end, ok);

        if(!ok || (fr.tell() + length > d.end))
        {
            m_errorString.append("HMI/HMP: Can't read Special event - Unexpected end of track data.\n");
            return false;
        }

        event.type = byte;
        event.subtype = subType;

        if(event.subtype == MidiEvent::ST_TEMPOCHANGE)
        {
            if(length > 5)
            {
                m_errorString.append("HMI/HMP: Can't read one of special events - Too long event data (more than 5!).\n");
                return false;
            }

            event.data_loc_size = length;
            if(fr.read(event.data_loc, 1, length) != length)
            {
                m_errorString.append("HMI/HMP: Failed to read event's data (2).\n");
                return false;
            }
        }
        else if(subType == MidiEvent::ST_ENDTRACK)
        {
            status = -1; // Finalize track
        }
        else
        {
            m_errorString.append("HMI/HMP: Unsupported meta event sub-type.\n");
            return false;
        }
    }
    else if(byte == S_HMI_SPECIAL) // Special HMI-specific events
    {
        if(fr.read(&subType, 1, 1) != 1)
        {
            m_errorString.append("HMI/HMP: Failed to read event type!\n");
            return false;
        }
#ifdef BWMIDI_DEBUG_HMI_PARSE
        printf("-- HMI-specific tricky event 0x%02X\n", subType);
        fflush(stdout);
#endif

        event.type = byte;
        event.subtype = subType;

        switch(subType)
        {
        case ST_HMI_NEW_BRANCH: // 2 bytes, 1 byte, len(prev byte) + 4 bytes
            // Install the branch point, local or global
            event.data_loc_size = 2;

            if(fr.read(event.data_loc, 1, 2) != 2)
            {
                m_errorString.append("HMI/HMP: Failed to read branch location value!\n");
                return false;
            }

            if((event.data_loc[0] & 0x80) != 0) // Installs a global branch
            {
                event.type = MidiEvent::T_SPECIAL;
                event.subtype = MidiEvent::ST_BRANCH_LOCATION;
                event.data_loc[0] &= 0x7F; // Filter the highest bit
                event.data_loc_size = 2;
            }
            else
            {
                event.type = MidiEvent::T_SPECIAL;
                event.subtype = MidiEvent::ST_TRACK_BRANCH_LOCATION;
                event.data_loc_size = 2;
            }

            if(fr.read(&skipSize, 1, 1) != 1)
            {
                m_errorString.append("HMI/HMP: Failed to read branch location event length!\n");
                return false;
            }
            // Unknown data, possibly offset
            insertDataToBank(event, m_dataBank, fr, skipSize + 4);
            break;

        case ST_HMI_JUMP_TO_LOC_BRANCH: // 6 bytes
            // Jump to local branch
            event.type = MidiEvent::T_SPECIAL;
            event.subtype = MidiEvent::ST_TRACK_BRANCH_TO;
            event.data_loc_size = 2;

            if(fr.read(event.data_loc, 1, 2) != 2)
            {
                m_errorString.append("HMI/HMP: Failed to read jump to local branch data!\n");
                return false;
            }

            // Unknown data, possibly offset
            insertDataToBank(event, m_dataBank, fr, 4);
            break;

        case ST_HMI_TRACK_LOOP_START: // 2 bytes
            // Begin local in-track loop
            event.type = MidiEvent::T_SPECIAL;
            event.subtype = MidiEvent::ST_TRACK_LOOPSTACK_BEGIN;
            event.data_loc_size = 2;

            if(fr.read(event.data_loc, 1, 2) != 2)
            {
                m_errorString.append("HMI/HMP: Failed to read local loop start data!\n");
                return false;
            }

            if(event.data_loc[0] == 0xFF)
                event.data_loc[0] = 0; // 0xFF is "infinite" too
            else
                ++event.data_loc[0]; // Increase by 1

            break;

        case ST_HMI_TRACK_LOOP_END: // 6 bytes
            // End local loop
            event.type = MidiEvent::T_SPECIAL;
            event.subtype = MidiEvent::ST_TRACK_LOOPSTACK_END;
            event.data_loc_size = 0;
            // Unknown data, possibly offset
            insertDataToBank(event, m_dataBank, fr, 6);
            break;


        case ST_HMI_GLOB_LOOP_START: // 2 bytes
            // Begin global loop
            event.type = MidiEvent::T_SPECIAL;
            event.subtype = MidiEvent::ST_LOOPSTACK_BEGIN;
            event.data_loc_size = 2;

            if(fr.read(event.data_loc, 1, 2) != 2)
            {
                m_errorString.append("HMI/HMP: Failed to read global loop start data!\n");
                return false;
            }

            if(event.data_loc[0] == 0xFF)
                event.data_loc[0] = 0; // 0xFF is "infinite" too
            else
                ++event.data_loc[0]; // Increase by 1

            break;

        case ST_HMI_GLOB_LOOP_END: // 6 bytes
            // End global loop
            event.type = MidiEvent::T_SPECIAL;
            event.subtype = MidiEvent::ST_LOOPSTACK_END;
            event.data_loc_size = 0;
            // Unknown data, possibly offset
            insertDataToBank(event, m_dataBank, fr, 6);
            break;

        case ST_HMI_JUMP_TO_GLOB_BRANCH: // 2 bytes
            // Jump to global branch
            event.type = MidiEvent::T_SPECIAL;
            event.subtype = MidiEvent::ST_BRANCH_TO;
            event.data_loc_size = 2;

            if(fr.read(event.data_loc, 1, 2) != 2)
            {
                m_errorString.append("HMI/HMP: Failed to read unknown event value!\n");
                return false;
            }

            event.data_loc[0] &= 0x7F; // Filter the highest bit
            break;

        default:
            m_errorString.append("HMI/HMP: Unsupported unknown event type.\n");
            return false;
        }
    }
    else if(byte == 0xF1 || byte == 0xF4 || byte == 0xF5 || byte == 0xF6 || (byte >= 0xF8 && byte <= 0xFD))
    {
        m_errorString.append("HMI/HMP: Totally unsupported event byte!\n");
        return false;
    }
    else
    {
#ifdef BWMIDI_DEBUG_HMI_PARSE
        printf("Byte=0x%02X (off=%ld = 0x%lX) : ", byte, fr.tell(), fr.tell());
#endif
        // Any normal event (80..EF)
        if(byte < 0x80)
        {
            // Extra data for the same type of event and channel
            byte = static_cast<uint8_t>(status | 0x80);
            fr.seek(-1, FileAndMemReader::CUR);
        }

        status = byte;

        if(byte >= 0xF0)
        {
            locSize = s_hmi_evtSizeCtrl[byte & 0x0F];

            if(fr.tell() + locSize > d.end)
            {
                m_parsingErrorsString.appendFmt("HMI/HMP: Can't read event of type 0x%02X- Unexpected end of track data.\n", byte);
                return false;
            }

            if(locSize > 0)
                fr.read(event.data_loc, 1, locSize);

            event.type = byte;
            event.channel = 0;
            event.data_loc_size = locSize;
        }
        else
        {
            midCh = byte & 0x0F;
            evType = (byte >> 4) & 0x0F;
            locSize = s_hmi_evtSize[evType];
            event.channel = midCh;
            event.type = evType;

            if(fr.tell() + locSize > d.end)
            {
                m_errorString.appendFmt("HMI/HMP: Can't read regular %u-byte event of type 0x%X0- Unexpected end of track data.\n", (unsigned)locSize, (unsigned)evType);
                return false;
            }

            if(locSize > 0)
                fr.read(event.data_loc, 1, locSize);
            event.data_loc_size = locSize;

#ifdef BWMIDI_DEBUG_HMI_PARSE
            printf("-- Regular %u-byte event 0x%02X, chan=%u\n", (unsigned)locSize, evType, midCh);
            fflush(stdout);
#endif

            if(evType == MidiEvent::T_NOTEON)
            {
                if(!hmi_data.isHMP) // Turn into Note-ON with duration
                {
                    duration = hmi_data.fReadVarLen(fr, d.end, ok);
                    if(!ok)
                    {
                        m_errorString.append("HMI/HMP: Can't read the duration of timed note.\n");
                        return false;
                    }

                    duration += 1;

                    event.type = MidiEvent::T_NOTEON_DURATED;
                    if(duration > 0xFFFFFF)
                        duration = 0xFFFFFF; // Fit to 3 bytes (maximum 16777215 ticks duration)

                    event.data_loc_size = 5;
                    event.data_loc[2] = ((duration >> 16) & 0xFF);
                    event.data_loc[3] = ((duration >> 8) & 0xFF);
                    event.data_loc[4] = (duration & 0xFF);
                }
                else if(event.data_loc[1] == 0) // Note ON with zero velocity is Note OFF!
                    event.type = MidiEvent::T_NOTEOFF;
            }
            else if(evType == MidiEvent::T_CTRLCHANGE)
            {
                switch(event.data_loc[0])
                {
                case HMI_CC_RESTORE_ENABLE: /* 103, Same as HMP_CC_DISABLE_CC_RESTORE */
                    if(!hmi_data.isHMP && !hmi_cc_restore_on_cvt(&event))
                    {
                        m_errorString.appendFmt("HMI: Unsupported value for the CC103 Enable state restoring on loop: %u", event.data_loc[1]);
                        return false;
                    }
                    else if(hmi_data.isHMP && !hmi_cc_restore_off_cvt(&event))
                    {
                        m_errorString.appendFmt("HMP: Unsupported value for the CC104 Disable state restoring on loop: %u", event.data_loc[1]);
                        return false;
                    }
                    break;

                case HMI_CC_RESTORE_DISABLE: /* 104, Same as HMP_CC_ENABLE_CC_RESTORE */
                    if(!hmi_data.isHMP && !hmi_cc_restore_off_cvt(&event))
                    {
                        m_errorString.appendFmt("HMI: Unsupported value for the CC104 Disable state restoring on loop: %u", event.data_loc[1]);
                        return false;
                    }
                    else if(hmi_data.isHMP && !hmi_cc_restore_on_cvt(&event))
                    {
                        m_errorString.appendFmt("HMP: Unsupported value for the CC103 Enable state restoring on loop: %u", event.data_loc[1]);
                        return false;
                    }
                    break;

                case HMI_CC_LOCK_CHANNEL: /* 106 */
                    if(!hmi_data.isHMP)
                    {
                        event.type = MidiEvent::T_SPECIAL;
                        event.subtype = event.data_loc[1] == 0 ? MidiEvent::ST_CHANNEL_UNLOCK : MidiEvent::ST_CHANNEL_LOCK;
                        event.data_loc_size = 0;
                    }
                    break;

                case HMI_CC_SET_CH_PRIORITY: /* 107 */
                    if(!hmi_data.isHMP)
                    {
                        event.type = MidiEvent::T_SPECIAL;
                        event.subtype = MidiEvent::ST_CHANNEL_PRIORITY;
                        event.data_loc[0] = event.data_loc[1];
                        event.data_loc_size = 1;
                    }
                    break;


                case HMI_CC_SET_LOCAL_BRANCH: /* 108 */
                    if(hmi_data.isHMP)
                        break;
                    event.type = MidiEvent::T_SPECIAL;
                    event.subtype = MidiEvent::ST_TRACK_BRANCH_LOCATION;
                    event.data_loc[0] = event.data_loc[1];
                    event.data_loc_size = 1;
                    break;
                case HMP_CC_SET_LOCAL_BRANCH: /* 120 */
                    if(!hmi_data.isHMP)
                        break;
                    event.type = MidiEvent::T_SPECIAL;
                    event.subtype = MidiEvent::ST_TRACK_BRANCH_LOCATION;
                    event.data_loc[0] = event.data_loc[1];
                    event.data_loc_size = 1;
                    break;

                case HMI_CC_JUMP_TO_LOC_BRANCH: /* 109, Same as HMP_CC_GLOB_LOOP_START */
                    if(hmi_data.isHMP)
                    {
                        event.type = MidiEvent::T_SPECIAL;
                        event.subtype = MidiEvent::ST_LOOPSTACK_BEGIN_ID;
                        fr.seek(3, FileAndMemReader::CUR);
                        // ID will be stored at second byte
                        fr.read(event.data_loc, 1, 1);
                        // fr.seek(-4, FileAndMemReader::CUR);
                        if(event.data_loc[0] == 0xFF)
                            event.data_loc[0] = 0; // 0xFF is "infinite" too
                        event.data_loc_size = 2;
                    }
                    else
                    {
                        event.type = MidiEvent::T_SPECIAL;
                        event.subtype = MidiEvent::ST_TRACK_BRANCH_TO;
                        event.data_loc[0] = event.data_loc[1];
                        event.data_loc_size = 1;
                    }
                    break;
                case HMP_CC_JUMP_TO_LOCLBRANCH: /* 121 */
                    if(!hmi_data.isHMP)
                        break;
                    event.type = MidiEvent::T_SPECIAL;
                    event.subtype = MidiEvent::ST_TRACK_BRANCH_TO;
                    event.data_loc[0] = event.data_loc[1];
                    event.data_loc_size = 1;
                    break;

                case HMI_CC_GLOB_LOOP_START: /* 110, Same as HMP_CC_GLOB_LOOPS_COUNT */
                    if(hmi_data.isHMP)
                        break;
                    event.type = MidiEvent::T_SPECIAL;
                    event.subtype = MidiEvent::ST_LOOPSTACK_BEGIN;
                    event.data_loc[0] = event.data_loc[1];
                    if(event.data_loc[0] == 0xFF)
                        event.data_loc[0] = 0; // 0xFF is "infinite" too
                    event.data_loc_size = 1;
                    break;

                case HMP_CC_GLOB_LOOP_END2: /* 112 */
                    if(!hmi_data.isHMP)
                        break;
                    /* fallthrough */
                case HMI_CC_GLOB_LOOP_END: /* 111 */
                    if(hmi_data.isHMP)
                    {
                        event.type = MidiEvent::T_SPECIAL;
                        event.subtype = MidiEvent::ST_LOOPSTACK_END_ID;
                        event.data_loc[0] = event.data_loc[1];
                        event.data_loc_size = 1;
                    }
                    else
                    {
                        event.type = MidiEvent::T_SPECIAL;
                        event.subtype = MidiEvent::ST_LOOPSTACK_END;
                        event.data_loc_size = 0;
                    }
                    break;

                case HMI_CC_SET_GLOBAL_BRANCH: /* 113, same as HMP_CC_SET_GLOBAL_BRANCH */
                    event.type = MidiEvent::T_SPECIAL;
                    event.subtype = MidiEvent::ST_BRANCH_LOCATION;
                    event.data_loc[0] = event.data_loc[1];
                    event.data_loc_size = 1;
                    break;

                case HMI_CC_JUMP_TO_GLOB_BRANCH: /* 114, same as HMP_CC_JUMP_TO_GLOB_BRANCH */
                    event.type = MidiEvent::T_SPECIAL;
                    event.subtype = MidiEvent::ST_BRANCH_TO;
                    event.data_loc[0] = event.data_loc[1];
                    event.data_loc_size = 1;
                    break;

                case HMP_CC_LOC_LOOP_START: /* 115 */
                    if(!hmi_data.isHMP)
                        break;
                    event.type = MidiEvent::T_SPECIAL;
                    event.subtype = MidiEvent::ST_TRACK_LOOPSTACK_BEGIN_ID;
                    fr.seek(3, FileAndMemReader::CUR);
                    // ID will be stored at second byte
                    fr.read(event.data_loc, 1, 1);
                    // fr.seek(-4, FileAndMemReader::CUR);
                    if(event.data_loc[0] == 0xFF)
                        event.data_loc[0] = 0; // 0xFF is "infinite" too
                    event.data_loc_size = 2;
                    break;

                case HMI_CC_LOCAL_LOOP_START: /* 116 */
                    if(hmi_data.isHMP)
                        break;
                    event.type = MidiEvent::T_SPECIAL;
                    event.subtype = MidiEvent::ST_TRACK_LOOPSTACK_BEGIN;
                    event.data_loc[0] = event.data_loc[1];
                    if(event.data_loc[0] == 0xFF)
                        event.data_loc[0] = 0; // 0xFF is "infinite" too
                    event.data_loc_size = 1;
                    break;

                case HMP_CC_LOCAL_LOOP_END2: /* 118 */
                    if(!hmi_data.isHMP)
                        break;
                    /* fallthrough */
                case HMI_CC_LOCAL_LOOP_END: /* 117 */
                    if(hmi_data.isHMP)
                    {
                        event.type = MidiEvent::T_SPECIAL;
                        event.subtype = MidiEvent::ST_TRACK_LOOPSTACK_END_ID;
                        event.data_loc[0] = event.data_loc[1];
                        event.data_loc_size = 1;
                    }
                    else
                    {
                        event.type = MidiEvent::T_SPECIAL;
                        event.subtype = MidiEvent::ST_TRACK_LOOPSTACK_END;
                        event.data_loc_size = 0;
                    }
                    break;

                case HMI_CC_CALLBACK_TRIGGER: /* 119 */
                    event.type = MidiEvent::T_SPECIAL;
                    event.subtype = MidiEvent::ST_CALLBACK_TRIGGER;
                    event.data_loc[0] = event.data_loc[1];
                    event.data_loc_size = 1;
                    break;
                }
            }
        }
    }

    return true;
}


bool BW_MidiSequencer::parseHMI(FileAndMemReader &fr)
{
    char readBuf[20];
    size_t fsize, file_size, totalGotten;
    uint64_t abs_position;
    HMIData hmi_data;
    bool ok = false;
    int status = 0;
    MidiTrackRow evtPos;
    LoopPointParseState loopState;
    MidiEvent event;
#ifdef BWMIDI_DEBUG_HMI_PARSE
    Tempo_t t;
#endif

    std::vector<TempoEvent> temposList;
    std::vector<HMITrackDir> dir;

    std::memset(&loopState, 0, sizeof(loopState));
    std::memset(&hmi_data, 0, sizeof(hmi_data));

    file_size = fr.fileSize();

    if(file_size < 0x100)
    {
        m_errorString.set("HMI/HMP: Too small file!\n");
        return false;
    }

    m_format = Format_HMI;
    totalGotten = 0;

    fsize = fr.read(readBuf, 1, sizeof(readBuf));
    if(fsize < sizeof(readBuf))
    {
        m_errorString.set("HMI/HMP: Unexpected end of file at header!\n");
        return false;
    }

    if(std::memcmp(readBuf, "HMI-MIDISONG061595", 19) == 0)
    {
        hmi_data.isHMP = false;
        hmi_data.fReadVarLen = readVarLenEx; // Same as SMF!

        fr.seek(HMI_OFFSET_DIVISION, FileAndMemReader::SET);
        if(!readUInt16LE(hmi_data.division, fr))
        {
            m_errorString.set("HMI/HMP: Failed to read division value!\n");
            return false;
        }

        hmi_data.division <<= 2;

        fr.seek(HMI_OFFSET_TRACKS_COUNT, FileAndMemReader::SET);
        if(!readUInt16LE(hmi_data.tracksCount, fr))
        {
            m_errorString.set("HMI: Failed to read tracks count!\n");
            return false;
        }

        fr.seek(2, FileAndMemReader::CUR);
        if(!readUInt32LE_SZ(hmi_data.track_dir_offset, fr))
        {
            m_errorString.set("HMI: Failed to read track dir pointer!\n");
            return false;
        }

#ifdef BWMIDI_DEBUG_HMI_PARSE
        printf("== Division: %lu\n", hmi_data.division);
        fflush(stdout);
#endif

        m_invDeltaTicks.nom = 1;
        m_invDeltaTicks.denom = 1000000l * hmi_data.division;
        m_tempo.nom = 1;
        m_tempo.denom = hmi_data.division;

        dir.resize(hmi_data.tracksCount);
        std::memset(dir.data(), 0, sizeof(HMITrackDir) * hmi_data.tracksCount);

        // Read track sizes
        for(size_t tk = 0; tk < hmi_data.tracksCount; ++tk)
        {
            HMITrackDir &d = dir[tk];

            fr.seek(hmi_data.track_dir_offset + (tk * 4), FileAndMemReader::SET);
            if(!readUInt32LE_SZ(d.start, fr))
            {
                m_errorString.set("HMI: Failed to read track start offset!\n");
                return false;
            }

            if(d.start > file_size - HMI_OFFSET_TRACK_DEVICES - 4)
            {
                d.len = 0;
                continue; // Track is incomplete
            }

            fr.seek(d.start, FileAndMemReader::SET);
            if(fr.read(readBuf, 1, 13) != 13)
            {
                m_errorString.set("HMI: Failed to read track magic!\n");
                return false;
            }

            if(std::memcmp(readBuf, "HMI-MIDITRACK", 13) != 0)
            {
                d.len = 0;
                continue; // Invalid track magic
            }

            if(tk == hmi_data.tracksCount - 1)
            {
                d.len = file_size - d.start;
                d.end = d.start + d.len;
            }
            else
            {
                fr.seek(hmi_data.track_dir_offset + (tk * HMI_SIZE_TRACK_DIR_HEAD) + HMI_OFFSET_TRACK_LEN, FileAndMemReader::SET);
                if(!readUInt32LE_SZ(d.len, fr))
                {
                    m_errorString.set("HMI: Failed to read track start offset!\n");
                    return false;
                }

                if(d.len < d.start)
                {
                    d.len = 0;
                    continue;
                }

                d.len -= d.start;

                if(file_size - d.start < d.len)
                    d.len = file_size - d.start;

                d.end = d.start + d.len;
            }

            fr.seek(d.start + HMI_OFFSET_TRACK_DATA_OFFSET, FileAndMemReader::SET);
            if(!readUInt32LE_SZ(d.offset, fr))
            {
                m_errorString.set("HMI: Failed to read MIDI events offset!\n");
                return false;
            }

            if(d.len < d.offset)
            {
                d.len = 0;
                continue;
            }

            d.len -= d.offset;
            totalGotten += d.len;

            d.devicesNum = HMI_MAX_TRACK_DEVICES;

            fr.seek(d.start + HMI_OFFSET_TRACK_DEVICES, FileAndMemReader::SET);
            for(size_t j = 0; j < HMI_MAX_TRACK_DEVICES; ++j)
            {
                if(!readUInt16LE(d.devices[j], fr))
                {
                    m_errorString.set("HMI: Failed to read track destignation value!\n");
                    return false;
                }
            }
        }
    }
    else if(std::memcmp(readBuf, "HMIMIDIP", 8) == 0)
    {
        /********************************************************************************
         *                         HMP header specification                             *
         *                                                                              *
         * 32 bytes         - Magic number                                              *
         * 4 bytes          - Offset to branches list (32-bit little endian)            *
         * 12 bytes         - Padding                                                   *
         * 4 bytes          - Total tracks in the song (32-bit little endian)           *
         * 4 bytes          - Ticks per Quote number (32-bit little endian)             *
         * 4 bytes          - Division factor (32-bit little endian)                    *
         * 4 bytes          - Time duration (aproximal) (32-bit little endian)          *
         * 16 x 4 bytes     - Array of initial channel priorities (32-bit LE)           *
         * 32 x 5 x 4 bytes - Two-dimensional array of device IDs (5 maximum) per track *
         *                    (Array of 32-bit little endian values)                    *
         * 128 bytes        - MIDI controller initializers (1 byte per CC)              *
         * 8 bytes          - Padding                                                   *
         ********************************************************************************/

        hmi_data.isHMP = true;
        hmi_data.fReadVarLen = readHMPVarLenEx; // Has different format!

        if(readBuf[8] == 0)
            hmi_data.tracks_offset = 0x308;
        else if(std::memcmp(readBuf + 8, "013195", 6) == 0)
            hmi_data.tracks_offset = 0x388;
        else
        {
            m_errorString.set("HMP: Unknown version of the HMIMIDIP!\n");
            return false;
        }

        // Magic number ends here
        fr.seek(0x20, FileAndMemReader::SET);

        // File length value
        if(!readUInt32LE_SZ(hmi_data.branch_offset, fr))
        {
            m_errorString.set("HMP: Failed to read file length field value!\n");
            return false;
        }

        // Skip 12 bytes of padding
        fr.seek(12, FileAndMemReader::CUR);

        // Tracks count value
        if(!readUInt32LE_SZ(hmi_data.tracksCount, fr))
        {
            m_errorString.set("HMP: Failed to read tracks count!\n");
            return false;
        }

        if(hmi_data.tracksCount > 32)
        {
            m_errorString.setFmt("HMP: File contains more than 32 tracks (%u)!\n", (unsigned)hmi_data.tracksCount);
            return false;
        }

        if(!readUInt32LE_SZ(hmi_data.tpqn, fr))
        {
            m_errorString.set("HMP: Failed to read TPQN value!\n");
            return false;
        }

        // Beats per minute
        if(!readUInt32LE_SZ(hmi_data.division, fr))
        {
            m_errorString.set("HMP: Failed to read division value!\n");
            return false;
        }

        if(!readUInt32LE_SZ(hmi_data.timeDuration, fr))
        {
            m_errorString.set("HMP: Failed to read time duration value!\n");
            return false;
        }

        for(size_t i = 0; i < 16; ++i)
        {
            if(!readUInt32LE(hmi_data.priorities[i], fr))
            {
                m_errorString.set("HMP: Failed to read one of channel priorities values!\n");
                return false;
            }
        }

        for(size_t i = 0; i < 32; ++i)
        {
            for(size_t j = 0; j < 5; ++j)
            {
                if(!readUInt32LE(hmi_data.trackDevice[i][j], fr))
                {
                    m_errorString.set("HMP: Failed to read one of track device map values!\n");
                    return false;
                }
            }
        }

        if(!fr.read(hmi_data.controlInit, 1, 128))
        {
            m_errorString.set("HMP: Failed to read one of control init bytes!\n");
            return false;
        }


        m_invDeltaTicks.nom = 1;
        m_invDeltaTicks.denom = 1000000l * hmi_data.division;
        m_tempo.nom = 1;
        m_tempo.denom = hmi_data.division;

        dir.resize(hmi_data.tracksCount);
        std::memset(dir.data(), 0, sizeof(HMITrackDir) * hmi_data.tracksCount);

        for(size_t tk = 0; tk < hmi_data.tracksCount; ++tk)
        {
            HMITrackDir &d = dir[tk];

            fr.seek(hmi_data.tracks_offset, FileAndMemReader::SET);
            d.start = hmi_data.tracks_offset;

            if(d.start > file_size - HMP_OFFSET_TRACK_DATA)
            {
                d.len = 0;
                break; // Track is incomplete
            }

            if(!readUInt32LE_SZ(hmi_data.current_track, fr))
            {
                m_errorString.set("HMP: Failed to read track number value!\n");
                return false;
            }

            if(hmi_data.current_track != tk)
            {
                m_errorString.set("HMP: Captured track number value is not matching!\n");
                return false;
            }

            if(!readUInt32LE_SZ(d.len, fr))
            {
                m_errorString.set("HMP: Failed to read track start offset!\n");
                return false;
            }

            if(!readUInt32LE_SZ(d.midichan, fr))
            {
                m_errorString.set("HMP: Failed to read track's MIDI channel value!\n");
                return false;
            }

            hmi_data.tracks_offset += d.len;
            d.end = d.start + d.len;

            if(file_size - d.start < d.len)
                d.len = file_size - d.start;

            if(d.len <= HMP_OFFSET_TRACK_DATA)
            {
                d.len = 0;
                continue;
            }

            d.offset = HMP_OFFSET_TRACK_DATA;
            d.len -= HMP_OFFSET_TRACK_DATA; // Track header size!

            d.devicesNum = HMP_MAX_TRACK_DEVICES;

            for(size_t j = 0; j < HMP_MAX_TRACK_DEVICES; ++j)
                d.devices[j] = hmi_data.trackDevice[tk][j];

            totalGotten += d.len;
        }
    }
    else
    {
        m_errorString.set("HMI/HMP: Invalid magic number!\n");
        return false;
    }

    if(totalGotten == 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Empty track data");
        return false;
    }


    buildSmfSetupReset(hmi_data.tracksCount);

    m_loopFormat = Loop_HMI;
    m_stateRestoreSetup = TRACK_RESTORE_DEFAULT_HMI;

    std::memset(&event, 0, sizeof(event));
    event.isValid = 1;
    event.type = MidiEvent::T_SPECIAL;
    event.subtype = MidiEvent::ST_SONG_BEGIN_HOOK;
    // HACK: Begin every track with "Reset all controllers" event to avoid controllers state break came from end of song
    addEventToBank(evtPos, event);

    std::memset(&event, 0, sizeof(event));
    event.isValid = 1;
    event.type = MidiEvent::T_SPECIAL;
    event.subtype = MidiEvent::ST_TEMPOCHANGE;
    event.data_loc_size = 3;
    if(hmi_data.isHMP)
    {
        event.data_loc[0] = 0x0F;
        event.data_loc[1] = 0x42;
        event.data_loc[2] = 0x40;
    }
    else
    {
        event.data_loc[0] = 0x3D;
        event.data_loc[1] = 0x09;
        event.data_loc[2] = 0x00;
    }
    addEventToBank(evtPos, event);
    TempoEvent tempoEvent = {readBEint(event.data_loc, event.data_loc_size), 0};
    temposList.push_back(tempoEvent);

    evtPos.delay = 0;
    evtPos.absPos = 0;
    m_trackData[0].push_back(evtPos);
    evtPos.clear();

#ifdef BWMIDI_DEBUG_HMI_PARSE
    printf("==Tempo %g, Div %g=========================\n", tempo_get(&m_tempo), tempo_get(&m_invDeltaTicks));
    fflush(stdout);
#endif

    size_t tk_v = 0;

    for(size_t tk = 0; tk < hmi_data.tracksCount; ++tk)
    {
        const HMITrackDir &d = dir[tk];
        MidiTrackState &trackState = m_trackState[tk_v];
        bool trackChannelHas = false;

        if(d.len == 0)
            continue; // This track is broken

        status = 0;
        fr.seek(d.start + d.offset, FileAndMemReader::SET);

#ifdef BWMIDI_DEBUG_HMI_PARSE
        printf("==Track %lu=(de-facto %lu)=============================\n", (unsigned long)tk, (unsigned long)tk_v);
        fflush(stdout);
#endif

        trackState.deviceMask = Device_ANY;

        for(size_t dev = 0; dev < d.devicesNum; ++dev)
        {
            if(!d.devices[dev])
                break;

            if(dev == 0)
                trackState.deviceMask = 0;

            switch(d.devices[dev])
            {
            case HMI_DRIVER_SOUND_MASTER_II:
                trackState.deviceMask |= Device_SoundMasterII;
                break;
            case HMI_DRIVER_MPU_401:
                trackState.deviceMask |= Device_GeneralMidi;
                break;
            case HMI_DRIVER_OPL2:
                trackState.deviceMask |= Device_OPL2;
                break;
            case HMI_DRIVER_CALLBACK:
                trackState.deviceMask |= Device_Callback;
                break;
            case HMI_DRIVER_MT_32:
                trackState.deviceMask |= Device_MT32;
                break;
            case HMI_DRIVER_DIGI:
                trackState.deviceMask |= Device_DIGI;
                break;
            case HMI_DRIVER_INTERNAL_SPEAKER:
                trackState.deviceMask |= Device_PCSpeaker;
                break;
            case HMI_DRIVER_WAVE_TABLE_SYNTH:
                trackState.deviceMask |= Device_WaveTable;
                break;
            case HMI_DRIVER_AWE32:
                trackState.deviceMask |= Device_AWE32;
                break;
            case HMI_DRIVER_OPL3:
                trackState.deviceMask |= Device_OPL3;
                break;
            case HMI_DRIVER_GUS:
                trackState.deviceMask |= Device_GravisUltrasound;
                break;
            default:
                m_errorString.setFmt("HMI/HMP: Unsupported device type 0x%04X\n", (unsigned)hmi_data.trackDevice[tk][dev]);
                return false;
            }
        }

        if(trackState.deviceMask != Device_ANY)
        {
            if(m_deviceMaskAvailable == Device_ANY)
                m_deviceMaskAvailable = trackState.deviceMask;
            else
                m_deviceMaskAvailable |= trackState.deviceMask;
        }

        if(m_deviceMask != Device_ANY && (m_deviceMask & trackState.deviceMask) == 0)
            continue; // Exclude this track completely

        // Time delay that follows the first event in the track
        abs_position = hmi_data.fReadVarLen(fr, d.end, ok);
        if(!ok)
        {
            m_errorString.set("HMI/HMP: Failed to read first event's delay\n");
            return false;
        }

        do
        {
            if(!hmi_parseEvent(hmi_data, d, fr, event, status))
                return false; // Error value already written

            if(event.isValid)
            {
                addEventToBank(evtPos, event);

                if(!trackChannelHas && event.type > 0x00 && event.type < 0xF0)
                {
                    trackState.state.track_channel = event.channel;
                    if(event.channel != 0) // If it's always zero channel, sounds like something wrong
                        trackChannelHas = true;
                }
            }

            if(event.type != MidiEvent::T_SPECIAL || event.subtype != MidiEvent::ST_ENDTRACK)
            {
                if(event.type == MidiEvent::T_SPECIAL)
                {
                    if(event.subtype == MidiEvent::ST_TEMPOCHANGE)
                    {
                        TempoEvent t = {readBEint(event.data_loc, event.data_loc_size), abs_position};
                        temposList.push_back(t);
                    }
                    else if(event.subtype != MidiEvent::ST_ENDTRACK)
                        analyseLoopEvent(loopState, event, abs_position, &trackState.loop);
                }

                evtPos.delay = hmi_data.fReadVarLen(fr, d.end, ok);
                if(!ok)
                {
                    /* End of track has been reached! However, there is no EOT event presented */
                    event.type = MidiEvent::T_SPECIAL;
                    event.subtype = MidiEvent::ST_ENDTRACK;
                    event.isValid = 1;
                }
            }

#ifdef ENABLE_END_SILENCE_SKIPPING
            //Have track end on its own row? Clear any delay on the row before
            if(event.type == MidiEvent::T_SPECIAL && event.subtype == MidiEvent::ST_ENDTRACK && (evtPos.events_end - evtPos.events_begin) == 1)
            {
                if(!m_trackData[tk_v].empty())
                {
                    MidiTrackRow &previous = m_trackData[tk_v].back();
                    previous.delay = 0;
                    previous.timeDelay = 0;
                }
            }
#endif

            if((evtPos.delay > 0) || loopState.gotLoopEventsInThisRow > 0  || (event.subtype == MidiEvent::ST_ENDTRACK))
            {
#ifdef BWMIDI_DEBUG_HMI_PARSE
                tempo_mul(&t, &m_tempo, abs_position);
                printf("- Delay %lu, Position %lu, stored events: %lu, time: %g seconds\n",
                        (unsigned long)evtPos.delay, abs_position, evtPos.events_end - evtPos.events_begin,
                        tempo_get(&t));
                fflush(stdout);
#endif
                smf_flushRow(evtPos, abs_position, tk_v, loopState);
            }

            if(status < 0 && evtPos.events_begin != evtPos.events_end) // Last row in the track
                smf_flushRow(evtPos, abs_position, tk_v, loopState, true);

        } while((fr.tell() <= d.end) && (event.subtype != MidiEvent::ST_ENDTRACK));

#ifdef BWMIDI_DEBUG_HMI_PARSE
        printf("==Track %lu==END=========================\n", (unsigned long)tk_v);
        fflush(stdout);
#endif

        if(loopState.ticksSongLength < abs_position)
            loopState.ticksSongLength = abs_position;

        // Set the chain of events begin
        initTracksBegin(tk_v);

        ++tk_v;
    }

    debugPrintDevices();

    // Shrink tracks store if real number of tracks is smaller
    if(m_tracksCount != tk_v)
        buildSmfResizeTracks(tk_v);

    installLoop(loopState);
    buildTimeLine(temposList, loopState.loopStartTicks, loopState.loopEndTicks);

    return true;
}

#endif /* BW_MIDISEQ_READ_HMI_IMPL_HPP */
