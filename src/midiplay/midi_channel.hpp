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

#ifndef OPLMIDI_MIDIPLAY_MIDI_CHANNEL_HPP
#define OPLMIDI_MIDIPLAY_MIDI_CHANNEL_HPP

#include "adldata.hh"
#include "structures/pl_list.hpp"
#include <cmath>
#include <cstddef>
#include <cassert>
#include <stdint.h>

/**
 * @brief Persistent settings for each MIDI channel
 */
struct MIDIchannel
{
    //! LSB Bank number
    uint8_t bank_lsb,
    //! MSB Bank number
            bank_msb;
    //! Current patch number
    uint8_t patch;
    //! Volume level
    uint8_t volume,
    //! Expression level
            expression;
    //! Panning level
    uint8_t panning,
    //! Vibrato level
            vibrato,
    //! Channel aftertouch level
            aftertouch;
    //! Portamento time
    uint16_t portamento;
    //! Is Pedal sustain active
    bool sustain;
    //! Is Soft pedal active
    bool softPedal;
    //! Is portamento enabled
    bool portamentoEnable;
    //! Source note number used by portamento
    int8_t portamentoSource;  // note number or -1
    //! Portamento rate
    double portamentoRate;
    //! Per note Aftertouch values
    uint8_t noteAftertouch[128];
    //! Is note aftertouch has any non-zero value
    bool    noteAfterTouchInUse;
    //! Reserved
    char _padding[6];
    //! Pitch bend value
    int bend;
    //! Pitch bend sensitivity
    double bendsense;
    //! Pitch bend sensitivity LSB value
    int bendsense_lsb,
    //! Pitch bend sensitivity MSB value
        bendsense_msb;
    //! Vibrato position value
    double  vibpos,
    //! Vibrato speed value
            vibspeed,
    //! Vibrato depth value
            vibdepth;
    //! Vibrato delay time
    int64_t vibdelay_us;
    //! Last LSB part of RPN value received
    uint8_t lastlrpn,
    //! Last MSB poart of RPN value received
            lastmrpn;
    //! Interpret RPN value as NRPN
    bool nrpn;
    //! Brightness level
    uint8_t brightness;

    //! Is melodic channel turned into percussion
    bool is_xg_percussion;

    /**
     * @brief Per-Note information
     */
    struct NoteInfo
    {
        //! Note number
        uint8_t note;
        //! Current pressure
        uint8_t vol;
        //! Note vibrato (a part of Note Aftertouch feature)
        uint8_t vibrato;
        //! Tone selected on noteon:
        int16_t noteTone;
        //! Current tone (!= noteTone if gliding note)
        double currentTone;
        //! Gliding rate
        double glideRate;
        //! Patch selected on noteon; index to bank.ins[]
        size_t  midiins;
        //! Is note the percussion instrument
        bool    isPercussion;
        //! Note that plays missing instrument. Doesn't using any chip channels
        bool    isBlank;
        //! Whether releasing and on extended life time defined by TTL
        bool    isOnExtendedLifeTime;
        //! Time-to-live until release (short percussion note fix)
        double  ttl;
        //! Patch selected
        const adlinsdata2 *ains;
        enum
        {
            MaxNumPhysChans = 2,
            MaxNumPhysItemCount = MaxNumPhysChans
        };

        struct FindPredicate
        {
            explicit FindPredicate(unsigned note)
                : note(note) {}
            bool operator()(const NoteInfo &ni) const
                { return ni.note == note; }
            unsigned note;
        };

        /**
         * @brief Reference to currently using chip channel
         */
        struct Phys
        {
            //! Destination chip channel
            uint16_t chip_chan;
            //! ins, inde to adl[]
            adldata ains;
            //! Is this voice must be detunable?
            bool    pseudo4op;

            void assign(const Phys &oth)
            {
                ains = oth.ains;
                pseudo4op = oth.pseudo4op;
            }
            bool operator==(const Phys &oth) const
            {
                return (ains == oth.ains) && (pseudo4op == oth.pseudo4op);
            }
            bool operator!=(const Phys &oth) const
            {
                return !operator==(oth);
            }
        };

        //! List of OPL3 channels it is currently occupying.
        Phys chip_channels[MaxNumPhysItemCount];
        //! Count of used channels.
        unsigned chip_channels_count;

        Phys *phys_find(unsigned chip_chan)
        {
            Phys *ph = NULL;
            for(unsigned i = 0; i < chip_channels_count && !ph; ++i)
                if(chip_channels[i].chip_chan == chip_chan)
                    ph = &chip_channels[i];
            return ph;
        }
        Phys *phys_find_or_create(uint16_t chip_chan)
        {
            Phys *ph = phys_find(chip_chan);
            if(!ph) {
                if(chip_channels_count < MaxNumPhysItemCount) {
                    ph = &chip_channels[chip_channels_count++];
                    ph->chip_chan = chip_chan;
                }
            }
            return ph;
        }
        Phys *phys_ensure_find_or_create(uint16_t chip_chan)
        {
            Phys *ph = phys_find_or_create(chip_chan);
            assert(ph);
            return ph;
        }
        void phys_erase_at(const Phys *ph)
        {
            intptr_t pos = ph - chip_channels;
            assert(pos < static_cast<intptr_t>(chip_channels_count));
            for(intptr_t i = pos + 1; i < static_cast<intptr_t>(chip_channels_count); ++i)
                chip_channels[i - 1] = chip_channels[i];
            --chip_channels_count;
        }
        void phys_erase(unsigned chip_chan)
        {
            Phys *ph = phys_find(chip_chan);
            if(ph)
                phys_erase_at(ph);
        }
    };

    //! Reserved
    char _padding2[5];
    //! Count of gliding notes in this channel
    unsigned gliding_note_count;
    //! Count of notes having a TTL countdown in this channel
    unsigned extended_note_count;

    //! Active notes in the channel
    pl_list<NoteInfo> activenotes;
    typedef pl_list<NoteInfo>::iterator notes_iterator;
    typedef pl_list<NoteInfo>::const_iterator const_notes_iterator;

    notes_iterator find_activenote(unsigned note)
    {
        return activenotes.find_if(NoteInfo::FindPredicate(note));
    }

    notes_iterator ensure_find_activenote(unsigned note)
    {
        notes_iterator it = find_activenote(note);
        assert(!it.is_end());
        return it;
    }

    notes_iterator find_or_create_activenote(unsigned note)
    {
        notes_iterator it = find_activenote(note);
        if(!it.is_end())
            cleanupNote(it);
        else
        {
            NoteInfo ni;
            ni.note = note;
            it = activenotes.insert(activenotes.end(), ni);
        }
        return it;
    }

    notes_iterator ensure_find_or_create_activenote(unsigned note)
    {
        notes_iterator it = find_or_create_activenote(note);
        assert(!it.is_end());
        return it;
    }

    /**
     * @brief Reset channel into initial state
     */
    void reset()
    {
        resetAllControllers();
        patch = 0;
        vibpos = 0;
        bank_lsb = 0;
        bank_msb = 0;
        lastlrpn = 0;
        lastmrpn = 0;
        nrpn = false;
        is_xg_percussion = false;
    }

    /**
     * @brief Reset all MIDI controllers into initial state
     */
    void resetAllControllers()
    {
        bend = 0;
        bendsense_msb = 2;
        bendsense_lsb = 0;
        updateBendSensitivity();
        volume  = 100;
        expression = 127;
        sustain = false;
        softPedal = false;
        vibrato = 0;
        aftertouch = 0;
        std::memset(noteAftertouch, 0, 128);
        noteAfterTouchInUse = false;
        vibspeed = 2 * 3.141592653 * 5.0;
        vibdepth = 0.5 / 127;
        vibdelay_us = 0;
        panning = 64;
        portamento = 0;
        portamentoEnable = false;
        portamentoSource = -1;
        portamentoRate = HUGE_VAL;
        brightness = 127;
    }

    /**
     * @brief Has channel vibrato to process
     * @return
     */
    bool hasVibrato()
    {
        return (vibrato > 0) || (aftertouch > 0) || noteAfterTouchInUse;
    }

    /**
     * @brief Commit pitch bend sensitivity value from MSB and LSB
     */
    void updateBendSensitivity()
    {
        int cent = bendsense_msb * 128 + bendsense_lsb;
        bendsense = cent * (1.0 / (128 * 8192));
    }

    /**
     * @brief Clean up the state of the active note before removal
     */
    void cleanupNote(notes_iterator i)
    {
        NoteInfo &info = i->value;
        if(info.glideRate != HUGE_VAL)
            --gliding_note_count;
        if(info.ttl > 0)
            --extended_note_count;
    }

    MIDIchannel()
        : activenotes(128)
    {
        gliding_note_count = 0;
        extended_note_count = 0;
        reset();
    }
};

#endif // OPLMIDI_MIDIPLAY_MIDI_CHANNEL_HPP
