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
#ifndef BW_MIDI_SEQUENCER_HHHHPPP
#define BW_MIDI_SEQUENCER_HHHHPPP

#include <list>
#include <vector>

#include "file_reader.hpp"
#include "midi_sequencer.h"

//! Helper for unused values
#define BW_MidiSequencer_UNUSED(x) (void)x;

class BW_MidiSequencer
{
public:
    /**********************************************************************************
     *                   Public structures and types definitions                      *
     **********************************************************************************/

    /*!
     * \brief Reference to the data bank entry
     */
    struct DataBlock
    {
        size_t offset;
        size_t size;
    };

    /**
     * @brief MIDI marker entry
     */
    struct MIDI_MarkerEntry
    {
        //! Label
        DataBlock       label;
        //! Position time in seconds
        double          pos_time;
        //! Position time in MIDI ticks
        uint64_t        pos_ticks;
    };

    /**
     * @brief Container of one raw CMF instrument
     */
    struct CmfInstrument
    {
        //! Raw CMF instrument data
        uint8_t data[16];
    };

    /**
     * @brief The FileFormat enum
     */
    enum FileFormat
    {
        //! MIDI format
        Format_MIDI,
        //! CMF format
        Format_CMF,
        //! Id-Software Music File
        Format_IMF,
        //! EA-MUS format
        Format_RSXX,
        //! AIL's XMIDI format (act same as MIDI, but with exceptions)
        Format_XMIDI,
        //! KLM format
        Format_KLM,
        //! HMI/HMP format
        Format_HMI
    };

    /**
     * @brief Format of loop points implemented by CC events
     */
    enum LoopFormat
    {
        Loop_Default,
        Loop_RPGMaker = 1,
        Loop_EMIDI,
        Loop_HMI
    };

    /*!
     * \brief Get the data pointer from the data bank
     * \param b Data block reference
     * \return Pointer to the destination data
     */
    inline const uint8_t *getData(const DataBlock &b) const
    {
        return m_dataBank.data() + b.offset;
    };

    /**
     * @brief Device types to filter incompatible MIDI tracks, primarily used by HMI/HMP and EMIDI.
     * Can be combined to enable more tracks.
     */
    enum DeviceFilter
    {
        Device_GeneralMidi      = 0x0001, // MPU-401 counted as here
        Device_OPL2             = 0x0002,
        Device_OPL3             = 0x0004,
        Device_MT32             = 0x0008,
        Device_AWE32            = 0x0010,
        Device_WaveBlaster      = 0x0020,
        Device_ProAudioSpectrum = 0x0040,
        Device_SoundMan16       = 0x0080,
        Device_DIGI             = 0x0100, // Digital samples controlled by MIDI
        Device_SoundScape       = 0x0200,
        Device_WaveTable        = 0x0400,
        Device_GravisUltrasound = 0x0800,
        Device_PCSpeaker        = 0x1000,
        Device_Callback         = 0x2000,
        Device_SoundMasterII    = 0x4000,

        Device_FM               = Device_OPL2|Device_OPL3|Device_ProAudioSpectrum|Device_SoundMan16,
        Device_AdLib            = Device_OPL2,
        Device_SoundBlaster     = Device_OPL2|Device_OPL3,
        Device_ANY              = 0xFFFF
    };

private:
    /**********************************************************************************
     *                   Private structures and types definitions                     *
     **********************************************************************************/

    /*!
     * \brief Container of the error report
     */
    struct ErrString
    {
        char err[1001];
        size_t size;

        ErrString();

        void clear();
        void set(const char *str);
        void append(const char *str);
        void append(const char *str, size_t len);

        void setFmt(const char *fmt, ...);
        void appendFmt(const char *fmt, ...);

        inline const char *c_str() const
        {
            return err;
        }
    };

    /**
     * @brief Tempo value as a fraction
     */
    struct Tempo_t
    {
        uint64_t nom;
        uint64_t denom;
    };

    /**
     * @brief MIDI Event utility container
     */
    struct MidiEvent
    {
        /**
         * @brief Main MIDI event types
         */
        enum Types
        {
            //! Unknown event
            T_UNKNOWN       = 0x00,
            //! Note-Off event
            T_NOTEOFF       = 0x08,//size == 2
            //! Note-On event
            T_NOTEON        = 0x09,//size == 2
            //! Note After-Touch event
            T_NOTETOUCH     = 0x0A,//size == 2
            //! Controller change event
            T_CTRLCHANGE    = 0x0B,//size == 2
            //! Patch change event
            T_PATCHCHANGE   = 0x0C,//size == 1
            //! Channel After-Touch event
            T_CHANAFTTOUCH  = 0x0D,//size == 1
            //! Pitch-bend change event
            T_WHEEL         = 0x0E,//size == 2

            //! System Exclusive message, type 1
            T_SYSEX         = 0xF0,//size == len
            //! Sys Com Song Position Pntr [LSB, MSB]
            T_SYSCOMSPOSPTR = 0xF2,//size == 2
            //! Sys Com Song Select(Song #) [0-127]
            T_SYSCOMSNGSEL  = 0xF3,//size == 1
            //! System Exclusive message, type 2
            T_SYSEX2        = 0xF7,//size == len
            //! Special event
            T_SPECIAL       = 0xFF,

            //! Note-On with duration time (HMI and XMI only)
            T_NOTEON_DURATED  = 0x109 //size = 5
        };
        /**
         * @brief Special MIDI event sub-types
         */
        enum SubTypes
        {
            //! Sequension number
            ST_SEQNUMBER    = 0x00,//size == 2
            //! Text label
            ST_TEXT         = 0x01,//size == len, dataBank
            //! Copyright notice
            ST_COPYRIGHT    = 0x02,//size == len, dataBank
            //! Sequence track title
            ST_SQTRKTITLE   = 0x03,//size == len, dataBank
            //! Instrument title
            ST_INSTRTITLE   = 0x04,//size == len, dataBank
            //! Lyrics text fragment
            ST_LYRICS       = 0x05,//size == len, dataBank
            //! MIDI Marker
            ST_MARKER       = 0x06,//size == len, dataBank
            //! Cue Point
            ST_CUEPOINT     = 0x07,//size == len, dataBank
            //! [Non-Standard] Device Switch
            ST_DEVICESWITCH = 0x09,//size == len <CUSTOM>, dataBank
            //! MIDI Channel prefix
            ST_MIDICHPREFIX = 0x20,//size == 1

            //! End of Track event
            ST_ENDTRACK     = 0x2F,//size == 0
            //! Tempo change event
            ST_TEMPOCHANGE  = 0x51,//size == 3
            //! SMPTE offset
            ST_SMPTEOFFSET  = 0x54,//size == 5
            //! Time signature
            ST_TIMESIGNATURE = 0x55, //size == 4
            //! Key signature
            ST_KEYSIGNATURE = 0x59,//size == 2
            //! Sequencer specs
            ST_SEQUENCERSPEC = 0x7F, //size == len, dataBank

            /**************************************
             * Non-standard, internal usage only, *
             * their values larger than one byte, *
             * so, no way that any raw MIDI event *
             * would even collide with them.      *
             **************************************/
            //! [Non-Standard] Identifier of the unknown event, used for the dumper only
            ST_UNKNOWN = 0x100,

            //! [Non-Standard] Built-in song begin hook trigger
            ST_SONG_BEGIN_HOOK,

            //! [Non-Standard] Loop Start point
            ST_LOOPSTART,   //size == 0 <CUSTOM>
            //! [Non-Standard] Loop End point
            ST_LOOPEND,     //size == 0 <CUSTOM>
            //! [Non-Standard] Raw OPL data
            ST_RAWOPL,      //size == 0 <CUSTOM>

            //! [Non-Standard] Loop Start point at entire song with support of multi-loops
            ST_LOOPSTACK_BEGIN,//size == 1 <CUSTOM>
            //! [Non-Standard] Loop End point at entire song with support of multi-loops
            ST_LOOPSTACK_END, //size == 0 <CUSTOM>
            //! Loop start point with ID
            ST_LOOPSTACK_BEGIN_ID,
            //! Loop end point of ID, jump to the point with the same ID
            ST_LOOPSTACK_END_ID,
            //! [Non-Standard] Break the current loop at entire song and continue playback without loop
            ST_LOOPSTACK_BREAK, //size == 0 <CUSTOM>

            //! [Non-Standard] Callback Trigger executed from XMI and HMI formats
            ST_CALLBACK_TRIGGER, //size == 1 <CUSTOM>

            //! [Non-Standard] Loop Start point at individual track with support of multi-loops
            ST_TRACK_LOOPSTACK_BEGIN,
            //! [Non-Standard] Loop End point at individual track with support of multi-loops
            ST_TRACK_LOOPSTACK_END,
            //! Loop start point with ID
            ST_TRACK_LOOPSTACK_BEGIN_ID,
            //! Loop end point of ID, jump to the point with the same ID
            ST_TRACK_LOOPSTACK_END_ID,
            //! [Non-Standard] Break the current loop at individual track and continue playback without loop
            ST_TRACK_LOOPSTACK_BREAK,

            //! Installs a new song-wide branch tag of ID marker at the location
            ST_BRANCH_LOCATION,
            //! Jump to the location of the song-wide by ID of the branch tag
            ST_BRANCH_TO,

            //! Installs a new track-local branch tag of ID marker at the location
            ST_TRACK_BRANCH_LOCATION,
            //! Jump to the location of the track-local by ID of the branch tag
            ST_TRACK_BRANCH_TO,

            //! Lock the MIDI channel in the multi-song mapper from the stealing
            ST_CHANNEL_LOCK,
            //! Unlock the MIDI channel in the multi-song mapper
            ST_CHANNEL_UNLOCK,

            //! Enable restore state of selected controller on loop
            ST_ENABLE_RESTORE_CC_ON_LOOP,
            //! Enable calling of note-offs on loop
            ST_ENABLE_NOTEOFF_ON_LOOP,
            //! Enable restore state of selected patch program on loop
            ST_ENABLE_RESTORE_PATCH_ON_LOOP,
            //! Enable restore state of selected pitch bend wheel on loop
            ST_ENABLE_RESTORE_WHEEL_ON_LOOP,
            //! Enable restore state of selected note after-touch on loop
            ST_ENABLE_RESTORE_NOTEAFTERTOUCH_ON_LOOP,
            //! Enable restore state of selected channel after-touch/pressure on loop
            ST_ENABLE_RESTORE_CHANAFTERTOUCH_ON_LOOP,
            //! Enable restore state of all alternated controllers on loop
            ST_ENABLE_RESTORE_ALL_CC_ON_LOOP,

            //! Disable restore state of selected controller on loop
            ST_DISABLE_RESTORE_CC_ON_LOOP,
            //! Enable calling of note-offs on loop
            ST_DISABLE_NOTEOFF_ON_LOOP,
            //! Enable restore state of selected patch program on loop
            ST_DISABLE_RESTORE_PATCH_ON_LOOP,
            //! Enable restore state of selected pitch bend wheel on loop
            ST_DISABLE_RESTORE_WHEEL_ON_LOOP,
            //! Enable restore state of selected note after-touch on loop
            ST_DISABLE_RESTORE_NOTEAFTERTOUCH_ON_LOOP,
            //! Enable restore state of selected channel after-touch/pressure on loop
            ST_DISABLE_RESTORE_CHANAFTERTOUCH_ON_LOOP,
            //! Enable restore state of all alternated controllers on loop
            ST_DISABLE_RESTORE_ALL_CC_ON_LOOP,

            //! Set the priority of MIDI channel for the stealing algorithm
            ST_CHANNEL_PRIORITY,

            //! Tail of the enum, is not supposed to be used
            ST_TYPE_LAST = ST_TRACK_BRANCH_TO
        };

        //! Main type of event
        uint_fast16_t type;
        //! Sub-type of the event
        uint_fast16_t subtype;
        //! Targeted MIDI channel
        uint_fast16_t channel;
        //! Is valid event
        uint_fast16_t isValid;
        //! 4 bytes of locally placed data bytes
        uint8_t data_loc[5];
        uint8_t data_loc_size;
        //! Larger data blocks such as SysEx queries
        DataBlock data_block;
    };

    /*!
     * \brief Individual tempo value used for the timeline calculation
     */
    struct TempoEvent
    {
        uint64_t tempo;
        uint64_t absPosition;
    };

    /**
     * @brief A track position event contains a chain of MIDI events until next delay value
     *
     * Created with purpose to sort events by type in the same position
     * (for example, to keep controllers always first than note on events or lower than note-off events)
     */
    class MidiTrackRow
    {
    public:
        MidiTrackRow();

        /*!
         * \brief Clear MIDI row data
         */
        void clear();

        //! Absolute time position in seconds
        double time;
        //! Delay to next event in ticks
        uint64_t delay;
        //! Absolute position in ticks
        uint64_t absPos;
        //! Delay to next event in seconds
        double timeDelay;
        //! Begin of the events row stored in the bank
        size_t events_begin;
        //! End of the events row stored in the bank
        size_t events_end;

        static int typePriority(const MidiEvent &evt);
        /**
         * @brief Sort events in this position
         * @param noteStates Buffer of currently pressed/released note keys in the track
         */
        void sortEvents(std::vector<MidiEvent> &eventsBank, bool *noteStates = NULL);
    };

    /**
     * @brief Tempo change point entry. Used in the MIDI data building function only.
     */
    struct TempoChangePoint
    {
        uint64_t absPos;
        Tempo_t tempo;
    };
    //P.S. I declared it here instead of local in-function because C++98 can't process templates with locally-declared structures

    typedef std::list<MidiTrackRow> MidiTrackQueue;

    /**
     * @brief The print left by the Note-On event with a duration supplied. Once it expires, the Note-Off event should be sent.
     */
    struct DuratedNote
    {
        int64_t ttl;
        uint8_t channel;
        uint8_t note;
        uint8_t velocity;
    };

    /**
     * @brief The per-track storage of active durated notes before they will expire.
     */
    struct DuratedNotesCache
    {
        DuratedNote notes[128];
        size_t notes_count;
    };

    /**
     * @brief The TrackStateRestore class
     */
    enum TrackRestoreSetup
    {
        TRACK_RESTORE_NONE        = 0x00,
        TRACK_RESTORE_CC          = 0x01,
        TRACK_RESTORE_NOTEOFFS    = 0x02,
        TRACK_RESTORE_PATCH       = 0x04,
        TRACK_RESTORE_WHEEL       = 0x08,
        TRACK_RESTORE_NOTE_ATT    = 0x10,
        TRACK_RESTORE_CHAN_ATT    = 0x20,
        TRACK_RESTORE_ALL_CC      = 0x40,

        TRACK_RESTORE_DEFAULT     = TRACK_RESTORE_NOTEOFFS,
        TRACK_RESTORE_DEFAULT_HMI = TRACK_RESTORE_ALL_CC|TRACK_RESTORE_NOTEOFFS|TRACK_RESTORE_PATCH|
                                    TRACK_RESTORE_WHEEL|TRACK_RESTORE_NOTE_ATT|TRACK_RESTORE_CHAN_ATT
    };

    /**
     * @brief Saved track state
     */
    struct TrackStateSaved
    {
        //! MIDI Channel that track uses right now (if track contains multi-channel events, this thing will fail!)
        uint8_t track_channel;
        //! On loop, restore state for controllers
        uint8_t cc_to_restore[102];
        //! Values of saved controllers
        uint8_t cc_values[102];
        //! Reserved patch
        uint8_t reserve_patch;
        //! Reserved pitch bend value
        uint8_t reserve_wheel[2];
        //! Reserved note after-touch
        uint8_t reserve_note_att[128];
        //! Reserved channel after-touch
        uint8_t reserve_channel_att;
    };

    /**
     * @brief Song position context
     */
    struct Position
    {
        //! Track information
        struct TrackInfo
        {
            //! MIDI Events queue position iterator
            MidiTrackQueue::iterator pos;
            //! Delay to next event in a track
            uint64_t delay;
            //! Last handled event type
            int32_t lastHandledEvent;
            //! Track's local state
            TrackStateSaved state;

            TrackInfo();

            TrackInfo(const TrackInfo &o);

            TrackInfo &operator=(const TrackInfo &o);
        };

        //! Waiting time before next event in seconds
        double wait;
        //! Absolute time position on the track in seconds
        double absTimePosition;
        //! Absolute MIDI tick position on the song
        uint64_t absTickPosition;
        //! Was track began playing
        bool began;
        //! Per-track info remembered by the position state
        std::vector<TrackInfo> track;

        Position();
        void clear();
        void assignOneTrack(const Position *o, size_t tk);
    };

    struct SequencerTime
    {
        //! Time buffer
        double   timeRest;
        //! Sample rate
        uint32_t sampleRate;
        //! Size of one frame in bytes
        uint32_t frameSize;
        //! Minimum possible delay, granuality
        double minDelay;
        //! Last delay
        double delay;

        void init();
        void reset();
    };

    enum LoopTypes
    {
        GLOBAL_LOOP = 0x01,
        GLOBAL_LOOPSTACK = 0x02,
        LOCAL_LOOPSTACK = 0x03
    };

    /**
     * @brief The loop state structure using during data parse of the song
     */
    struct LoopPointParseState
    {
        //! Tick position of loop start tag
        uint64_t loopStartTicks;
        //! Tick position of loop end tag
        uint64_t loopEndTicks;
        //! Full length of song in ticks
        uint64_t ticksSongLength;
        //! Got any loop events in currently processing row? (Must be reset after flushing the events row!)
        unsigned gotLoopEventsInThisRow;
        //! Got any global loop start event
        bool gotLoopStart;
        //! Got any loop end event
        bool gotLoopEnd;
        //! Got any stacked loop start event
        bool gotStackLoopStart;
        //! Was the local loop start event got? (Must be reset on next track start!)
        bool gotTrackStackLoopStart;
    };

    /**
     * @brief The loop state structure usind during playback
     */
    struct LoopRuntimeState
    {
        //! Time value of last stacked loop end caught
        double   stackLoopEndsTime;
        //! Number of global loop start events caught
        unsigned numGlobLoopStarts;
        //! Number of stacked loop start events caught
        unsigned numStackLoopStarts;
        //! Number of stacked loop end events caught
        unsigned numStackLoopEnds;
        //! Number of stacked loop end events caught
        unsigned numStackLoopBreaks;
        //! Number of branch jumps caught
        unsigned numBranchJumps;
        //! Shall global loop jump to be performed?
        bool     doLoopJump;
    };

    /**
     * @brief Loop stack entry
     */
    struct LoopStackEntry
    {
        LoopStackEntry();

        //! is infinite loop
        bool infinity;
        //! Count of loops left to break. <0 - infinite loop
        int loops;
        //! Start position snapshot to return back
        Position startPosition;
        //! Loop start tick
        uint64_t start;
        //! Loop end tick
        uint64_t end;
        //! ID of loop entry (by default unused)
        uint16_t id;
    };

    static const uint32_t LOOP_STACK_NO_ID = 0xFFFF;

    /**
     * @brief The named tag that allows to jump to here from anywhere
     */
    struct BranchEntry
    {
        //! Position snapshot where branch is placed
        Position offset;
        //! Tick
        uint64_t tick;
        //! Owner track (if 0xFFFFFFFF, branch is global)
        uint32_t track;
        //! ID of the branch
        uint16_t id;
        //! Is track position built?
        bool init;
    };

    static const uint32_t BRANCH_GLOBAL_TRACK = 0xFFFFFFFF;

    struct LoopState
    {
        //! Loop start has reached
        bool    caughtStart;
        //! Loop end has reached, reset on handling
        bool    caughtEnd;

        //! Loop start has reached
        bool    caughtStackStart;
        //! Loop next has reached, reset on handling
        bool    caughtStackEnd;
        //! Loop break has reached, reset on handling
        bool    caughtStackBreak;
        //! Skip next stack loop start event handling
        bool    skipStackStart;
        //! Destination loop stack ID
        uint16_t dstLoopStackId;

        //! Are loop points invalid?
        bool    invalidLoop; /*Loop points are invalid (loopStart after loopEnd or loopStart and loopEnd are on same place)*/

        //! Is look got temporarily broken because of post-end seek?
        bool    temporaryBroken;

        //! How much times the loop should start repeat? For example, if you want to loop song twice, set value 1
        int     loopsCount;

        //! how many loops left until finish the song
        int     loopsLeft;

        bool        caughtBranchJump;
        uint16_t    dstBranchId;

        static const size_t stackDepthMax = 127;
        //! Stack of nested loops
        LoopStackEntry      stack[stackDepthMax];
        //! Current size of the loop stack
        size_t              stackDepth;
        //! Current level on the loop stack (<0 - out of loop, 0++ - the index in the loop stack)
        int                 stackLevel;

        //! Constructor to initialize member variables
        LoopState();

        /**
         * @brief Reset loop state to initial
         */
        void reset();
        void fullReset();

        bool isStackEnd();

        void stackUp(int count = 1);

        void stackDown(int count = 1);

        LoopStackEntry &getCurStack();
    };

    /**
     * @brief Handler of callback trigger events
     * @param userData Pointer to user data (usually, context of something)
     * @param trigger Value of the event which triggered this callback.
     * @param track Identifier of the track which triggered this callback.
     */
    typedef void (*TriggerHandler)(void *userData, unsigned trigger, size_t track);

    /**
     * @brief The state of the MIDI track
     */
    struct MidiTrackState
    {
        //! Cache of active durated notes per track
        DuratedNotesCache duratedNotes;
        //! Track-local loop state
        LoopState loop;
        //! Device designation mask (don't play this track if no match with sequencer setup)
        uint32_t deviceMask;
        //! Disable handling of this track
        bool disabled;
        //! Track's current state (gets captured on the fly)
        TrackStateSaved state;
        //! Track's state restore setup
        uint32_t stateRestoreSetup;

        //! Constructor to initialize member variables
        MidiTrackState();
    };

    /**********************************************************************************
     *                      Private variable fields definitions                       *
     **********************************************************************************/

    //! MIDI Output interface context
    const BW_MidiRtInterface *m_interface;

    //! Storage of data block refered in tracks
    std::vector<uint8_t> m_dataBank;

    //! Array of all MIDI events across all tracks
    std::vector<MidiEvent> m_eventBank;

    //! The number of track of multi-track file (for exmaple, XMI) to load
    int m_loadTrackNumber;

    //! Handler of callback trigger events
    TriggerHandler m_triggerHandler;
    //! User data of callback trigger events
    void *m_triggerUserData;



    // TO MOVE INTO MIDISong!

    //! Music file format type. MIDI is default.
    FileFormat m_format;
    //! SMF format identifier.
    unsigned m_smfFormat;
    //! Loop points format
    LoopFormat m_loopFormat;

    //! Current position
    Position m_currentPosition;
    //! Track begin position
    Position m_trackBeginPosition;
    //! Loop start point
    Position m_loopBeginPosition;

    //! Is looping enabled or not
    bool    m_loopEnabled;
    //! Don't process loop: trigger hooks only if they are set
    bool    m_loopHooksOnly;

    //! Full song length in seconds
    double m_fullSongTimeLength;
    //! Delay after song playd before rejecting the output stream requests
    double m_postSongWaitDelay;

    //! Global loop start time
    double m_loopStartTime;
    //! Global loop end time
    double m_loopEndTime;

    //! Pre-processed track data storage
    std::vector<MidiTrackQueue> m_trackData;

    //! State of every MIDI track
    std::vector<MidiTrackState> m_trackState;

    //! List of available branches
    std::vector<BranchEntry> m_branches;

    //! Song-wide on-loop state restore setup
    uint32_t m_stateRestoreSetup;

    //! Current count of MIDI tracks
    size_t m_tracksCount;

    //! CMF instruments
    std::vector<CmfInstrument> m_cmfInstruments;

    //! Title of music
    DataBlock m_musTitle;
    //! Copyright notice of music
    DataBlock m_musCopyright;
    //! List of track titles
    std::vector<DataBlock> m_musTrackTitles;
    //! List of MIDI markers
    std::vector<MIDI_MarkerEntry> m_musMarkers;

    //! Time of one tick
    Tempo_t m_invDeltaTicks;
    //! Current tempo
    Tempo_t m_tempo;
    //! Is song at end
    bool    m_atEnd;

    //! Set the number of loops limit. Lesser than 0 - loop infinite
    int     m_loopCount;

    //! Current filter (by default "Allow everything", for some formats by default the "FM" is set)
    uint32_t m_deviceMask;

    //! Complete mask that includes all supported devices by loaded files (if 0xFFFF, then file doesn't use track filtering)
    uint32_t m_deviceMaskAvailable;


    //! The XMI-specific list of raw songs, converted into SMF format
    std::vector<std::vector<uint8_t > > m_rawSongsData;

    //! The state of the loop
    LoopState m_loop;

    //! Index of solo track, or max for disabled
    size_t m_trackSolo;
    //! MIDI channel disable (exception for extra port-prefix-based channels)
    bool m_channelDisable[16];


    // KEEP HERE AS A GLOBAL STATE

    //! Global tempo multiplier factor
    double  m_tempoMultiplier;
    //! File parsing errors string (adding into m_errorString on aborting of the process)
    ErrString m_parsingErrorsString;
    //! Common error string
    ErrString m_errorString;
    //! Sequencer's time processor
    SequencerTime m_time;

    /**********************************************************************************
     *                             Tempo fraction                                     *
     **********************************************************************************/
    /**
     * @brief Convert fraction into double
     * @param tempo Tempo fraction value
     * @return Double value converted from the fraction
     */
    static inline double tempo_get(Tempo_t *tempo) { return tempo->nom / (double)tempo->denom; }

    /**
     * @brief Multiple tempo fraction by integer
     * @param out Product output
     * @param val1 Tempo fraction multiplier
     * @param val2 Integer multiplier
     */
    static void tempo_mul(Tempo_t *out, const Tempo_t *val1, uint64_t val2);

    /**
     * @brief Optimize the tempo fraction
     * @param tempo Tempo fraction to optimize
     */
    static void tempo_optimize(Tempo_t *tempo);


    /**********************************************************************************
     *                             Durated note                                       *
     **********************************************************************************/

    bool duratedNoteAlloc(size_t track, DuratedNote **note);
    void duratedNoteClear();
    void duratedNoteTick(size_t track, int64_t ticks);
    void duratedNotePop(size_t track, size_t i);



    /**********************************************************************************
     *                                 Loop                                           *
     **********************************************************************************/

    /**
     * @brief Installs the final state of the loop state after all the music data parsed
     * @param loopState The loop state structure resulted after everything got been passed
     */
    void installLoop(LoopPointParseState &loopState);

    /**
     * @brief Sets the global or local loop stack begin state
     * @param loopState Loop state for the currently parsing music file
     * @param dstLoop Target loop state (global or track-local)
     * @param event Event that contains input data
     * @param abs_position Absolute position
     * @param type Type of loop to track
     */
    void setLoopStackStart(LoopPointParseState &loopState, LoopState *dstLoop, const MidiEvent &event, uint64_t abs_position, unsigned type);

    /**
     * @brief Sets the global or local loop stack end state
     * @param loopState Loop state for the currently parsing music file
     * @param dstLoop Target loop state (global or track-local)
     * @param abs_position Absolute position
     * @param type Type of loop to track
     */
    void setLoopStackEnd(LoopPointParseState &loopState, LoopState *dstLoop, uint64_t abs_position, unsigned type);

    /**
     * @brief During the parse, inspects the trackLoop event and according to the current trackLoop state, installs the necessary properties
     * @param loopState Loop state for the currently parsing music file
     * @param event An event entry that was been proceeded just now
     * @param abs_position Current in-track absolute position
     */
    void analyseLoopEvent(LoopPointParseState &loopState, const MidiEvent &event, uint64_t abs_position, LoopState *trackLoop);



    /**********************************************************************************
     *                                 Data bank                                      *
     **********************************************************************************/

    static void insertDataToBank(MidiEvent &evt, std::vector<uint8_t> &bank, const uint8_t *data, size_t length);
    static void insertDataToBank(MidiEvent &evt, std::vector<uint8_t> &bank, FileAndMemReader &fr, size_t length);
    static void insertDataToBankWithByte(MidiEvent &evt, std::vector<uint8_t> &bank, uint8_t begin_byte, const uint8_t *data, size_t length);
    static void insertDataToBankWithByte(MidiEvent &evt, std::vector<uint8_t> &bank, uint8_t begin_byte, FileAndMemReader &fr, size_t length);
    static void insertDataToBankWithTerm(MidiEvent &evt, std::vector<uint8_t> &bank, const uint8_t *data, size_t length);
    static void insertDataToBankWithTerm(MidiEvent &evt, std::vector<uint8_t> &bank, FileAndMemReader &fr, size_t length);
    void addEventToBank(MidiTrackRow &row, const MidiEvent &evt);


    /**********************************************************************************
     *                                 MIDI Data                                      *
     **********************************************************************************/

    /**
     * @brief Prepare internal events storage for track data building
     * @param trackCount Count of tracks
     */
    void buildSmfSetupReset(size_t trackCount);

    void buildSmfResizeTracks(size_t tracksCount);

    /**
     * @brief Initialize begin of the track after it completely filled by the data
     * @param track Index of track to initialize
     */
    void initTracksBegin(size_t track);

    /**
     * @brief Build the time line from off loaded events
     * @param tempos Pre-collected list of tempo events
     * @param loopStartTicks Global loop start tick (give zero if no global loop presented)
     * @param loopEndTicks Global loop end tick (give zero if no global loop presented)
     *
     * IMPORTANT: This function MUST be called after parsing any file format!
     */
    void buildTimeLine(const std::vector<TempoEvent> &tempos,
                       uint64_t loopStartTicks = 0,
                       uint64_t loopEndTicks = 0);


    /**********************************************************************************
     *                                 Process                                        *
     **********************************************************************************/

    /**
     * @brief Handle one event from the chain
     * @param tk MIDI track
     * @param evt MIDI event entry
     * @param status Recent event type, -1 returned when end of track event was handled.
     */
    void handleEvent(size_t tk, const MidiEvent &evt, int32_t &status);

    /**
     * @brief Run processing of active durated notes, trigger true Note-OFF events for expired notes
     * @param track Track where to run the operation
     * @param status [_out] Last-triggered event (Note-Off) will be returned here
     */
    void processDuratedNotes(size_t track, int32_t &status);

    /**
     * @brief Check the state of caught loop start points
     * @param state Runtime state (for the track or for the global row)
     * @param loop Loop state (for the track or for the entire song)
     * @param tk Track info structure
     * @param glob Is global loop or local?
     */
    void handleLoopStart(LoopRuntimeState &state, LoopState &loop, Position::TrackInfo &tk, bool glob);

    /**
     * @brief Check the state of caught loop end points
     * @param state Runtime state (for the track or for the global row)
     * @param loop Loop state (for the track or for the entire song)
     * @param tk Track info structure
     * @param glob Is global loop or local?
     * @return true if it's required to stop further handling of events in this row (track or entire row)
     */
    bool handleLoopEnd(LoopRuntimeState &state, LoopState &loop, Position::TrackInfo &tk, bool glob);



    /**
     * @brief Process all caught loop points
     * @param state Runtime state (for the track or for the global row)
     * @param loop Loop state (for the track or for the entire song)
     * @param glob Is global loop or local?
     * @param tk Track number, used for local loops only, for global loop checks is unused
     * @param pos Begin of the current row position
     * @return true if it's required to don't process the global loop end and end of the song
     */
    bool processLoopPoints(LoopRuntimeState &state, LoopState &loop, bool glob, size_t tk, const Position &pos);

    void restoreSongState();

    void restoreTrackState(size_t track);

    /**
     * @brief Changes current playback position to the state of `pos`
     * @param track Track number to change position or 0xFFFFFFFF of entire song
     * @param pos Position state structure.
     *
     * IMPORTANT: Tracks count in structure must match the song's tracks, otherwise the thing will crash.
     * For single-track positions it's allowed to have only one track entry.
     */
    void jumpToPosition(size_t track, const Position *pos);

    /**
     * @brief Jump to selected branch
     * @param dstTrack Jump to the track-local branch or to global branch (if specify 0xFFFFFFFF value)
     * @param dstBranch Branch ID to jump
     * @return true if jump happen, or false if branch ID and track combo was not found
     */
    bool jumpToBranch(uint32_t dstTrack, uint16_t dstBranch);

    /**
     * @brief Process MIDI events on the current tick moment
     * @param isSeek is a seeking process
     * @return returns false on reaching end of the song
     */
    bool processEvents(bool isSeek = false);


    /**********************************************************************************
     *                             Private file parser functions                      *
     **********************************************************************************/


    /**********************************************************************************
     *                          Parse Standard MIDI File                              *
     **********************************************************************************/

    /**
     * @brief Build MIDI track data from the raw track data storage
     * @param fr File read handler
     * @param tracks_offset Absolute offset where tracks data begins
     * @param tracks_count Total number of tracks stored in the file
     * @return true if everything successfully processed, or false on any error
     */
    bool smf_buildTracks(FileAndMemReader &fr, const size_t tracks_offset, const size_t tracks_count);

    /**
     * @brief Build data for the single track (without header)
     * @param fr File read handler
     * @param track_idx Inndex of currently parsing track (at 0)
     * @return true if everything successfully processed, or false on any error
     */
    bool smf_buildOneTrack(FileAndMemReader &fr, const size_t track_idx, const size_t track_size,
                           std::vector<TempoEvent> &temposList, LoopPointParseState &loopState);

    /**
     * @brief Parse one event from raw MIDI track stream
     * @param [_inout] ptr pointer to pointer to current position on the raw data track
     * @param [_in] end Address to end of raw track data, needed to validate position and size
     * @param [_inout] status Status of the track processing
     * @return Parsed MIDI event entry
     */
    MidiEvent smf_parseEvent(FileAndMemReader &fr, const size_t end, int &status);

    /**
     * @brief Finalize the MIDI track row and start a new one, additionally increase the abs_position by delay
     * @param evtPos MIDI track row entry prepared to be saved
     * @param abs_position Absolute position counter
     * @param track_num Number of track for which this row is
     * @param loopState Parse loop state
     * @param finish Is this a final row for the track?
     */
    void smf_flushRow(MidiTrackRow &evtPos, uint64_t &abs_position, size_t track_num, LoopPointParseState &loopState, bool finish = false);

    /**
     * @brief Load file as Standard MIDI file
     * @param fr Context with opened file
     * @return true on successful load
     */
    bool parseSMF(FileAndMemReader &fr);

    /**
     * @brief Load file as RIFF MIDI
     * @param fr Context with opened file
     * @return true on successful load
     */
    bool parseRMI(FileAndMemReader &fr);



    /**********************************************************************************
     *                                Parse GMF File                                  *
     **********************************************************************************/
    /**
     * @brief Load file as GMD/MUS files (ScummVM)
     * @param fr Context with opened file
     * @return true on successful load
     */
    bool parseGMF(FileAndMemReader &fr);



    /**********************************************************************************
     *                              Parse DMX MUS File                                *
     **********************************************************************************/
    /**
     * @brief Load file as DMX MUS file (Doom)
     * @param fr Context with opened file
     * @return true on successful load
     */
    bool parseMUS(FileAndMemReader &fr);


    /**********************************************************************************
     *                Parse HMI Sound Operating System specific MIDI File             *
     **********************************************************************************/
    enum HMIDriver
    {
        HMI_DRIVER_SOUND_MASTER_II    = 0xA000,
        HMI_DRIVER_MPU_401            = 0xA001,
        HMI_DRIVER_FM                 = 0xA002,
        HMI_DRIVER_OPL2               = 0xA002,
        HMI_DRIVER_CALLBACK           = 0xA003,
        HMI_DRIVER_MT_32              = 0xA004,
        HMI_DRIVER_DIGI               = 0xA005,
        HMI_DRIVER_INTERNAL_SPEAKER   = 0xA006,
        HMI_DRIVER_WAVE_TABLE_SYNTH   = 0xA007,
        HMI_DRIVER_AWE32              = 0xA008,
        HMI_DRIVER_OPL3               = 0xA009,
        HMI_DRIVER_GUS                = 0xA00A
    };

    struct HMITrackDir
    {
        size_t start;
        size_t len;
        size_t end;
        size_t offset;
        size_t midichan;
        size_t devicesNum;
        uint16_t devices[8];
    };

    typedef uint64_t (*HMIVarLenReadCB)(FileAndMemReader &fr, const size_t end, bool &ok);

    struct HMIData
    {
        char magic[32];
        size_t branch_offset;
        size_t tracksCount;
        size_t tpqn;
        size_t division;
        size_t timeDuration;
        uint32_t priorities[16];
        uint32_t trackDevice[32][5];
        uint8_t controlInit[128];

        size_t track_dir_offset;
        size_t tracks_offset;
        size_t current_track;

        bool isHMP;
        // Helper function pointers
        HMIVarLenReadCB fReadVarLen;
    };

    static bool hmi_cc_restore_on_cvt(MidiEvent *event);
    static bool hmi_cc_restore_off_cvt(MidiEvent *event);

    /**
     * @brief Parse single event for the HMI/HMP music file
     * @param hmi_data Header data of the HMI/HMP file
     * @param d Directory entry of current track
     * @param fr File/Memory read context
     * @param event [inout] MIDI event entry
     * @param status [inout] MIDI status value
     * @return true on success, false on any parse error ocurred
     */
    bool hmi_parseEvent(const HMIData &hmi_data, const HMITrackDir &d, FileAndMemReader &fr, MidiEvent &event, int &status);

    /**
     * @brief Load file as HMI/HMP file for the HMI Sound Operating System
     * @param fr Context with opened file
     * @return true on successful load
     */
    bool parseHMI(FileAndMemReader &fr);

#ifndef BWMIDI_DISABLE_XMI_SUPPORT
    /**********************************************************************************
     *                     Parse Miles Sound System's XMIDI File                      *
     **********************************************************************************/
    /**
     * @brief Load file as AIL eXtended MIdi
     * @param fr Context with opened file
     * @return true on successful load
     */
    bool parseXMI(FileAndMemReader &fr);
#endif


#ifdef BWMIDI_ENABLE_OPL_MUSIC_SUPPORT
    /**********************************************************************************
     *                            Parse Id's Music File                               *
     **********************************************************************************/
    /**
     * @brief Load file as Id-software-Music-File (Wolfenstein)
     * @param fr Context with opened file
     * @return true on successful load
     */
    bool parseIMF(FileAndMemReader &fr);

    /**********************************************************************************
     *                       Parse Wacky Wheels Music File                            *
     **********************************************************************************/
    /**
     * @brief Load file as Wacky Wheels KLM file
     * @param fr Context with opened file
     * @return true on successful load
     */
    bool parseKLM(FileAndMemReader &fr);

    /**********************************************************************************
     *                         Parse EA's MUS Music File                              *
     **********************************************************************************/
    /**
     * @brief Load file as EA MUS
     * @param fr Context with opened file
     * @return true on successful load
     */
    bool parseRSXX(FileAndMemReader &fr);

    /**********************************************************************************
     *                         Parse Creative Music File                              *
     **********************************************************************************/
    /**
     * @brief Load file as Creative Music Format
     * @param fr Context with opened file
     * @return true on successful load
     */
    bool parseCMF(FileAndMemReader &fr);
#endif


public:
    /**********************************************************************************
     *                             Public functions API                               *
     **********************************************************************************/

    BW_MidiSequencer();
    virtual ~BW_MidiSequencer();

    /**
     * @brief Sets the RT interface
     * @param intrf Pre-Initialized interface structure (pointer will be taken)
     */
    void setInterface(const BW_MidiRtInterface *intrf);

    /**
     * @brief Returns file format type of currently loaded file
     * @return File format type enumeration
     */
    FileFormat getFormat();

    /**
     * @brief Returns the number of tracks
     * @return Track count
     */
    size_t getTrackCount() const;

    /**
     * @brief Sets whether a track is playing
     * @param track Track identifier
     * @param enable Whether to enable track playback
     * @return true on success, false if there was no such track
     */
    bool setTrackEnabled(size_t track, bool enable);

    /**
     * @brief Disable/enable a channel is sounding
     * @param channel Channel number from 0 to 15
     * @param enable Enable the channel playback
     * @return true on success, false if there was no such channel
     */
    bool setChannelEnabled(size_t channel, bool enable);

    /**
     * @brief Enables or disables solo on a track
     * @param track Identifier of solo track, or max to disable
     */
    void setSoloTrack(size_t track);

    /**
     * @brief Set the song number of a multi-song file (such as XMI)
     * @param trackNumber Identifier of the song to load (or -1 to mix all songs as one song)
     */
    void setSongNum(int track);

    /**
     * @brief Set the device mask to allow tracks being played. Default value is Device_ANY.
     * @param devMask A mask built from values of @{DeviceFilter} enumeration using OR operation
     *
     * Set it before loading the song file, otherwise excluded tracks will never appear in the final list.
     */
    void setDeviceMask(uint32_t devMask);

    /**
     * @brief If debug handler is installed, the list of supported device types by MIDI file will be printed
     */
    void debugPrintDevices();

    /**
     * @brief Retrive the number of songs in a currently opened file
     * @return Number of songs in the file. If 1 or less, means, the file has only one song inside.
     */
    int getSongsCount();

    /**
     * @brief Defines a handler for callback trigger events
     * @param handler Handler to invoke from the sequencer when triggered, or NULL.
     * @param userData Instance of the library
     */
    void setTriggerHandler(TriggerHandler handler, void *userData);

    /**
     * @brief Get the list of CMF instruments (CMF only)
     * @return Array of raw CMF instruments entries
     */
    const std::vector<CmfInstrument> getRawCmfInstruments();

    /**
     * @brief Get string that describes reason of error
     * @return Error string
     */
    const char *getErrorString() const;

    /**
     * @brief Check is loop enabled
     * @return true if loop enabled
     */
    bool getLoopEnabled();

    /**
     * @brief Switch loop on/off
     * @param enabled Enable loop
     */
    void setLoopEnabled(bool enabled);

    /**
     * @brief Get the number of loops set
     * @return number of loops or -1 if loop infinite
     */
    int getLoopsCount();

    /**
     * @brief How many times song should loop
     * @param loops count or -1 to loop infinite
     */
    void setLoopsCount(int loops);

    /**
     * @brief Switch loop hooks-only mode on/off
     * @param enabled Don't loop: trigger hooks only without loop
     */
    void setLoopHooksOnly(bool enabled);

    /**
     * @brief Get music title
     * @return music title string
     */
    const char *getMusicTitle() const;

    /**
     * @brief Get music copyright notice
     * @return music copyright notice string
     */
    const char *getMusicCopyright() const;

    /**
     * @brief Get list of track titles
     * @return array of track title strings
     */
    const std::vector<DataBlock> &getTrackTitles();

    /**
     * @brief Get list of MIDI markers
     * @return Array of MIDI marker structures
     */
    const std::vector<MIDI_MarkerEntry> &getMarkers();


    /**********************************************************************************
     *                                 Load music                                     *
     **********************************************************************************/

    /**
     * @brief Load MIDI file from path
     * @param filename Path to file to open
     * @return true if file successfully opened, false on any error
     */
    bool loadMIDI(const std::string &filename);

    /**
     * @brief Load MIDI file from a memory block
     * @param data Pointer to memory block with MIDI data
     * @param size Size of source memory block
     * @return true if file successfully opened, false on any error
     */
    bool loadMIDI(const void *data, size_t size);

    /**
     * @brief Load MIDI file by using FileAndMemReader interface
     * @param fr FileAndMemReader context with opened source file
     * @return true if file successfully opened, false on any error
     */
    bool loadMIDI(FileAndMemReader &fr);

#ifdef BWMIDI_ENABLE_DEBUG_SONG_DUMP
    /**
     * @brief Dump all the currently loaded content of the song as a text file
     * @param outFile Output text file to write the output
     * @return true on success, false on any error occurred
     */
    bool debugDumpContents(const std::string &outFile);
    bool debugDumpContents(FILE *output);

    friend const char *evtName(BW_MidiSequencer::MidiEvent::Types type, BW_MidiSequencer::MidiEvent::SubTypes subType);
#endif

    /**********************************************************************************
     *                                 Input/Output                                   *
     **********************************************************************************/

    /**
     * @brief Periodic tick handler.
     * @param s seconds since last call
     * @param granularity don't expect intervals smaller than this, in seconds
     * @return desired number of seconds until next call
     */
    double Tick(double s, double granularity);

    /**
     * @brief Runs ticking in a sync with audio streaming. Use this together with onPcmRender hook to easily play MIDI.
     * @param stream pointer to the output PCM stream
     * @param length length of the buffer in bytes
     * @return Count of recorded data in bytes
     */
    int playStream(uint8_t *stream, size_t length);

    /**
     * @brief Change current position to specified time position in seconds
     * @param granularity don't expect intervals smaller than this, in seconds
     * @param seconds Absolute time position in seconds
     * @return desired number of seconds until next call of Tick()
     */
    double seek(double seconds, const double granularity);

    /**
     * @brief Gives current time position in seconds
     * @return Current time position in seconds
     */
    double  tell();

    /**
     * @brief Gives time length of current song in seconds
     * @return Time length of current song in seconds
     */
    double  timeLength();

    /**
     * @brief Gives loop start time position in seconds
     * @return Loop start time position in seconds or -1 if song has no loop points
     */
    double  getLoopStart();

    /**
     * @brief Gives loop end time position in seconds
     * @return Loop end time position in seconds or -1 if song has no loop points
     */
    double  getLoopEnd();

    /**
     * @brief Return to begin of current song
     */
    void    rewind();

    /**
     * @brief Is position of song at end
     * @return true if end of song was reached
     */
    bool positionAtEnd();

    /**
     * @brief Get current tempor multiplier value
     * @return
     */
    double getTempoMultiplier();

    /**
     * @brief Set tempo multiplier
     * @param tempo Tempo multiplier: 1.0 - original tempo. >1 - faster, <1 - slower
     */
    void   setTempo(double tempo);
};

#endif /* BW_MIDI_SEQUENCER_HHHHPPP */
