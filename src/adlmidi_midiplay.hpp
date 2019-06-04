/*
 * libADLMIDI is a free Software MIDI synthesizer library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2019 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Library is based on the ADLMIDI, a MIDI player for Linux and Windows with OPL3 emulation:
 * http://iki.fi/bisqwit/source/adlmidi.html
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

#ifndef ADLMIDI_MIDIPLAY_HPP
#define ADLMIDI_MIDIPLAY_HPP

#include "adldata.hh"
#include "adlmidi_private.hpp"
#include "adlmidi_ptr.hpp"
#include "midiplay/event_hooks.hpp"
#include "midiplay/midi_channel.hpp"
#include "midiplay/chip_voice.hpp"
#include "midiplay/setup.hpp"
#include "midiplay/player.hpp"

class MIDIplay
{
    friend void adl_reset(struct ADL_MIDIPlayer*);
public:
    explicit MIDIplay(unsigned long sampleRate = 22050);
    ~MIDIplay();

    void applySetup();

    void partialReset();
    void resetMIDI();

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    /**
     * @brief MIDI files player sequencer
     */
    AdlMIDI_UPtr<MidiSequencer> m_sequencer;

    /**
     * @brief Interface between MIDI sequencer and this library
     */
    AdlMIDI_UPtr<BW_MidiRtInterface> m_sequencerInterface;

    /**
     * @brief Initialize MIDI sequencer interface
     */
    void initSequencerInterface();
#endif //ADLMIDI_DISABLE_MIDI_SEQUENCER

    /**
     * @brief MIDI Marker entry
     */
    struct MIDI_MarkerEntry
    {
        //! Label of marker
        std::string     label;
        //! Absolute position in seconds
        double          pos_time;
        //! Absolute position in ticks in the track
        uint64_t        pos_ticks;
    };

    //! Available MIDI Channels
    std::vector<MIDIchannel> m_midiChannels;

    //! CMF Rhythm mode
    bool    m_cmfPercussionMode;

    //! Master volume, controlled via SysEx
    uint8_t m_masterVolume;

    //! SysEx device ID
    uint8_t m_sysExDeviceId;

    /**
     * @brief MIDI Synthesizer mode
     */
    enum SynthMode
    {
        Mode_GM  = 0x00,
        Mode_GS  = 0x01,
        Mode_XG  = 0x02,
        Mode_GM2 = 0x04
    };
    //! MIDI Synthesizer mode
    uint32_t m_synthMode;

    //! Installed function hooks
    MIDIEventHooks hooks;

private:
    //! Per-track MIDI devices map
    std::map<std::string, size_t> m_midiDevices;
    //! Current MIDI device per track
    std::map<size_t /*track*/, size_t /*channel begin index*/> m_currentMidiDevice;

    //! Padding to fix CLanc code model's warning
    char _padding[7];

    //! Chip channels map
    std::vector<ChipChannel> m_chipChannels;
    //! Counter of arpeggio processing
    size_t m_arpeggioCounter;

#if defined(ADLMIDI_AUDIO_TICK_HANDLER)
    //! Audio tick counter
    uint32_t m_audioTickCounter;
#endif

    //! Local error string
    std::string errorStringOut;

    //! Missing instruments catches
    std::set<size_t> caugh_missing_instruments;
    //! Missing melodic banks catches
    std::set<size_t> caugh_missing_banks_melodic;
    //! Missing percussion banks catches
    std::set<size_t> caugh_missing_banks_percussion;

public:

    const std::string &getErrorString();
    void setErrorString(const std::string &err);

    //! OPL3 Chip manager
    AdlMIDI_UPtr<Synth> m_synth;

    //! Generator output buffer
    int32_t m_outBuf[1024];

    //! Synthesizer setup
    MIDIsetup m_setup;

    /**
     * @brief Load custom bank from file
     * @param filename Path to bank file
     * @return true on succes
     */
    bool LoadBank(const std::string &filename);

    /**
     * @brief Load custom bank from memory block
     * @param data Pointer to memory block where raw bank file is stored
     * @param size Size of given memory block
     * @return true on succes
     */
    bool LoadBank(const void *data, size_t size);

    /**
     * @brief Load custom bank from opened FileAndMemReader class
     * @param fr Instance with opened file
     * @return true on succes
     */
    bool LoadBank(FileAndMemReader &fr);

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    /**
     * @brief MIDI file loading pre-process
     * @return true on success, false on failure
     */
    bool LoadMIDI_pre();

    /**
     * @brief MIDI file loading post-process
     * @return true on success, false on failure
     */
    bool LoadMIDI_post();

    /**
     * @brief Load music file from a file
     * @param filename Path to music file
     * @return true on success, false on failure
     */

    bool LoadMIDI(const std::string &filename);

    /**
     * @brief Load music file from the memory block
     * @param data pointer to the memory block
     * @param size size of memory block
     * @return true on success, false on failure
     */
    bool LoadMIDI(const void *data, size_t size);

    /**
     * @brief Periodic tick handler.
     * @param s seconds since last call
     * @param granularity don't expect intervals smaller than this, in seconds
     * @return desired number of seconds until next call
     */
    double Tick(double s, double granularity);
#endif //ADLMIDI_DISABLE_MIDI_SEQUENCER

    /**
     * @brief Process extra iterators like vibrato or arpeggio
     * @param s seconds since last call
     */
    void   TickIterators(double s);


    /* RealTime event triggers */
    /**
     * @brief Reset state of all channels
     */
    void realTime_ResetState();

    /**
     * @brief Note On event
     * @param channel MIDI channel
     * @param note Note key (from 0 to 127)
     * @param velocity Velocity level (from 0 to 127)
     * @return true if Note On event was accepted
     */
    bool realTime_NoteOn(uint8_t channel, uint8_t note, uint8_t velocity);

    /**
     * @brief Note Off event
     * @param channel MIDI channel
     * @param note Note key (from 0 to 127)
     */
    void realTime_NoteOff(uint8_t channel, uint8_t note);

    /**
     * @brief Note aftertouch event
     * @param channel MIDI channel
     * @param note Note key (from 0 to 127)
     * @param atVal After-Touch level (from 0 to 127)
     */
    void realTime_NoteAfterTouch(uint8_t channel, uint8_t note, uint8_t atVal);

    /**
     * @brief Channel aftertouch event
     * @param channel MIDI channel
     * @param atVal After-Touch level (from 0 to 127)
     */
    void realTime_ChannelAfterTouch(uint8_t channel, uint8_t atVal);

    /**
     * @brief Controller Change event
     * @param channel MIDI channel
     * @param type Type of controller
     * @param value Value of the controller (from 0 to 127)
     */
    void realTime_Controller(uint8_t channel, uint8_t type, uint8_t value);

    /**
     * @brief Patch change
     * @param channel MIDI channel
     * @param patch Patch Number (from 0 to 127)
     */
    void realTime_PatchChange(uint8_t channel, uint8_t patch);

    /**
     * @brief Pitch bend change
     * @param channel MIDI channel
     * @param pitch Concoctated raw pitch value
     */
    void realTime_PitchBend(uint8_t channel, uint16_t pitch);

    /**
     * @brief Pitch bend change
     * @param channel MIDI channel
     * @param msb MSB of pitch value
     * @param lsb LSB of pitch value
     */
    void realTime_PitchBend(uint8_t channel, uint8_t msb, uint8_t lsb);

    /**
     * @brief LSB Bank Change CC
     * @param channel MIDI channel
     * @param lsb LSB value of bank number
     */
    void realTime_BankChangeLSB(uint8_t channel, uint8_t lsb);

    /**
     * @brief MSB Bank Change CC
     * @param channel MIDI channel
     * @param msb MSB value of bank number
     */
    void realTime_BankChangeMSB(uint8_t channel, uint8_t msb);

    /**
     * @brief Bank Change (united value)
     * @param channel MIDI channel
     * @param bank Bank number value
     */
    void realTime_BankChange(uint8_t channel, uint16_t bank);

    /**
     * @brief Sets the Device identifier
     * @param id 7-bit Device identifier
     */
    void setDeviceId(uint8_t id);

    /**
     * @brief System Exclusive message
     * @param msg Raw SysEx Message
     * @param size Length of SysEx message
     * @return true if message was passed successfully. False on any errors
     */
    bool realTime_SysEx(const uint8_t *msg, size_t size);

    /**
     * @brief Turn off all notes and mute the sound of releasing notes
     */
    void realTime_panic();

    /**
     * @brief Device switch (to extend 16-channels limit of MIDI standard)
     * @param track MIDI track index
     * @param data Device name
     * @param length Length of device name string
     */
    void realTime_deviceSwitch(size_t track, const char *data, size_t length);

    /**
     * @brief Currently selected device index
     * @param track MIDI track index
     * @return Multiple 16 value
     */
    size_t realTime_currentDevice(size_t track);

    /**
     * @brief Send raw OPL chip command
     * @param reg OPL Register
     * @param value Value to write
     */
    void realTime_rawOPL(uint8_t reg, uint8_t value);

#if defined(ADLMIDI_AUDIO_TICK_HANDLER)
    // Audio rate tick handler
    void AudioTick(uint32_t chipId, uint32_t rate);
#endif

private:
    /**
     * @brief Hardware manufacturer (Used for SysEx)
     */
    enum
    {
        Manufacturer_Roland               = 0x41,
        Manufacturer_Yamaha               = 0x43,
        Manufacturer_UniversalNonRealtime = 0x7E,
        Manufacturer_UniversalRealtime    = 0x7F
    };

    /**
     * @brief Roland Mode (Used for SysEx)
     */
    enum
    {
        RolandMode_Request = 0x11,
        RolandMode_Send    = 0x12
    };

    /**
     * @brief Device model (Used for SysEx)
     */
    enum
    {
        RolandModel_GS   = 0x42,
        RolandModel_SC55 = 0x45,
        YamahaModel_XG   = 0x4C
    };

    /**
     * @brief Process generic SysEx events
     * @param dev Device ID
     * @param realtime Is real-time event
     * @param data Raw SysEx data
     * @param size Size of given SysEx data
     * @return true when event was successfully handled
     */
    bool doUniversalSysEx(unsigned dev, bool realtime, const uint8_t *data, size_t size);

    /**
     * @brief Process events specific to Roland devices
     * @param dev Device ID
     * @param data Raw SysEx data
     * @param size Size of given SysEx data
     * @return true when event was successfully handled
     */
    bool doRolandSysEx(unsigned dev, const uint8_t *data, size_t size);

    /**
     * @brief Process events specific to Yamaha devices
     * @param dev Device ID
     * @param data Raw SysEx data
     * @param size Size of given SysEx data
     * @return true when event was successfully handled
     */
    bool doYamahaSysEx(unsigned dev, const uint8_t *data, size_t size);

private:
    /**
     * @brief Note Update properties
     */
    enum
    {
        Upd_Patch  = 0x1,
        Upd_Pan    = 0x2,
        Upd_Volume = 0x4,
        Upd_Pitch  = 0x8,
        Upd_All    = Upd_Pan + Upd_Volume + Upd_Pitch,
        Upd_Off    = 0x20,
        Upd_Mute   = 0x40,
        Upd_OffMute = Upd_Off + Upd_Mute
    };

    /**
     * @brief Update active note
     * @param MidCh MIDI Channel where note is processing
     * @param i Iterator that points to active note in the MIDI channel
     * @param props_mask Properties to update
     * @param select_adlchn Specify chip channel, or -1 - all chip channels used by the note
     */
    void noteUpdate(size_t midCh,
                    MIDIchannel::notes_iterator i,
                    unsigned props_mask,
                    int32_t select_adlchn = -1);

    /**
     * @brief Update all notes in specified MIDI channel
     * @param midCh MIDI channel to update all notes in it
     * @param props_mask Properties to update
     */
    void noteUpdateAll(size_t midCh, unsigned props_mask);

    /**
     * @brief Determine how good a candidate this adlchannel would be for playing a note from this instrument.
     * @param c Wanted chip channel
     * @param ins Instrument wanted to be used in this channel
     * @return Calculated coodness points
     */
    int64_t calculateChipChannelGoodness(size_t c, const MIDIchannel::NoteInfo::Phys &ins) const;

    /**
     * @brief A new note will be played on this channel using this instrument.
     * @param c Wanted chip channel
     * @param ins Instrument wanted to be used in this channel
     * Kill existing notes on this channel (or don't, if we do arpeggio)
     */
    void prepareChipChannelForNewNote(size_t c, const MIDIchannel::NoteInfo::Phys &ins);

    /**
     * @brief Kills note that uses wanted channel. When arpeggio is possible, note is evaluating to another channel
     * @param from_channel Wanted chip channel
     * @param j Chip channel instance
     * @param i MIDI Channel active note instance
     */
    void killOrEvacuate(
        size_t  from_channel,
        ChipChannel::users_iterator j,
        MIDIchannel::notes_iterator i);

    /**
     * @brief Off all notes and silence sound
     */
    void panic();

    /**
     * @brief Kill note, sustaining by pedal or sostenuto
     * @param MidCh MIDI channel, -1 - all MIDI channels
     * @param this_adlchn Chip channel, -1 - all chip channels
     * @param sustain_type Type of systain to process
     */
    void killSustainingNotes(int32_t midCh = -1,
                             int32_t this_adlchn = -1,
                             uint32_t sustain_type = ChipChannel::LocationData::Sustain_ANY);
    /**
     * @brief Find active notes and mark them as sostenuto-sustained
     * @param MidCh MIDI channel, -1 - all MIDI channels
     */
    void markSostenutoNotes(int32_t midCh = -1);

    /**
     * @brief Set RPN event value
     * @param MidCh MIDI channel
     * @param value 1 byte part of RPN value
     * @param MSB is MSB or LSB part of value
     */
    void setRPN(size_t midCh, unsigned value, bool MSB);

    /**
     * @brief Update portamento setup in MIDI channel
     * @param midCh MIDI channel where portamento needed to be updated
     */
    void updatePortamento(size_t midCh);

    /**
     * @brief Off the note
     * @param midCh MIDI channel
     * @param note Note to off
     * @param forceNow Do not delay the key-off to a later time
     */
    void noteOff(size_t midCh, uint8_t note, bool forceNow = false);

    /**
     * @brief Update processing of vibrato to amount of seconds
     * @param amount Amount value in seconds
     */
    void updateVibrato(double amount);

    /**
     * @brief Update auto-arpeggio
     * @param amount Amount value in seconds [UNUSED]
     */
    void updateArpeggio(double /*amount*/);

    /**
     * @brief Update Portamento gliding to amount of seconds
     * @param amount Amount value in seconds
     */
    void updateGlide(double amount);

public:
    /**
     * @brief Checks was device name used or not
     * @param name Name of MIDI device
     * @return Offset of the MIDI Channels, multiple to 16
     */
    size_t chooseDevice(const std::string &name);

    /**
     * @brief Gets a textual description of the state of chip channels
     * @param text character pointer for text
     * @param attr character pointer for text attributes
     * @param size number of characters available to write
     */
    void describeChannels(char *text, char *attr, size_t size);
};

#endif //  ADLMIDI_MIDIPLAY_HPP
