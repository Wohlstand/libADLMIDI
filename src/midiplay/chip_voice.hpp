/*
 * libADLMIDI is a free Software MIDI synthesizer library with OPL3 emulation
 *
 * ADLMIDI Library API:   Copyright (c) 2015-2019 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPLMIDI_MIDIPLAY_PLAYER_HPP
#define OPLMIDI_MIDIPLAY_PLAYER_HPP

#pragma message("TODO: get rid of include")
#include "midi_channel.hpp"
#include "structures/pl_list.hpp"

/**
 * @brief Additional information about OPL3 channels
 */
struct ChipChannel
{
    struct Location
    {
        uint16_t    MidCh;
        uint8_t     note;
        bool operator==(const Location &l) const
            { return MidCh == l.MidCh && note == l.note; }
        bool operator!=(const Location &l) const
            { return !operator==(l); }
    };
    struct LocationData
    {
        Location loc;
        enum {
            Sustain_None        = 0x00,
            Sustain_Pedal       = 0x01,
            Sustain_Sostenuto   = 0x02,
            Sustain_ANY         = Sustain_Pedal | Sustain_Sostenuto
        };
        uint32_t sustained;
        char _padding[6];
        MIDIchannel::NoteInfo::Phys ins;  // a copy of that in phys[]
        //! Has fixed sustain, don't iterate "on" timeout
        bool    fixed_sustain;
        //! Timeout until note will be allowed to be killed by channel manager while it is on
        int64_t kon_time_until_neglible_us;
        int64_t vibdelay_us;

        struct FindPredicate
        {
            explicit FindPredicate(Location loc)
                : loc(loc) {}
            bool operator()(const LocationData &ld) const
                { return ld.loc == loc; }
            Location loc;
        };
    };

    //! Time left until sounding will be muted after key off
    int64_t koff_time_until_neglible_us;

    //! Recently passed instrument, improves a goodness of released but busy channel when matching
    MIDIchannel::NoteInfo::Phys recent_ins;

    pl_list<LocationData> users;
    typedef pl_list<LocationData>::iterator users_iterator;
    typedef pl_list<LocationData>::const_iterator const_users_iterator;

    users_iterator find_user(const Location &loc)
    {
        return users.find_if(LocationData::FindPredicate(loc));
    }

    users_iterator find_or_create_user(const Location &loc)
    {
        users_iterator it = find_user(loc);
        if(it.is_end() && users.size() != users.capacity())
        {
            LocationData ld;
            ld.loc = loc;
            it = users.insert(users.end(), ld);
        }
        return it;
    }

    // For channel allocation:
    ChipChannel(): koff_time_until_neglible_us(0), users(128)
    {
        std::memset(&recent_ins, 0, sizeof(MIDIchannel::NoteInfo::Phys));
    }

    ChipChannel(const ChipChannel &oth): koff_time_until_neglible_us(oth.koff_time_until_neglible_us), users(oth.users)
    {
    }

    ChipChannel &operator=(const ChipChannel &oth)
    {
        koff_time_until_neglible_us = oth.koff_time_until_neglible_us;
        users = oth.users;
        return *this;
    }

    /**
     * @brief Increases age of active note in microseconds time
     * @param us Amount time in microseconds
     */
    void addAge(int64_t us);
};

#endif // OPLMIDI_MIDIPLAY_PLAYER_HPP
