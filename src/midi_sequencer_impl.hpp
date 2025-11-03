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

#include "midi_sequencer.hpp"
#include <stdio.h>
#include <cstring>
#include <stack>
#include <cerrno>
#include <iterator>  // std::back_inserter
#include <algorithm> // std::copy
#include <assert.h>

#if defined(VITA)
#define timingsafe_memcmp  timingsafe_memcmp_workaround // Workaround to fix the C declaration conflict
#include <psp2kern/kernel/sysclib.h> // snprintf
#endif

#if defined(_WIN32) && !defined(__WATCOMC__)
#   ifdef _MSC_VER
#       ifdef _WIN64
typedef __int64 ssize_t;
#       else
typedef __int32 ssize_t;
#       endif
#   else
#       ifdef _WIN64
typedef int64_t ssize_t;
#       else
typedef int32_t ssize_t;
#       endif
#   endif
#endif

#if defined(_MSC_VER) && _MSC_VER < 1900

#define snprintf c99_snprintf
#define vsnprintf c99_vsnprintf

__inline int c99_vsnprintf(char *outBuf, size_t size, const char *format, va_list ap)
{
    int count = -1;

    if (size != 0)
        count = _vsnprintf_s(outBuf, size, _TRUNCATE, format, ap);
    if (count == -1)
        count = _vscprintf(format, ap);

    return count;
}

__inline int c99_snprintf(char *outBuf, size_t size, const char *format, ...)
{
    int count;
    va_list ap;

    va_start(ap, format);
    count = c99_vsnprintf(outBuf, size, format, ap);
    va_end(ap);

    return count;
}
#endif

#ifndef BWMIDI_DISABLE_XMI_SUPPORT
#include "cvt_xmi2mid.hpp"
#endif // XMI

/**
 * @brief Utility function to read Big-Endian integer from raw binary data
 * @param buffer Pointer to raw binary buffer
 * @param nbytes Count of bytes to parse integer
 * @return Extracted unsigned integer
 */
static inline uint64_t readBEint(const void *buffer, size_t nbytes)
{
    uint64_t result = 0;
    const uint8_t *data = reinterpret_cast<const uint8_t *>(buffer);

    for(size_t n = 0; n < nbytes; ++n)
    {
        result <<= 8;
        result |= data[n];
    }

    return result;
}

/**
 * @brief Utility function to read Little-Endian integer from raw binary data
 * @param buffer Pointer to raw binary buffer
 * @param nbytes Count of bytes to parse integer
 * @return Extracted unsigned integer
 */
static inline uint64_t readLEint(const void *buffer, size_t nbytes)
{
    uint64_t result = 0;
    const uint8_t *data = reinterpret_cast<const uint8_t *>(buffer);

    for(size_t n = 0; n < nbytes; ++n)
        result += static_cast<uint64_t>(data[n] << (n * 8));

    return result;
}

static inline uint16_t readLEint16(const void *buffer, size_t nbytes)
{
    uint16_t result = 0;
    const uint8_t *data = reinterpret_cast<const uint8_t *>(buffer);

    for(size_t n = 0; n < nbytes; ++n)
        result += static_cast<uint16_t>(data[n] << (n * 8));

    return result;
}

static inline uint32_t readLEint32(const void *buffer, size_t nbytes)
{
    uint32_t result = 0;
    const uint8_t *data = reinterpret_cast<const uint8_t *>(buffer);

    for(size_t n = 0; n < nbytes; ++n)
        result += static_cast<uint32_t>(data[n] << (n * 8));

    return result;
}

static bool readUInt32LE(size_t &out, FileAndMemReader &fr)
{
    uint8_t buf[4];

    if(fr.read(buf, 1, 4) != 4)
        return false;

    out = readLEint32(buf, 4);

    return true;
}

static bool readUInt16LE(size_t &out, FileAndMemReader &fr)
{
    uint8_t buf[2];

    if(fr.read(buf, 1, 2) != 2)
        return false;

    out = readLEint16(buf, 2);

    return true;
}

static bool readUInt16LE(uint16_t &out, FileAndMemReader &fr)
{
    uint8_t buf[2];

    if(fr.read(buf, 1, 2) != 2)
        return false;

    out = readLEint16(buf, 2);

    return true;
}

/**
 * @brief Secure Standard MIDI Variable-Length numeric value parser with anti-out-of-range protection
 * @param [_inout] ptr Pointer to memory block that contains begin of variable-length value, will be iterated forward
 * @param [_in end Pointer to end of memory block where variable-length value is stored (after end of track)
 * @param [_out] ok Reference to boolean which takes result of variable-length value parsing
 * @return Unsigned integer that conains parsed variable-length value
 */
static inline uint64_t readVarLenEx(const uint8_t **ptr, const uint8_t *end, bool &ok)
{
    uint64_t result = 0;
    ok = false;

    for(;;)
    {
        if(*ptr >= end)
            return 2;
        unsigned char byte = *((*ptr)++);
        result = (result << 7) + (byte & 0x7F);
        if(!(byte & 0x80))
            break;
    }

    ok = true;
    return result;
}

static inline uint64_t readVarLenEx(FileAndMemReader &fr, const size_t end, bool &ok)
{
    uint64_t result = 0;
    uint8_t byte;

    ok = false;

    for(;;)
    {
        if(fr.tell() >= end || fr.read(&byte, 1, 1) != 1)
            return 2;

        result = (result << 7) + (byte & 0x7F);

        if((byte & 0x80) == 0)
            break;
    }

    ok = true;

    return result;
}

static inline uint64_t readHMPVarLenEx(FileAndMemReader &fr, const size_t end, bool &ok)
{
    uint64_t result = 0;
    uint8_t byte;
    int offset = 0;

    ok = false;

    for(;;)
    {
        if(fr.tell() >= end || fr.read(&byte, 1, 1) != 1)
            return 2;

        result |= (byte & 0x7F) << offset;
        offset += 7;

        if((byte & 0x80) != 0)
            break;
    }

    ok = true;

    return result;
}

BW_MidiSequencer::MidiTrackRow::MidiTrackRow() :
    time(0.0),
    delay(0),
    absPos(0),
    timeDelay(0.0),
    events_begin(0),
    events_end(0)
{}

void BW_MidiSequencer::MidiTrackRow::clear()
{
    time = 0.0;
    delay = 0;
    absPos = 0;
    timeDelay = 0.0;
    events_begin = 0;
    events_end = 0;
}

int BW_MidiSequencer::MidiTrackRow::typePriority(const BW_MidiSequencer::MidiEvent &evt)
{
    switch(evt.type)
    {
    case MidiEvent::T_SYSEX:
    case MidiEvent::T_SYSEX2:
        return 0;

    case MidiEvent::T_NOTEOFF:
        return 1;

    case MidiEvent::T_SPECIAL:
    case MidiEvent::T_SYSCOMSPOSPTR:
    case MidiEvent::T_SYSCOMSNGSEL:
        switch(evt.subtype)
        {
        case MidiEvent::ST_MARKER:
        case MidiEvent::ST_DEVICESWITCH:
        case MidiEvent::ST_SONG_BEGIN_HOOK:
        case MidiEvent::ST_LOOPSTART:
        case MidiEvent::ST_LOOPEND:
        case MidiEvent::ST_LOOPSTACK_BEGIN:
        case MidiEvent::ST_LOOPSTACK_END:
        case MidiEvent::ST_LOOPSTACK_BREAK:
            return 2;

        case MidiEvent::ST_ENDTRACK:
            return 20;

        default:
            return 10;
        }

    case MidiEvent::T_NOTETOUCH:
    case MidiEvent::T_CTRLCHANGE:
    case MidiEvent::T_PATCHCHANGE:
    case MidiEvent::T_CHANAFTTOUCH:
    case MidiEvent::T_WHEEL:
        return 3;

    case MidiEvent::T_NOTEON:
    case MidiEvent::T_NOTEON_DURATED:
        return 4;

    default:
        return 10;
    }
}

void BW_MidiSequencer::MidiTrackRow::sortEvents(std::vector<MidiEvent> &eventsBank, bool *noteStates)
{
    //if(events.size() > 0)
    size_t arr_size = 0;
    MidiEvent *arr = NULL, *arr_end = NULL;

    if(events_begin != events_end && events_begin < events_end)
    {
        MidiEvent piv;
        MidiEvent *i, *j;

        arr = eventsBank.data() + events_begin;
        arr_end = eventsBank.data() + events_end;
        arr_size = events_end - events_begin;

        for(i = arr + 1; i < arr_end; ++i)
        {
            memcpy(&piv, i, sizeof(MidiEvent));
            int piv_pri = typePriority(piv);

            for(j = i; j > arr && piv_pri < typePriority(*(j - 1)); --j)
                memcpy(j, j - 1, sizeof(MidiEvent));

            memcpy(j, &piv, sizeof(MidiEvent));
        }
    }

    /*
     * If Note-Off and it's Note-On is on the same row - move this damned note off down!
     */
    if(noteStates && arr && arr_size > 0)
    {
        size_t max_size = arr_size;

        if(arr_size > 1)
        {
            for(size_t i = arr_size - 1; ; --i)
            {
                const MidiEvent &e = arr[i];

                if(e.type == MidiEvent::T_NOTEOFF)
                    break;

                if(e.type != MidiEvent::T_NOTEON)
                {
                    if(i == 0)
                        break;
                    continue;
                }

                const size_t note_i = (static_cast<size_t>(e.channel) << 7) | (e.data_loc[0] & 0x7F);

                //Check, was previously note is on or off
                bool wasOn = noteStates[note_i];

                // Detect zero-length notes are following previously pressed note
                int noteOffsOnSameNote = 0;

                for(size_t j = 0; j < max_size; )
                {
                    const MidiEvent &o = arr[j];
                    if(o.type == MidiEvent::T_NOTEON)
                        break;

                    if(o.type != MidiEvent::T_NOTEOFF)
                    {
                        ++j;
                        continue;
                    }

                    const size_t note_j = (static_cast<size_t>(o.channel) << 7) | (o.data_loc[0] & 0x7F);

                    // If note was off, and note-off on same row with note-on - move it down!
                    if(note_j == note_i)
                    {
                        // If note is already off OR more than one note-off on same row and same note
                        if(!wasOn || (noteOffsOnSameNote != 0))
                        {
                            MidiEvent tmp;

                            if(j < arr_size - 1)
                            {
                                MidiEvent *dst = arr + j;
                                MidiEvent *src = dst + 1;

                                // Move this event to end of the list
                                memcpy(&tmp, &o, sizeof(MidiEvent));

                                for(size_t k = j + 1; k < arr_size; ++k)
                                    memcpy(dst++, src++, sizeof(MidiEvent));

                                memcpy(&arr[arr_size - 1], &tmp, sizeof(MidiEvent));

                                --max_size;

                                // Caused offset of i (j is always smaller than i!)
                                if(j < i)
                                    --i;

                            }
                            else
                                ++j;

                            continue;
                        }
                        else
                        {
                            // When same row has many note-offs on same row
                            // that means a zero-length note follows previous note
                            // it must be shuted down
                            ++noteOffsOnSameNote;
                        }
                    }

                    ++j;
                } // Sub-Loop through note-offs

                if(i == 0)
                    break;
            } // loop through note-ons
        } // arr_size > 1

        // Apply note states according to event types
        for(size_t i = 0; i < arr_size ; ++i)
        {
            const MidiEvent &e = arr[i];

            if(e.type == MidiEvent::T_NOTEON)
                noteStates[(static_cast<size_t>(e.channel) << 7) | (e.data_loc[0] & 0x7F)] = true;
            else if(e.type == MidiEvent::T_NOTEOFF)
                noteStates[(static_cast<size_t>(e.channel) << 7) | (e.data_loc[0] & 0x7F)] = false;
        }
    }
}

BW_MidiSequencer::BW_MidiSequencer() :
    m_interface(NULL),
    m_loadTrackNumber(0),
    m_triggerHandler(NULL),
    m_triggerUserData(NULL),
    m_format(Format_MIDI),
    m_smfFormat(0),
    m_loopFormat(Loop_Default),
    m_loopEnabled(false),
    m_loopHooksOnly(false),
    m_fullSongTimeLength(0.0),
    m_postSongWaitDelay(1.0),
    m_loopStartTime(-1.0),
    m_loopEndTime(-1.0),
    m_atEnd(false),
    m_loopCount(-1),
    m_trackSolo(~static_cast<size_t>(0)),
    m_tempoMultiplier(1.0)
{
    m_loop.reset();
    m_loop.invalidLoop = false;
    m_time.init();
}

BW_MidiSequencer::~BW_MidiSequencer()
{}

void BW_MidiSequencer::setInterface(const BW_MidiRtInterface *intrf)
{
    // Interface must NOT be NULL
    assert(intrf);

    // Note ON hook is REQUIRED
    assert(intrf->rt_noteOn);
    // Note OFF hook is REQUIRED
    assert(intrf->rt_noteOff || intrf->rt_noteOffVel);
    // Note Aftertouch hook is REQUIRED
    assert(intrf->rt_noteAfterTouch);
    // Channel Aftertouch hook is REQUIRED
    assert(intrf->rt_channelAfterTouch);
    // Controller change hook is REQUIRED
    assert(intrf->rt_controllerChange);
    // Patch change hook is REQUIRED
    assert(intrf->rt_patchChange);
    // Pitch bend hook is REQUIRED
    assert(intrf->rt_pitchBend);
    // System Exclusive hook is REQUIRED
    assert(intrf->rt_systemExclusive);

    if(intrf->pcmSampleRate != 0 && intrf->pcmFrameSize != 0)
    {
        m_time.sampleRate = intrf->pcmSampleRate;
        m_time.frameSize = intrf->pcmFrameSize;
        m_time.reset();
    }

    m_interface = intrf;
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

BW_MidiSequencer::FileFormat BW_MidiSequencer::getFormat()
{
    return m_format;
}

size_t BW_MidiSequencer::getTrackCount() const
{
    return m_trackData.size();
}

bool BW_MidiSequencer::setTrackEnabled(size_t track, bool enable)
{
    size_t trackCount = m_trackData.size();
    if(track >= trackCount)
        return false;
    m_trackDisable[track] = !enable;
    return true;
}

bool BW_MidiSequencer::setChannelEnabled(size_t channel, bool enable)
{
    if(channel >= 16)
        return false;

    if(!enable && m_channelDisable[channel] != !enable)
    {
        uint8_t ch = static_cast<uint8_t>(channel);

        // Releae all pedals
        m_interface->rt_controllerChange(m_interface->rtUserData, ch, 64, 0);
        m_interface->rt_controllerChange(m_interface->rtUserData, ch, 66, 0);

        // Release all notes on the channel now
        for(int i = 0; i < 127; ++i)
        {
            if(m_interface->rt_noteOff)
                m_interface->rt_noteOff(m_interface->rtUserData, ch, i);
            if(m_interface->rt_noteOffVel)
                m_interface->rt_noteOffVel(m_interface->rtUserData, ch, i, 0);
        }
    }

    m_channelDisable[channel] = !enable;
    return true;
}

void BW_MidiSequencer::setSoloTrack(size_t track)
{
    m_trackSolo = track;
}

void BW_MidiSequencer::setSongNum(int track)
{
    m_loadTrackNumber = track;

    if(!m_rawSongsData.empty() && m_format == Format_XMIDI) // Reload the song
    {
        if(m_loadTrackNumber >= (int)m_rawSongsData.size())
            m_loadTrackNumber = m_rawSongsData.size() - 1;

        if(m_interface && m_interface->rt_controllerChange)
        {
            for(int i = 0; i < 15; i++)
                m_interface->rt_controllerChange(m_interface->rtUserData, i, 123, 0);
        }

        m_atEnd            = false;
        m_loop.fullReset();
        m_loop.caughtStart = true;

        m_smfFormat = 0;

        FileAndMemReader fr;
        fr.openData(m_rawSongsData[m_loadTrackNumber].data(),
                    m_rawSongsData[m_loadTrackNumber].size());
        parseSMF(fr);

        m_format = Format_XMIDI;
    }
}

int BW_MidiSequencer::getSongsCount()
{
    return (int)m_rawSongsData.size();
}


void BW_MidiSequencer::setTriggerHandler(TriggerHandler handler, void *userData)
{
    m_triggerHandler = handler;
    m_triggerUserData = userData;
}

const std::vector<BW_MidiSequencer::CmfInstrument> BW_MidiSequencer::getRawCmfInstruments()
{
    return m_cmfInstruments;
}

const char *BW_MidiSequencer::getErrorString() const
{
    return m_errorString.c_str();
}

bool BW_MidiSequencer::getLoopEnabled()
{
    return m_loopEnabled;
}

void BW_MidiSequencer::setLoopEnabled(bool enabled)
{
    m_loopEnabled = enabled;
}

int BW_MidiSequencer::getLoopsCount()
{
    return m_loopCount >= 0 ? (m_loopCount + 1) : m_loopCount;
}

void BW_MidiSequencer::setLoopsCount(int loops)
{
    if(loops >= 1)
        loops -= 1; // Internally, loops count has the 0 base
    m_loopCount = loops;
}

void BW_MidiSequencer::setLoopHooksOnly(bool enabled)
{
    m_loopHooksOnly = enabled;
}

const char *BW_MidiSequencer::getMusicTitle() const
{
    if(m_musTitle.size == 0)
        return "";
    else
        return reinterpret_cast<const char*>(getData(m_musTitle));
}

const char *BW_MidiSequencer::getMusicCopyright() const
{
    if(m_musCopyright.size == 0)
        return "";
    else
        return reinterpret_cast<const char*>(getData(m_musCopyright));
}

const std::vector<BW_MidiSequencer::DataBlock> &BW_MidiSequencer::getTrackTitles()
{
    return m_musTrackTitles;
}

const std::vector<BW_MidiSequencer::MIDI_MarkerEntry> &BW_MidiSequencer::getMarkers()
{
    return m_musMarkers;
}

bool BW_MidiSequencer::positionAtEnd()
{
    return m_atEnd;
}

double BW_MidiSequencer::getTempoMultiplier()
{
    return m_tempoMultiplier;
}

bool BW_MidiSequencer::duratedNoteInsert(size_t track, BW_MidiSequencer::DuratedNote *note)
{
    DuratedNotesCache &cache = m_trackDuratedNotes[track];

    if(cache.notes_count >= 128)
        return false; // Can't insert delayed note off!

    std::memcpy(cache.notes + cache.notes_count, note, sizeof(DuratedNote));
    ++cache.notes_count;

    return true;
}

void BW_MidiSequencer::duratedNoteClear()
{
    for(std::vector<DuratedNotesCache>::iterator it = m_trackDuratedNotes.begin(); it != m_trackDuratedNotes.end(); ++it)
        it->notes_count = 0;
}

void BW_MidiSequencer::duratedNoteTick(size_t track, int64_t ticks)
{
    DuratedNotesCache &cache = m_trackDuratedNotes[track];

    for(size_t i = 0; i < cache.notes_count; ++i)
        cache.notes[i].ttl -= ticks;
}

void BW_MidiSequencer::duratedNotePop(size_t track, size_t i)
{
    DuratedNotesCache &cache = m_trackDuratedNotes[track];

    if(i < cache.notes_count)
    {
        if(cache.notes_count > 1)
            std::memcpy(cache.notes + i, cache.notes + cache.notes_count - 1, sizeof(DuratedNote));
        --cache.notes_count;
    }
}

void BW_MidiSequencer::buildSmfSetupReset(size_t trackCount)
{
    m_fullSongTimeLength = 0.0;
    m_loopStartTime = -1.0;
    m_loopEndTime = -1.0;
    m_loopFormat = Loop_Default;
    m_trackDisable.clear();
    std::memset(m_channelDisable, 0, sizeof(m_channelDisable));
    m_trackSolo = ~(size_t)0;
    m_musTitle.size = 0;
    m_musTitle.offset = 0;
    m_musCopyright.size = 0;
    m_musCopyright.offset = 0;
    m_musTrackTitles.clear();
    m_musMarkers.clear();
    m_dataBank.clear();
    m_eventBank.clear();
    m_trackData.clear();
    m_trackData.resize(trackCount, MidiTrackQueue());
    m_trackDuratedNotes.clear();
    m_trackDuratedNotes.resize(trackCount);
    std::memset(m_trackDuratedNotes.data(), 0, sizeof(DuratedNotesCache) * trackCount);
    m_trackDisable.resize(trackCount);

    m_loop.reset();
    m_loop.invalidLoop = false;
    m_time.reset();

    m_currentPosition.began = false;
    m_currentPosition.absTimePosition = 0.0;
    m_currentPosition.wait = 0.0;
    m_currentPosition.track.clear();
    m_currentPosition.track.resize(trackCount);
}

bool BW_MidiSequencer::buildSmfTrackData(FileAndMemReader &fr, const size_t tracks_offset, const size_t tracks_count)
{
    // bool gotGlobalLoopStart = false,
    //      gotGlobalLoopEnd = false,
    //      gotStackLoopStart = false,
    //      gotLoopEventInThisRow = false;

    uint8_t headBuf[8];
    size_t fsize = 0;
    size_t trackLength;
    //! Tempo change events list
    std::vector<TempoEvent> temposList;
    LoopPointParseState loopState;

    std::memset(&loopState, 0, sizeof(loopState));

    buildSmfSetupReset(tracks_count);

    // Read tracks from here
    fr.seek(tracks_offset, FileAndMemReader::SET);

    /*
     * TODO: Make this be safer for memory in case of broken input data
     * which may cause going away of available track data (and then give a crash!)
     *
     * POST: Check this more carefully for possible vulnuabilities are can crash this
     */
    for(size_t tk = 0; tk < tracks_count; ++tk)
    {
        fsize = fr.read(headBuf, 1, 8);
        if((fsize < 8) || (std::memcmp(headBuf, "MTrk", 4) != 0))
        {
            m_errorString.set(fr.fileName().c_str());
            m_errorString.append(": Invalid format, MTrk signature is not found!\n");
            return false;
        }

        trackLength = (size_t)readBEint(headBuf + 4, 4);

        if(!buildSmfTrack(fr, tk, trackLength, temposList, loopState))
            return false; // Failed to parse track (error already written!)
    }

    installLoop(loopState);
    buildTimeLine(temposList, loopState.loopStartTicks, loopState.loopEndTicks);

    return true;
}

bool BW_MidiSequencer::buildSmfTrack(FileAndMemReader &fr,
                                     const size_t track_idx,
                                     const size_t track_size,
                                     std::vector<TempoEvent> &temposList,
                                     LoopPointParseState &loopState)
{
    MidiTrackRow evtPos;
    MidiEvent event;
    uint64_t abs_position = 0;
    int status = 0;
    const size_t end = fr.tell() + track_size;
    bool ok = false;
    char error[150];

    //! Caches note on/off states.
    bool noteStates[0x7FF]; // [ccc|cnnnnnnn] - c = channel, n = note
    /* This is required to carefully detect zero-length notes           *
     * and avoid a move of "note-off" event over "note-on" while sort.  *
     * Otherwise, after sort those notes will play infinite sound       */

    std::memset(noteStates, 0, sizeof(noteStates));

    // Time delay that follows the first event in the track
    if(m_format == Format_RSXX)
        ok = true;
    else
        evtPos.delay = readVarLenEx(fr, end, ok);

    if(!ok)
    {
        int len = snprintf(error, 150, "buildTrackData: Can't read variable-length value at begin of track %lu.\n", (unsigned long)track_idx);
        if((len > 0) && (len < 150))
            m_parsingErrorsString.append(error, (size_t)len);
        return false;
    }

    // HACK: Begin every track with "Reset all controllers" event to avoid controllers state break came from end of song
    if(track_idx == 0)
    {
        MidiEvent resetEvent;
        std::memset(&resetEvent, 0, sizeof(resetEvent));
        resetEvent.isValid = 1;
        resetEvent.type = MidiEvent::T_SPECIAL;
        resetEvent.subtype = MidiEvent::ST_SONG_BEGIN_HOOK;
        addEventToBank(evtPos, resetEvent);
    }

    evtPos.absPos = abs_position;
    abs_position += evtPos.delay;
    m_trackData[track_idx].push_back(evtPos);
    evtPos.clear();

    do
    {
        event = parseEvent(fr, end, status);
        if(!event.isValid)
        {
            int len = snprintf(error, 150, "buildTrackData: Fail to parse event in the track %lu.\n", (unsigned long)track_idx);
            if((len > 0) && (len < 150))
                m_parsingErrorsString.append(error, (size_t)len);
            return false;
        }

        addEventToBank(evtPos, event);

        if(event.type == MidiEvent::T_SPECIAL)
        {
            if(event.subtype == MidiEvent::ST_TEMPOCHANGE)
            {
                TempoEvent t = {readBEint(event.data_loc, event.data_loc_size), abs_position};
                temposList.push_back(t);
            }
            else
                analyseLoopEvent(loopState, event, abs_position);
        }

        if(event.subtype != MidiEvent::ST_ENDTRACK) // Don't try to read delta after EndOfTrack event!
        {
            evtPos.delay = readVarLenEx(fr, end, ok);
            if(!ok)
            {
                /* End of track has been reached! However, there is no EOT event presented */
                event.type = MidiEvent::T_SPECIAL;
                event.subtype = MidiEvent::ST_ENDTRACK;
            }
        }

#ifdef ENABLE_END_SILENCE_SKIPPING
        //Have track end on its own row? Clear any delay on the row before
        if(event.subtype == MidiEvent::ST_ENDTRACK && (evtPos.events_end - evtPos.events_begin) == 1)
        {
            if (!m_trackData[track_idx].empty())
            {
                MidiTrackRow &previous = m_trackData[track_idx].back();
                previous.delay = 0;
                previous.timeDelay = 0;
            }
        }
#endif

        if((evtPos.delay > 0) || (event.subtype == MidiEvent::ST_ENDTRACK))
        {
            evtPos.absPos = abs_position;
            abs_position += evtPos.delay;
            evtPos.sortEvents(m_eventBank, noteStates);
            m_trackData[track_idx].push_back(evtPos);
            evtPos.clear();
            loopState.gotLoopEventsInThisRow = false;
        }
    }
    while((fr.tell() <= end) && (event.subtype != MidiEvent::ST_ENDTRACK));

    if(loopState.ticksSongLength < abs_position)
        loopState.ticksSongLength = abs_position;

    // Set the chain of events begin
    if(m_trackData[track_idx].size() > 0)
        m_currentPosition.track[track_idx].pos = m_trackData[track_idx].begin();

    return true;
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
    if(!m_loop.invalidLoop && (event.subtype == MidiEvent::ST_LOOPSTART))
    {
        /*
         * loopStart is invalid when:
         * - starts together with loopEnd
         * - appears more than one time in same MIDI file
         */
        if(loopState.gotLoopStart || loopState.gotLoopEventsInThisRow)
            m_loop.invalidLoop = true;
        else
        {
            loopState.gotLoopStart = true;
            loopState.loopStartTicks = abs_position;
        }
        // In this row we got loop event, register this!
        loopState.gotLoopEventsInThisRow = true;
    }
    else if(!m_loop.invalidLoop && (event.subtype == MidiEvent::ST_LOOPEND))
    {
        /*
         * loopEnd is invalid when:
         * - starts before loopStart
         * - starts together with loopStart
         * - appars more than one time in same MIDI file
         */
        if(loopState.gotLoopEnd || loopState.gotLoopEventsInThisRow)
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
        loopState.gotLoopEventsInThisRow = true;
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
        if(m_loop.stackLevel >= static_cast<int>(m_loop.stack.size()))
        {
            LoopStackEntry e;
            e.loops = event.data_loc[0];
            e.infinity = (event.data_loc[0] == 0);
            e.start = abs_position;
            e.end = abs_position;
            m_loop.stack.push_back(e);
        }
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
    }
}

void BW_MidiSequencer::buildTimeLine(const std::vector<TempoEvent> &tempos,
                                          uint64_t loopStartTicks,
                                          uint64_t loopEndTicks)
{
    const size_t    trackCount = m_trackData.size();
    /********************************************************************************/
    // Calculate time basing on collected tempo events
    /********************************************************************************/
    for(size_t tk = 0; tk < trackCount; ++tk)
    {
        fraction<uint64_t> currentTempo = m_tempo;
        double  time = 0.0;
        // uint64_t abs_position = 0;
        size_t tempo_change_index = 0;
        MidiTrackQueue &track = m_trackData[tk];
        if(track.empty())
            continue;//Empty track is useless!

#ifdef DEBUG_TIME_CALCULATION
        std::fprintf(stdout, "\n============Track %" PRIuPTR "=============\n", tk);
        std::fflush(stdout);
#endif

        MidiTrackRow fakePos, *posPrev = &(*(track.begin()));//First element
        if(posPrev->absPos > 0) // If doesn't begins with zero!
        {
            fakePos.absPos = 0;
            fakePos.delay = posPrev->absPos;
            posPrev = &fakePos;
        }

        for(MidiTrackQueue::iterator it = track.begin(); it != track.end(); it++)
        {
#ifdef DEBUG_TIME_CALCULATION
            bool tempoChanged = false;
#endif
            MidiTrackRow &pos = *it;
            if((posPrev != &pos) && // Skip first event
               (!tempos.empty()) && // Only when in-track tempo events are available
               (tempo_change_index < tempos.size())
              )
            {
                // If tempo event is going between of current and previous event
                if(tempos[tempo_change_index].absPosition <= pos.absPos)
                {
                    // Stop points: begin point and tempo change points are before end point
                    std::vector<TempoChangePoint> points;
                    fraction<uint64_t> t;
                    TempoChangePoint firstPoint = {posPrev->absPos, currentTempo};
                    points.push_back(firstPoint);

                    // Collect tempo change points between previous and current events
                    do
                    {
                        TempoChangePoint tempoMarker;
                        const TempoEvent &tempoPoint = tempos[tempo_change_index];
                        tempoMarker.absPos = tempoPoint.absPosition;
                        tempoMarker.tempo = m_invDeltaTicks * fraction<uint64_t>(tempoPoint.tempo);
                        points.push_back(tempoMarker);
                        tempo_change_index++;
                    }
                    while((tempo_change_index < tempos.size()) &&
                          (tempos[tempo_change_index].absPosition <= pos.absPos));

                    // Re-calculate time delay of previous event
                    time -= posPrev->timeDelay;
                    posPrev->timeDelay = 0.0;

                    for(size_t i = 0, j = 1; j < points.size(); i++, j++)
                    {
                        /* If one or more tempo events are appears between of two events,
                         * calculate delays between each tempo point, begin and end */
                        uint64_t midDelay = 0;
                        // Delay between points
                        midDelay  = points[j].absPos - points[i].absPos;
                        // Time delay between points
                        t = midDelay * currentTempo;
                        posPrev->timeDelay += t.value();

                        // Apply next tempo
                        currentTempo = points[j].tempo;
#ifdef DEBUG_TIME_CALCULATION
                        tempoChanged = true;
#endif
                    }
                    // Then calculate time between last tempo change point and end point
                    TempoChangePoint tailTempo = points.back();
                    uint64_t postDelay = pos.absPos - tailTempo.absPos;
                    t = postDelay * currentTempo;
                    posPrev->timeDelay += t.value();

                    // Store Common time delay
                    posPrev->time = time;
                    time += posPrev->timeDelay;
                }
            }

            fraction<uint64_t> t = pos.delay * currentTempo;
            pos.timeDelay = t.value();
            pos.time = time;
            time += pos.timeDelay;

            // Capture markers after time value calculation
            for(size_t i = pos.events_begin; i < pos.events_end; ++i)
            {
                MidiEvent &e = m_eventBank[i];
                if((e.type == MidiEvent::T_SPECIAL) && (e.subtype == MidiEvent::ST_MARKER))
                {
                    MIDI_MarkerEntry marker;
                    marker.label = e.data_block;
                    marker.pos_ticks = pos.absPos;
                    marker.pos_time = pos.time;
                    m_musMarkers.push_back(marker);
                }
            }

            // Capture loop points time positions
            if(!m_loop.invalidLoop)
            {
                // Set loop points times
                if(loopStartTicks == pos.absPos)
                    m_loopStartTime = pos.time;
                else if(loopEndTicks == pos.absPos)
                    m_loopEndTime = pos.time;
            }

#ifdef DEBUG_TIME_CALCULATION
            std::fprintf(stdout, "= %10" PRId64 " = %10f%s\n", pos.absPos, pos.time, tempoChanged ? " <----TEMPO CHANGED" : "");
            std::fflush(stdout);
#endif

            // abs_position += pos.delay;
            posPrev = &pos;
        }

        if(time > m_fullSongTimeLength)
            m_fullSongTimeLength = time;
    }

    m_fullSongTimeLength += m_postSongWaitDelay;
    // Set begin of the music
    m_trackBeginPosition = m_currentPosition;
    // Initial loop position will begin at begin of track until passing of the loop point
    m_loopBeginPosition  = m_currentPosition;
    // Set lowest level of the loop stack
    m_loop.stackLevel = -1;

    // Set the count of loops
    m_loop.loopsCount = m_loopCount;
    m_loop.loopsLeft = m_loopCount;

    /********************************************************************************/
    // Find and set proper loop points
    /********************************************************************************/
    if(!m_loop.invalidLoop && !m_currentPosition.track.empty())
    {
        unsigned caughLoopStart = 0;
        bool scanDone = false;
        const size_t  trackCount = m_currentPosition.track.size();
        Position rowPosition(m_currentPosition);

        while(!scanDone)
        {
            const Position      rowBeginPosition(rowPosition);

            for(size_t tk = 0; tk < trackCount; ++tk)
            {
                Position::TrackInfo &track = rowPosition.track[tk];
                if((track.lastHandledEvent >= 0) && (track.delay <= 0))
                {
                    // Check is an end of track has been reached
                    if(track.pos == m_trackData[tk].end())
                    {
                        track.lastHandledEvent = -1;
                        continue;
                    }

                    for(size_t i = track.pos->events_begin; i < track.pos->events_end; ++i)
                    {
                        const MidiEvent &evt = m_eventBank[i];
                        if(evt.type == MidiEvent::T_SPECIAL && evt.subtype == MidiEvent::ST_LOOPSTART)
                        {
                            caughLoopStart++;
                            scanDone = true;
                            break;
                        }
                    }

                    if(track.lastHandledEvent >= 0)
                    {
                        track.delay += track.pos->delay;
                        track.pos++;
                    }
                }
            }

            // Find a shortest delay from all track
            uint64_t shortestDelay = 0;
            bool     shortestDelayNotFound = true;

            for(size_t tk = 0; tk < trackCount; ++tk)
            {
                Position::TrackInfo &track = rowPosition.track[tk];
                if((track.lastHandledEvent >= 0) && (shortestDelayNotFound || track.delay < shortestDelay))
                {
                    shortestDelay = track.delay;
                    shortestDelayNotFound = false;
                }
            }

            // Schedule the next playevent to be processed after that delay
            for(size_t tk = 0; tk < trackCount; ++tk)
                rowPosition.track[tk].delay -= shortestDelay;

            if(caughLoopStart > 0)
            {
                m_loopBeginPosition = rowBeginPosition;
                m_loopBeginPosition.absTimePosition = m_loopStartTime;
                scanDone = true;
            }

            if(shortestDelayNotFound)
                break;
        }
    }

    /********************************************************************************/
    // Resolve "hell of all times" of too short drum notes:
    // move too short percussion note-offs far far away as possible
    /********************************************************************************/
// OLD DEAD CODE NO LONGER NEEDED
#if 0 // Use this to record WAVEs for comparison before/after implementing of this
    if(m_format == Format_MIDI) // Percussion fix is needed for MIDI only, not for IMF/RSXX or CMF
    {
        //! Minimal real time in seconds
#define DRUM_NOTE_MIN_TIME  0.03
        //! Minimal ticks count
#define DRUM_NOTE_MIN_TICKS 15
        struct NoteState
        {
            double       delay;
            uint64_t     delayTicks;
            bool         isOn;
            char         ___pad[7];
        } drNotes[255];
        size_t banks[16];

        for(size_t tk = 0; tk < trackCount; ++tk)
        {
            std::memset(drNotes, 0, sizeof(drNotes));
            std::memset(banks, 0, sizeof(banks));
            MidiTrackQueue &track = m_trackData[tk];
            if(track.empty())
                continue; // Empty track is useless!

            for(MidiTrackQueue::iterator it = track.begin(); it != track.end(); it++)
            {
                MidiTrackRow &pos = *it;

                for(ssize_t e = 0; e < (ssize_t)pos.events.size(); e++)
                {
                    MidiEvent *et = &pos.events[(size_t)e];

                    /* Set MSB/LSB bank */
                    if(et->type == MidiEvent::T_CTRLCHANGE)
                    {
                        uint8_t ctrlno = et->data[0];
                        uint8_t value =  et->data[1];
                        switch(ctrlno)
                        {
                        case 0: // Set bank msb (GM bank)
                            banks[et->channel] = (value << 8) | (banks[et->channel] & 0x00FF);
                            break;
                        case 32: // Set bank lsb (XG bank)
                            banks[et->channel] = (banks[et->channel] & 0xFF00) | (value & 0x00FF);
                            break;
                        default:
                            break;
                        }
                        continue;
                    }

                    bool percussion = (et->channel == 9) ||
                                      banks[et->channel] == 0x7E00 || // XG SFX1/SFX2 channel (16128 signed decimal)
                                      banks[et->channel] == 0x7F00;   // XG Percussion channel (16256 signed decimal)
                    if(!percussion)
                        continue;

                    if(et->type == MidiEvent::T_NOTEON)
                    {
                        uint8_t     note = et->data[0] & 0x7F;
                        NoteState   &ns = drNotes[note];
                        ns.isOn = true;
                        ns.delay = 0.0;
                        ns.delayTicks = 0;
                    }
                    else if(et->type == MidiEvent::T_NOTEOFF)
                    {
                        uint8_t note = et->data[0] & 0x7F;
                        NoteState &ns = drNotes[note];
                        if(ns.isOn)
                        {
                            ns.isOn = false;
                            if(ns.delayTicks < DRUM_NOTE_MIN_TICKS || ns.delay < DRUM_NOTE_MIN_TIME) // If note is too short
                            {
                                //Move it into next event position if that possible
                                for(MidiTrackQueue::iterator itNext = it;
                                    itNext != track.end();
                                    itNext++)
                                {
                                    MidiTrackRow &posN = *itNext;
                                    if(ns.delayTicks > DRUM_NOTE_MIN_TICKS && ns.delay > DRUM_NOTE_MIN_TIME)
                                    {
                                        // Put note-off into begin of next event list
                                        posN.events.insert(posN.events.begin(), pos.events[(size_t)e]);
                                        // Renive this event from a current row
                                        pos.events.erase(pos.events.begin() + (int)e);
                                        e--;
                                        break;
                                    }
                                    ns.delay += posN.timeDelay;
                                    ns.delayTicks += posN.delay;
                                }
                            }
                            ns.delay = 0.0;
                            ns.delayTicks = 0;
                        }
                    }
                }

                //Append time delays to sustaining notes
                for(size_t no = 0; no < 128; no++)
                {
                    NoteState &ns = drNotes[no];
                    if(ns.isOn)
                    {
                        ns.delay        += pos.timeDelay;
                        ns.delayTicks   += pos.delay;
                    }
                }
            }
        }
#undef DRUM_NOTE_MIN_TIME
#undef DRUM_NOTE_MIN_TICKS
    }
#endif

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
    bool     doLoopJump = false;
    unsigned caughLoopStart = 0;
    unsigned caughLoopStackStart = 0;
    unsigned caughLoopStackEnds = 0;
    double   caughLoopStackEndsTime = 0.0;
    unsigned caughLoopStackBreaks = 0;

#ifdef DEBUG_TIME_CALCULATION
    double maxTime = 0.0;
#endif

    for(size_t tk = 0; tk < trackCount; ++tk)
    {
        Position::TrackInfo &track = m_currentPosition.track[tk];

        // Process note-OFFs
        processDuratedNotes(tk, track.lastHandledEvent);

        if((track.lastHandledEvent >= 0) && (track.delay <= 0))
        {
            // Check is an end of track has been reached
            if(track.pos == m_trackData[tk].end())
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

                if(m_loop.caughtStart)
                {
                    if(m_interface->onloopStart) // Loop Start hook
                        m_interface->onloopStart(m_interface->onloopStart_userData);

                    caughLoopStart++;
                    m_loop.caughtStart = false;
                }

                if(m_loop.caughtStackStart)
                {
                    if(m_interface->onloopStart && (m_loopStartTime >= track.pos->time)) // Loop Start hook
                        m_interface->onloopStart(m_interface->onloopStart_userData);

                    caughLoopStackStart++;
                    m_loop.caughtStackStart = false;
                }

                if(m_loop.caughtStackBreak)
                {
                    caughLoopStackBreaks++;
                    m_loop.caughtStackBreak = false;
                }

                if(m_loop.caughtEnd || m_loop.isStackEnd())
                {
                    if(m_loop.caughtStackEnd)
                    {
                        m_loop.caughtStackEnd = false;
                        caughLoopStackEnds++;
                        caughLoopStackEndsTime = track.pos->time;
                    }
                    doLoopJump = true;
                    break; // Stop event handling on catching loopEnd event!
                }
            }

#ifdef DEBUG_TIME_CALCULATION
            if(maxTime < track.pos->time)
                maxTime = track.pos->time;
#endif
            // Read next event time (unless the track just ended)
            if(track.lastHandledEvent >= 0)
            {
                track.delay += track.pos->delay;
                track.pos++;
            }

            if(doLoopJump)
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
        DuratedNotesCache &timedNotes = m_trackDuratedNotes[tk];

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

    fraction<uint64_t> t = shortestDelay * m_tempo;

#ifdef ENABLE_BEGIN_SILENCE_SKIPPING
    if(m_currentPosition.began)
#endif
        m_currentPosition.wait += t.value();

    if(caughLoopStart > 0 && m_loopBeginPosition.absTimePosition <= 0.0)
        m_loopBeginPosition = rowBeginPosition;

    if(caughLoopStackStart > 0)
    {
        while(caughLoopStackStart > 0)
        {
            m_loop.stackUp();
            LoopStackEntry &s = m_loop.getCurStack();
            s.startPosition = rowBeginPosition;
            caughLoopStackStart--;
        }
        return true;
    }

    if(caughLoopStackBreaks > 0)
    {
        while(caughLoopStackBreaks > 0)
        {
            LoopStackEntry &s = m_loop.getCurStack();
            s.loops = 0;
            s.infinity = false;
            // Quit the loop
            m_loop.stackDown();
            caughLoopStackBreaks--;
        }
    }

    if(caughLoopStackEnds > 0)
    {
        while(caughLoopStackEnds > 0)
        {
            LoopStackEntry &s = m_loop.getCurStack();
            if(s.infinity)
            {
                if(m_interface->onloopEnd && (m_loopEndTime >= caughLoopStackEndsTime)) // Loop End hook
                {
                    m_interface->onloopEnd(m_interface->onloopEnd_userData);
                    if(m_loopHooksOnly) // Stop song on reaching loop end
                    {
                        m_atEnd = true; // Don't handle events anymore
                        m_currentPosition.wait += m_postSongWaitDelay; // One second delay until stop playing
                    }
                }

                m_currentPosition = s.startPosition;
                m_loop.skipStackStart = true;

                for(uint8_t i = 0; i < 16; i++)
                    m_interface->rt_controllerChange(m_interface->rtUserData, i, 123, 0);

                return true;
            }
            else
            if(s.loops >= 0)
            {
                s.loops--;
                if(s.loops > 0)
                {
                    m_currentPosition = s.startPosition;
                    m_loop.skipStackStart = true;

                    for(uint8_t i = 0; i < 16; i++)
                        m_interface->rt_controllerChange(m_interface->rtUserData, i, 123, 0);

                    return true;
                }
                else
                {
                    // Quit the loop
                    m_loop.stackDown();
                }
            }
            else
            {
                // Quit the loop
                m_loop.stackDown();
            }
            caughLoopStackEnds--;
        }

        return true;
    }

    if(shortestDelayNotFound || m_loop.caughtEnd)
    {
        if(m_interface->onloopEnd) // Loop End hook
            m_interface->onloopEnd(m_interface->onloopEnd_userData);

        for(uint8_t i = 0; i < 16; i++)
            m_interface->rt_controllerChange(m_interface->rtUserData, i, 123, 0);

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
            m_currentPosition = m_trackBeginPosition;
            m_loop.temporaryBroken = false;
        }
        else if(m_loop.loopsCount < 0 || m_loop.loopsLeft >= 1)
        {
            m_currentPosition = m_loopBeginPosition;
            if(m_loop.loopsCount >= 1)
                m_loop.loopsLeft--;
        }
    }

    return true; // Has events in queue
}

void BW_MidiSequencer::processDuratedNotes(size_t track, int32_t &status)
{
    DuratedNotesCache &cache = m_trackDuratedNotes[track];

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

static bool strEqual(const uint8_t *in_str, size_t length, const char *needle)
{
    const char *it_i = reinterpret_cast<const char *>(in_str), *it_n = needle;
    size_t i = 0;

    for( ; i < length && *it_n != 0; ++i, ++it_i, ++it_n)
    {
        if(*it_i <= 'Z' && *it_i >= 'A')
        {
            if(*it_i - ('Z' - 'z') != *it_n)
                return false; // Mismatch!
        }
        else if(*it_i != *it_n)
            return false; // Mismatch!
    }

    return i == length && *it_n == 0; // Length is same
}

BW_MidiSequencer::MidiEvent BW_MidiSequencer::parseEvent(FileAndMemReader &fr, const size_t end, int &status)
{
    // const uint8_t *&ptr = *pptr;
    BW_MidiSequencer::MidiEvent evt;

    std::memset(&evt, 0, sizeof(evt));
    evt.isValid = 1;

    if(fr.tell() + 1 > end)
    {
        // When track doesn't ends on the middle of event data, it's must be fine
        evt.type = MidiEvent::T_SPECIAL;
        evt.subtype = MidiEvent::ST_ENDTRACK;
        return evt;
    }

    uint8_t byte;
    bool ok = false;
    std::vector<uint8_t> m_text_buffer;

    if(fr.read(&byte, 1, 1) != 1)
    {
        m_parsingErrorsString.append("parseEvent: Failed to read first byte of the event\n");
        evt.isValid = 0;
        return evt;
    }

    if(byte == MidiEvent::T_SYSEX || byte == MidiEvent::T_SYSEX2) // Parse SysEx
    {
        uint64_t length = readVarLenEx(fr, end, ok);
        if(!ok || (fr.tell() + length > end))
        {
            m_parsingErrorsString.append("parseEvent: Can't read SysEx event - Unexpected end of track data.\n");
            evt.isValid = 0;
            return evt;
        }

        evt.type = MidiEvent::T_SYSEX;
        insertDataToBankWithByte(evt, m_dataBank, byte, fr, length);
        return evt;
    }

    if(byte == MidiEvent::T_SPECIAL)
    {
        // Special event FF
        uint8_t  evtype;
        uint64_t length;

        if(fr.read(&evtype, 1, 1) != 1)
        {
            m_parsingErrorsString.append("parseEvent: Failed to read event type!\n");
            evt.isValid = 0;
            return evt;
        }

        length = readVarLenEx(fr, end, ok);

        if(!ok || (fr.tell() + length > end))
        {
            m_parsingErrorsString.append("parseEvent: Can't read Special event - Unexpected end of track data.\n");
            evt.isValid = 0;
            return evt;
        }

        // const uint8_t *data = ptr;
        // ptr += (size_t)length;

        evt.type = byte;
        evt.subtype = evtype;

        if(evt.subtype == MidiEvent::ST_SEQNUMBER ||
           evt.subtype == MidiEvent::ST_MIDICHPREFIX ||
           evt.subtype == MidiEvent::ST_TEMPOCHANGE ||
           evt.subtype == MidiEvent::ST_SMPTEOFFSET ||
           evt.subtype == MidiEvent::ST_TIMESIGNATURE ||
           evt.subtype == MidiEvent::ST_KEYSIGNATURE
        )
        {
            if(length > 5)
            {
                m_parsingErrorsString.append("parseEvent: Can't read one of special events - Too long event data (more than 5!).\n");
                evt.isValid = 0;
                return evt;
            }

            evt.data_loc_size = length;
            if(fr.read(evt.data_loc, 1, length) != length)
            {
                m_parsingErrorsString.append("parseEvent: Failed to read event's data (1).\n");
                evt.isValid = 0;
                return evt;
            }

#if 0 /* Print all tempo events */
            if(evt.subtype == MidiEvent::ST_TEMPOCHANGE && hooks.onDebugMessage)
                hooks.onDebugMessage(hooks.onDebugMessage_userData, "Temp Change: %02X%02X%02X", evt.data_loc[0], evt.data_loc[1], evt.data_loc[2]);
#endif
        }
        /* TODO: Store those meta-strings separately and give ability to read them
         * by external functions (to display song title and copyright in the player) */
        else if(evt.subtype == MidiEvent::ST_COPYRIGHT)
        {
            insertDataToBankWithTerm(evt, m_dataBank, fr, length);
            const char *entry = reinterpret_cast<const char*>(getData(evt.data_block));

            if(m_musCopyright.size == 0)
            {
                m_musCopyright = evt.data_block;

                if(m_interface->onDebugMessage)
                    m_interface->onDebugMessage(m_interface->onDebugMessage_userData, "Music copyright: %s", entry);
            }
            else if(m_interface->onDebugMessage)
                m_interface->onDebugMessage(m_interface->onDebugMessage_userData, "Extra copyright event: %s", entry);
        }
        else if(evt.subtype == MidiEvent::ST_SQTRKTITLE)
        {
            insertDataToBankWithTerm(evt, m_dataBank, fr, length);
            const char *entry = reinterpret_cast<const char*>(getData(evt.data_block));

            if(m_musTitle.size == 0)
            {
                m_musTitle = evt.data_block;
                if(m_interface->onDebugMessage)
                    m_interface->onDebugMessage(m_interface->onDebugMessage_userData, "Music title: %s", entry);
            }
            else
            {
                m_musTrackTitles.push_back(evt.data_block);

                if(m_interface->onDebugMessage)
                    m_interface->onDebugMessage(m_interface->onDebugMessage_userData, "Track title: %s", entry);
            }
        }
        else if(evt.subtype == MidiEvent::ST_INSTRTITLE)
        {
            insertDataToBankWithTerm(evt, m_dataBank, fr, length);
            const uint8_t *entry = getData(evt.data_block);

            if(m_interface->onDebugMessage)
                m_interface->onDebugMessage(m_interface->onDebugMessage_userData, "Instrument: %s", entry);
        }
        else if(evt.subtype == MidiEvent::ST_MARKER)
        {
            m_text_buffer.resize(length);

            if(fr.read(m_text_buffer.data(), 1, length) != length)
            {
                m_parsingErrorsString.append("parseEvent: Failed to read marker data!\n");
                evt.isValid = 0;
                return evt;
            }

            if(strEqual(m_text_buffer.data(), length, "loopstart"))
            {
                // Return a custom Loop Start event instead of Marker
                evt.subtype = MidiEvent::ST_LOOPSTART;
                return evt;
            }
            else if(strEqual(m_text_buffer.data(), length, "loopend"))
            {
                // Return a custom Loop End event instead of Marker
                evt.subtype = MidiEvent::ST_LOOPEND;
                return evt;
            }
            else if(length > 10 && strEqual(m_text_buffer.data(), 10, "loopstart="))
            {
                char loop_key[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                size_t key_len = length - 10;
                std::memcpy(loop_key, m_text_buffer.data() + 10, key_len > 10 ? 10 : key_len);

                evt.type = MidiEvent::T_SPECIAL;
                evt.subtype = MidiEvent::ST_LOOPSTACK_BEGIN;
                evt.data_loc_size = 1;
                evt.data_loc[0] = static_cast<uint8_t>(atoi(loop_key));

                if(m_interface->onDebugMessage)
                {
                    m_interface->onDebugMessage(
                        m_interface->onDebugMessage_userData,
                        "Stack Marker Loop Start at %d to %d level with %d loops",
                        m_loop.stackLevel,
                        m_loop.stackLevel + 1,
                        evt.data_loc[0]
                    );
                }

                return evt;
            }
            else if(length > 8 && strEqual(m_text_buffer.data(), 8, "loopend="))
            {
                evt.type = MidiEvent::T_SPECIAL;
                evt.subtype = MidiEvent::ST_LOOPSTACK_END;
                evt.data_loc_size = 0;

                if(m_interface->onDebugMessage)
                {
                    m_interface->onDebugMessage(
                        m_interface->onDebugMessage_userData,
                        "Stack Marker Loop %s at %d to %d level",
                        (evt.subtype == MidiEvent::ST_LOOPSTACK_END ? "End" : "Break"),
                        m_loop.stackLevel,
                        m_loop.stackLevel - 1
                    );
                }
                return evt;
            }
            else
            {
                insertDataToBankWithTerm(evt, m_dataBank, m_text_buffer.data(), length);
            }
        }
        else if(evtype == MidiEvent::ST_ENDTRACK)
        {
            status = -1; // Finalize track
        }
        else
        {
            insertDataToBank(evt, m_dataBank, fr, length);
        }

        return evt;
    }

    // Any normal event (80..EF)
    if(byte < 0x80)
    {
        byte = static_cast<uint8_t>(status | 0x80);
        // --ptr;
        fr.seek(-1, FileAndMemReader::CUR);
    }

    // Sys Com Song Select(Song #) [0-127]
    if(byte == MidiEvent::T_SYSCOMSNGSEL)
    {
        if(fr.tell() + 1 > end)
        {
            m_parsingErrorsString.append("parseEvent: Can't read System Command Song Select event - Unexpected end of track data.\n");
            evt.isValid = 0;
            return evt;
        }
        evt.type = byte;
        fr.read(evt.data_loc, 1, 1);
        evt.data_loc_size = 1;
        return evt;
    }

    // Sys Com Song Position Pntr [LSB, MSB]
    if(byte == MidiEvent::T_SYSCOMSPOSPTR)
    {
        if(fr.tell() + 2 > end)
        {
            m_parsingErrorsString.append("parseEvent: Can't read System Command Position Pointer event - Unexpected end of track data.\n");
            evt.isValid = 0;
            return evt;
        }

        evt.type = byte;
        fr.read(evt.data_loc, 1, 2);
        evt.data_loc_size = 2;
        return evt;
    }

    uint8_t midCh = byte & 0x0F, evType = (byte >> 4) & 0x0F;
    status = byte;
    evt.channel = midCh;
    evt.type = evType;

    switch(evType)
    {
    case MidiEvent::T_NOTEOFF: // 2 byte length
    case MidiEvent::T_NOTEON:
    case MidiEvent::T_NOTETOUCH:
    case MidiEvent::T_CTRLCHANGE:
    case MidiEvent::T_WHEEL:
        if(fr.tell() + 2 > end)
        {
            m_parsingErrorsString.append("parseEvent: Can't read regular 2-byte event - Unexpected end of track data.\n");
            evt.isValid = 0;
            break;
        }

        fr.read(evt.data_loc, 1, 2);
        evt.data_loc_size = 2;

        if((evType == MidiEvent::T_NOTEON) && (evt.data_loc[1] == 0))
        {
            evt.type = MidiEvent::T_NOTEOFF; // Note ON with zero velocity is Note OFF!
        }
        else if(evType == MidiEvent::T_CTRLCHANGE)
        {
            // 111'th loopStart controller (RPG Maker and others)
            if(m_format == Format_MIDI)
            {
                switch(evt.data_loc[0])
                {
                case 110:
                    if(m_loopFormat == Loop_Default)
                    {
                        // Change event type to custom Loop Start event and clear data
                        evt.type = MidiEvent::T_SPECIAL;
                        evt.subtype = MidiEvent::ST_LOOPSTART;
                        m_loopFormat = Loop_HMI;
                    }
                    else if(m_loopFormat == Loop_HMI)
                    {
                        // Repeating of 110'th point is BAD practice, treat as EMIDI
                        m_loopFormat = Loop_EMIDI;
                    }
                    break;

                case 111:
                    if(m_loopFormat == Loop_HMI)
                    {
                        // Change event type to custom Loop End event and clear data
                        evt.type = MidiEvent::T_SPECIAL;
                        evt.subtype = MidiEvent::ST_LOOPEND;
                    }
                    else if(m_loopFormat != Loop_EMIDI)
                    {
                        // Change event type to custom Loop Start event and clear data
                        evt.type = MidiEvent::T_SPECIAL;
                        evt.subtype = MidiEvent::ST_LOOPSTART;
                    }
                    break;

                case 113:
                    if(m_loopFormat == Loop_EMIDI)
                    {
                        // EMIDI does using of CC113 with same purpose as CC7
                        evt.data_loc[0] = 7;
                    }
                    break;
#if 0 //WIP
                case 116:
                    if(m_loopFormat == Loop_EMIDI)
                    {
                        evt.type = MidiEvent::T_SPECIAL;
                        evt.subtype = MidiEvent::ST_LOOPSTACK_BEGIN;
                        evt.data_loc[0] = evt.data_loc[1];

                        if(m_interface->onDebugMessage)
                        {
                            m_interface->onDebugMessage(
                                m_interface->onDebugMessage_userData,
                                "Stack EMIDI Loop Start at %d to %d level with %d loops",
                                m_loop.stackLevel,
                                m_loop.stackLevel + 1,
                                evt.data_loc[0]
                            );
                        }
                    }
                    break;

                case 117:  // Next/Break Loop Controller
                    if(m_loopFormat == Loop_EMIDI)
                    {
                        evt.type = MidiEvent::T_SPECIAL;
                        evt.subtype = MidiEvent::ST_LOOPSTACK_END;

                        if(m_interface->onDebugMessage)
                        {
                            m_interface->onDebugMessage(
                                m_interface->onDebugMessage_userData,
                                "Stack EMIDI Loop End at %d to %d level",
                                m_loop.stackLevel,
                                m_loop.stackLevel - 1
                            );
                        }
                    }
                    break;
#endif
                }
            }

            else if(m_format == Format_XMIDI)
            {
                switch(evt.data_loc[0])
                {
                case 116:  // For Loop Controller
                    evt.type = MidiEvent::T_SPECIAL;
                    evt.subtype = MidiEvent::ST_LOOPSTACK_BEGIN;
                    evt.data_loc[0] = evt.data_loc[1];
                    evt.data_loc_size = 1;

                    if(m_interface->onDebugMessage)
                    {
                        m_interface->onDebugMessage(
                            m_interface->onDebugMessage_userData,
                            "Stack XMI Loop Start at %d to %d level with %d loops",
                            m_loop.stackLevel,
                            m_loop.stackLevel + 1,
                            evt.data_loc[0]
                        );
                    }
                    break;

                case 117:  // Next/Break Loop Controller
                    evt.type = MidiEvent::T_SPECIAL;
                    evt.subtype = evt.data_loc[1] < 64 ?
                                MidiEvent::ST_LOOPSTACK_BREAK :
                                MidiEvent::ST_LOOPSTACK_END;
                    evt.data_loc_size = 0;

                    if(m_interface->onDebugMessage)
                    {
                        m_interface->onDebugMessage(
                            m_interface->onDebugMessage_userData,
                            "Stack XMI Loop %s at %d to %d level",
                            (evt.subtype == MidiEvent::ST_LOOPSTACK_END ? "End" : "Break"),
                            m_loop.stackLevel,
                            m_loop.stackLevel - 1
                        );
                    }
                    break;

                case 119:  // Callback Trigger
                    evt.type = MidiEvent::T_SPECIAL;
                    evt.subtype = MidiEvent::ST_CALLBACK_TRIGGER;
                    evt.data_loc[0] = evt.data_loc[1];
                    evt.data_loc_size = 1;
                    break;
                }
            }

            else if(m_format == Format_CMF)
            {
                switch(evt.data_loc[0])
                {
                case 102: // Song markers (0x66)
                    evt.type = MidiEvent::T_SPECIAL;
                    evt.subtype = MidiEvent::ST_CALLBACK_TRIGGER;
                    evt.data_loc[0] = evt.data_loc[1];
                    evt.data_loc_size = 1;
                    break;

                case 104: // Transpose Up (0x68), convert into pitch bend
                {
                    int16_t bend = 8192 + (((int)evt.data_loc[1] * 8192) / 128);
                    evt.type = MidiEvent::T_WHEEL;
                    evt.data_loc[0] = (bend & 0x7F);
                    evt.data_loc[1] = ((bend >> 7) & 0x7F);
                    evt.data_loc_size = 2;
                    break;
                }

                case 105: // Transpose Down (0x69), convert into pitch bend
                {
                    int16_t bend = 8192 - (((int)evt.data_loc[1] * 8192) / 128);
                    evt.type = MidiEvent::T_WHEEL;
                    evt.data_loc[0] = (bend & 0x7F);
                    evt.data_loc[1] = ((bend >> 7) & 0x7F);
                    evt.data_loc_size = 2;
                    break;
                }
                }
            }
        }

        return evt;
    case MidiEvent::T_PATCHCHANGE: // 1 byte length
    case MidiEvent::T_CHANAFTTOUCH:
        if(fr.tell() + 1 > end)
        {
            m_parsingErrorsString.append("parseEvent: Can't read regular 1-byte event - Unexpected end of track data.\n");
            evt.isValid = 0;
            return evt;
        }

        fr.read(evt.data_loc, 1, 1);
        evt.data_loc_size = 1;

        return evt;
    default:
        break;
    }

    return evt;
}

void BW_MidiSequencer::handleEvent(size_t track, const BW_MidiSequencer::MidiEvent &evt, int32_t &status)
{
    if(track == 0 && m_smfFormat < 2 && evt.type == MidiEvent::T_SPECIAL &&
       (evt.subtype == MidiEvent::ST_TEMPOCHANGE || evt.subtype == MidiEvent::ST_TIMESIGNATURE))
    {
        /* never reject track 0 timing events on SMF format != 2
           note: multi-track XMI convert to format 2 SMF */
    }
    else
    {
        if(m_trackSolo != ~static_cast<size_t>(0) && track != m_trackSolo)
            return;
        if(m_trackDisable[track])
            return;
    }

    if(m_interface->onEvent)
    {
        if(evt.data_block.size > 0)
            m_interface->onEvent(m_interface->onEvent_userData,
                                 evt.type, evt.subtype, evt.channel,
                                 getData(evt.data_block), evt.data_block.size);
        else
            m_interface->onEvent(m_interface->onEvent_userData,
                                 evt.type, evt.subtype, evt.channel,
                                 evt.data_loc, evt.data_loc_size);
    }

    if(evt.type == MidiEvent::T_SYSEX || evt.type == MidiEvent::T_SYSEX2) // Handle SysEx
    {
        m_interface->rt_systemExclusive(m_interface->rtUserData, getData(evt.data_block), evt.data_block.size);
        return;
    }

    if(evt.type == MidiEvent::T_SPECIAL)
    {
        // Special event FF
        uint_fast16_t  evtype = evt.subtype;
        size_t length = evt.data_block.size > 0 ? evt.data_block.size : static_cast<size_t>(evt.data_loc_size);
        const uint8_t *datau = evt.data_block.size > 0 ? getData(evt.data_block) : evt.data_loc;
        const char *data(length ? reinterpret_cast<const char *>(datau) : "\0\0\0\0\0\0\0\0");

        if(m_interface->rt_metaEvent) // Meta event hook
            m_interface->rt_metaEvent(m_interface->rtUserData, evtype, reinterpret_cast<const uint8_t*>(data), length);

        if(evtype == MidiEvent::ST_ENDTRACK) // End Of Track
        {
            status = -1;
            return;
        }

        if(evtype == MidiEvent::ST_TEMPOCHANGE) // Tempo change
        {
            m_tempo = m_invDeltaTicks * fraction<uint64_t>(readBEint(datau, length));
            return;
        }

        if(evtype == MidiEvent::ST_MARKER) // Meta event
        {
            // Do nothing! :-P
            return;
        }

        if(evtype == MidiEvent::ST_DEVICESWITCH)
        {
            if(m_interface->onDebugMessage)
                m_interface->onDebugMessage(m_interface->onDebugMessage_userData, "Switching another device: %.*s", length, data);
            if(m_interface->rt_deviceSwitch)
                m_interface->rt_deviceSwitch(m_interface->rtUserData, track, data, length);
            return;
        }

        // Turn on Loop handling when loop is enabled
        if(m_loopEnabled && !m_loop.invalidLoop)
        {
            if(evtype == MidiEvent::ST_LOOPSTART) // Special non-spec MIDI loop Start point
            {
                m_loop.caughtStart = true;
                return;
            }

            if(evtype == MidiEvent::ST_LOOPEND) // Special non-spec MIDI loop End point
            {
                m_loop.caughtEnd = true;
                return;
            }

            if(evtype == MidiEvent::ST_LOOPSTACK_BEGIN)
            {
                if(m_loop.skipStackStart)
                {
                    m_loop.skipStackStart = false;
                    return;
                }

                char x = data[0];
                size_t slevel = static_cast<size_t>(m_loop.stackLevel + 1);
                while(slevel >= m_loop.stack.size())
                {
                    LoopStackEntry e;
                    e.loops = x;
                    e.infinity = (x == 0);
                    e.start = 0;
                    e.end = 0;
                    m_loop.stack.push_back(e);
                }

                LoopStackEntry &s = m_loop.stack[slevel];
                s.loops = static_cast<int>(x);
                s.infinity = (x == 0);
                m_loop.caughtStackStart = true;
                return;
            }

            if(evtype == MidiEvent::ST_LOOPSTACK_END)
            {
                m_loop.caughtStackEnd = true;
                return;
            }

            if(evtype == MidiEvent::ST_LOOPSTACK_BREAK)
            {
                m_loop.caughtStackBreak = true;
                return;
            }
        }

        if(evtype == MidiEvent::ST_CALLBACK_TRIGGER)
        {
#if 0 /* Print all callback triggers events */
            if(m_interface->onDebugMessage)
                m_interface->onDebugMessage(m_interface->onDebugMessage_userData, "Callback Trigger: %02X", evt.data[0]);
#endif
            if(m_triggerHandler)
                m_triggerHandler(m_triggerUserData, static_cast<unsigned>(data[0]), track);
            return;
        }

        if(evtype == MidiEvent::ST_RAWOPL) // Special non-spec ADLMIDI special for IMF playback: Direct poke to AdLib
        {
            if(m_interface->rt_rawOPL)
                m_interface->rt_rawOPL(m_interface->rtUserData, datau[0], datau[1]);
            return;
        }

        if(evtype == MidiEvent::ST_SONG_BEGIN_HOOK)
        {
            if(m_interface->onSongStart)
                m_interface->onSongStart(m_interface->onSongStart_userData);
            return;
        }

        return;
    }

    if(evt.type == MidiEvent::T_SYSCOMSNGSEL ||
       evt.type == MidiEvent::T_SYSCOMSPOSPTR)
        return;

    size_t midCh = evt.channel;
    if(m_interface->rt_currentDevice)
        midCh += m_interface->rt_currentDevice(m_interface->rtUserData, track);

    status = evt.type;

    switch(evt.type)
    {
    case MidiEvent::T_NOTEOFF: // Note off
    {
        if(midCh < 16 && m_channelDisable[midCh])
            break; // Disabled channel
        uint8_t note = evt.data_loc[0];
        uint8_t vol = evt.data_loc[1];
        if(m_interface->rt_noteOff)
            m_interface->rt_noteOff(m_interface->rtUserData, static_cast<uint8_t>(midCh), note);
        if(m_interface->rt_noteOffVel)
            m_interface->rt_noteOffVel(m_interface->rtUserData, static_cast<uint8_t>(midCh), note, vol);
        break;
    }

    case MidiEvent::T_NOTEON: // Note on
    {
        if(midCh < 16 && m_channelDisable[midCh])
            break; // Disabled channel
        uint8_t note = evt.data_loc[0];
        uint8_t vol  = evt.data_loc[1];
        m_interface->rt_noteOn(m_interface->rtUserData, static_cast<uint8_t>(midCh), note, vol);
        break;
    }

    case MidiEvent::T_NOTEON_DURATED: // Note on with duration
    {
        if(midCh < 16 && m_channelDisable[midCh])
            break; // Disabled channel
        DuratedNote note;
        note.channel = evt.channel;
        note.note = evt.data_loc[0];
        note.velocity = evt.data_loc[1];
        note.ttl = readBEint(evt.data_loc + 2, 3);
        if(duratedNoteInsert(track, &note)) // Do call true Note ON only when note OFF is successfully added into the list!
            m_interface->rt_noteOn(m_interface->rtUserData, static_cast<uint8_t>(midCh), note.note, note.velocity);
        break;
    }

    case MidiEvent::T_NOTETOUCH: // Note touch
    {
        uint8_t note = evt.data_loc[0];
        uint8_t vol =  evt.data_loc[1];
        m_interface->rt_noteAfterTouch(m_interface->rtUserData, static_cast<uint8_t>(midCh), note, vol);
        break;
    }

    case MidiEvent::T_CTRLCHANGE: // Controller change
    {
        uint8_t ctrlno = evt.data_loc[0];
        uint8_t value =  evt.data_loc[1];
        m_interface->rt_controllerChange(m_interface->rtUserData, static_cast<uint8_t>(midCh), ctrlno, value);
        break;
    }

    case MidiEvent::T_PATCHCHANGE: // Patch change
    {
        m_interface->rt_patchChange(m_interface->rtUserData, static_cast<uint8_t>(midCh), evt.data_loc[0]);
        break;
    }

    case MidiEvent::T_CHANAFTTOUCH: // Channel after-touch
    {
        uint8_t chanat = evt.data_loc[0];
        m_interface->rt_channelAfterTouch(m_interface->rtUserData, static_cast<uint8_t>(midCh), chanat);
        break;
    }

    case MidiEvent::T_WHEEL: // Wheel/pitch bend
    {
        uint8_t a = evt.data_loc[0];
        uint8_t b = evt.data_loc[1];
        m_interface->rt_pitchBend(m_interface->rtUserData, static_cast<uint8_t>(midCh), b, a);
        break;
    }

    default:
        break;
    }//switch
}

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
    m_time.reset();

    // Clear any hanging timed notes
    duratedNoteClear();
}

void BW_MidiSequencer::setTempo(double tempo)
{
    m_tempoMultiplier = tempo;
}

bool BW_MidiSequencer::loadMIDI(const std::string &filename)
{
    FileAndMemReader file;
    file.openFile(filename.c_str());

    if(!loadMIDI(file))
        return false;

    return true;
}

bool BW_MidiSequencer::loadMIDI(const void *data, size_t size)
{
    FileAndMemReader file;
    file.openData(data, size);
    return loadMIDI(file);
}

template<class T>
class BufferGuard
{
    T *m_ptr;
public:
    BufferGuard() : m_ptr(NULL)
    {}

    ~BufferGuard()
    {
        set();
    }

    void set(T *p = NULL)
    {
        if(m_ptr)
            free(m_ptr);
        m_ptr = p;
    }
};

#ifdef BWMIDI_ENABLE_OPL_MUSIC_SUPPORT
/**
 * @brief Detect the EA-MUS file format
 * @param head Header part
 * @param fr Context with opened file data
 * @return true if given file was identified as EA-MUS
 */
static bool detectRSXX(const char *head, FileAndMemReader &fr)
{
    char headerBuf[7] = "";
    bool ret = false;

    // Try to identify RSXX format
    if(head[0] >= 0x5D && fr.fileSize() > reinterpret_cast<const uint8_t*>(head)[0])
    {
        fr.seek(head[0] - 0x10, FileAndMemReader::SET);
        fr.read(headerBuf, 1, 6);
        if(std::memcmp(headerBuf, "rsxx}u", 6) == 0)
            ret = true;
    }

    fr.seek(0, FileAndMemReader::SET);
    return ret;
}

/**
 * @brief Detect the Id-software Music File format
 * @param head Header part
 * @param fr Context with opened file data
 * @return true if given file was identified as IMF
 */
static bool detectIMF(const char *head, FileAndMemReader &fr)
{
    uint8_t raw[4];
    size_t end = static_cast<size_t>(head[0]) + 256 * static_cast<size_t>(head[1]);

    if(end & 3)
        return false;

    size_t backup_pos = fr.tell();
    int64_t sum1 = 0, sum2 = 0;
    fr.seek((end > 0 ? 2 : 0), FileAndMemReader::SET);

    for(size_t n = 0; n < 16383; ++n)
    {
        if(fr.read(raw, 1, 4) != 4)
            break;
        int64_t value1 = raw[0];
        value1 += raw[1] << 8;
        sum1 += value1;
        int64_t value2 = raw[2];
        value2 += raw[3] << 8;
        sum2 += value2;
    }

    fr.seek(static_cast<long>(backup_pos), FileAndMemReader::SET);

    return (sum1 > sum2);
}

static bool detectKLM(const char *head, FileAndMemReader &fr)
{
    uint16_t song_off = static_cast<uint64_t>(readLEint(head + 3, 2));

    if(head[2] != 0x01)
        return false;

    if(song_off > fr.fileSize())
        return false;

    if((song_off - 5) % 11 != 0) // Invalid offset!
        return false;

    return true;
}
#endif

bool BW_MidiSequencer::loadMIDI(FileAndMemReader &fr)
{
    size_t  fsize = 0;
    BW_MidiSequencer_UNUSED(fsize);
    m_parsingErrorsString.clear();

    assert(m_interface); // MIDI output interface must be defined!

    if(!fr.isValid())
    {
        m_errorString.set("Invalid data stream!\n");
#ifndef _WIN32
        m_errorString.append(std::strerror(errno));
#endif
        return false;
    }

    m_atEnd            = false;
    m_loop.fullReset();
    m_loop.caughtStart = true;

    m_format = Format_MIDI;
    m_smfFormat = 0;

    m_cmfInstruments.clear();
    m_rawSongsData.clear();

    const size_t headerSize = 4 + 4 + 2 + 2 + 2; // 14
    char headerBuf[headerSize] = "";

    fsize = fr.read(headerBuf, 1, headerSize);
    if(fsize < headerSize)
    {
        m_errorString.set("Unexpected end of file at header!\n");
        return false;
    }


    if(std::memcmp(headerBuf, "MThd\0\0\0\6", 8) == 0)
    {
        fr.seek(0, FileAndMemReader::SET);
        return parseSMF(fr);
    }

    if(std::memcmp(headerBuf, "RIFF", 4) == 0)
    {
        fr.seek(0, FileAndMemReader::SET);
        return parseRMI(fr);
    }

    if(std::memcmp(headerBuf, "GMF\x1", 4) == 0)
    {
        fr.seek(0, FileAndMemReader::SET);
        return parseGMF(fr);
    }

    if(std::memcmp(headerBuf, "MUS\x1A", 4) == 0)
    {
        fr.seek(0, FileAndMemReader::SET);
        return parseMUS(fr);
    }

    if(std::memcmp(headerBuf, "HMI-MIDISONG06", 14) == 0 || std::memcmp(headerBuf, "HMIMIDIP", 8) == 0)
    {
        fr.seek(0, FileAndMemReader::SET);
        return parseHMI(fr);
    }

#ifndef BWMIDI_DISABLE_XMI_SUPPORT
    if((std::memcmp(headerBuf, "FORM", 4) == 0) && (std::memcmp(headerBuf + 8, "XDIR", 4) == 0))
    {
        fr.seek(0, FileAndMemReader::SET);
        return parseXMI(fr);
    }
#endif

#ifdef BWMIDI_ENABLE_OPL_MUSIC_SUPPORT
    if(std::memcmp(headerBuf, "CTMF", 4) == 0)
    {
        fr.seek(0, FileAndMemReader::SET);
        return parseCMF(fr);
    }

    if(detectRSXX(headerBuf, fr))
    {
        fr.seek(0, FileAndMemReader::SET);
        return parseRSXX(fr);
    }

    if(detectKLM(headerBuf, fr))
    {
        fr.seek(0, FileAndMemReader::SET);
        return parseKLM(fr);
    }

    // This file type should be parsed last!
    if(detectIMF(headerBuf, fr))
    {
        fr.seek(0, FileAndMemReader::SET);
        return parseIMF(fr);
    }
#endif

    m_errorString.set("Unknown or unsupported file format");
    return false;
}

#ifdef BWMIDI_ENABLE_OPL_MUSIC_SUPPORT
bool BW_MidiSequencer::parseIMF(FileAndMemReader &fr)
{
    const size_t    deltaTicks = 1;
    const size_t    trackCount = 1;
    // const uint32_t  imfTempo = 1428;
    size_t          imfEnd = 0;
    uint64_t        abs_position = 0;
    uint8_t         imfRaw[4];

    MidiTrackRow    evtPos;
    MidiEvent       event;

    std::memset(&event, 0, sizeof(event));
    event.isValid = 1;

    std::vector<TempoEvent> temposList;

    m_format = Format_IMF;

    buildSmfSetupReset(trackCount);

    m_invDeltaTicks = fraction<uint64_t>(1, 1000000l * static_cast<uint64_t>(deltaTicks));
    m_tempo = fraction<uint64_t>(1, static_cast<uint64_t>(deltaTicks) * 2);

    fr.seek(0, FileAndMemReader::SET);
    if(fr.read(imfRaw, 1, 2) != 2)
    {
        m_errorString.set("Unexpected end of file at header!\n");
        return false;
    }

    imfEnd = static_cast<size_t>(imfRaw[0]) + 256 * static_cast<size_t>(imfRaw[1]);

    // Define the playing tempo
    event.type = MidiEvent::T_SPECIAL;
    event.subtype = MidiEvent::ST_TEMPOCHANGE;
    event.data_loc_size = 3;
    event.data_loc[0] = 0x00; // static_cast<uint8_t>((imfTempo >> 16) & 0xFF);
    event.data_loc[1] = 0x05; // static_cast<uint8_t>((imfTempo >> 8) & 0xFF);
    event.data_loc[2] = 0x94; // static_cast<uint8_t>((imfTempo & 0xFF));
    addEventToBank(evtPos, event);
    TempoEvent tempoEvent = {readBEint(event.data_loc, event.data_loc_size), 0};
    temposList.push_back(tempoEvent);

    // Define the draft for IMF events
    event.type = MidiEvent::T_SPECIAL;
    event.subtype = MidiEvent::ST_RAWOPL;
    event.data_loc_size = 2;

    fr.seek((imfEnd > 0) ? 2 : 0, FileAndMemReader::SET);

    if(imfEnd == 0) // IMF Type 0 with unlimited file length
        imfEnd = fr.fileSize();

    while(fr.tell() < imfEnd && !fr.eof())
    {
        if(fr.read(imfRaw, 1, 4) != 4)
            break;

        event.data_loc[0] = imfRaw[0]; // port index
        event.data_loc[1] = imfRaw[1]; // port value
        event.isValid = 1;

        addEventToBank(evtPos, event);
        evtPos.delay = static_cast<uint64_t>(imfRaw[2]) + 256 * static_cast<uint64_t>(imfRaw[3]);

        if(evtPos.delay > 0)
        {
            evtPos.absPos = abs_position;
            abs_position += evtPos.delay;
            m_trackData[0].push_back(evtPos);
            evtPos.clear();
        }
    }

    // Add final row
    evtPos.absPos = abs_position;
    m_trackData[0].push_back(evtPos);

    if(!m_trackData[0].empty())
        m_currentPosition.track[0].pos = m_trackData[0].begin();

    buildTimeLine(temposList);

    return true;
}

bool BW_MidiSequencer::parseKLM(FileAndMemReader &fr)
{
// #define KLM_DEBUG
    const size_t headerSize = 5;
    char headerBuf[headerSize] = {0, 0, 0, 0, 0};
    size_t fsize = 0, file_size;
    uint64_t tempo = 0, musOffset = 0;
    uint64_t abs_position = 0;

    MidiTrackRow    evtPos;
    MidiEvent       event;

    std::memset(&event, 0, sizeof(event));
    event.isValid = 1;

    fsize = fr.read(headerBuf, 1, headerSize);
    if(fsize < headerSize)
    {
        fr.close();
        m_errorString.set("Unexpected end of file at header!\n");
        return false;
    }

    file_size = fr.fileSize();
    tempo = readLEint(headerBuf + 0, 2);
    musOffset = readLEint(headerBuf + 3, 2);

    if(musOffset >= file_size)
    {
        fr.close();
        m_errorString.set("Song data offset is out of file size\n");
        return false;
    }

    m_format = Format_KLM;

    buildSmfSetupReset(1);

    m_invDeltaTicks = fraction<uint64_t>(1, 1000000l * static_cast<uint64_t>(tempo));
    m_tempo         = fraction<uint64_t>(1, static_cast<uint64_t>(tempo) * 2);

    uint64_t ins_count = 0;

    // Used temporarily
    m_cmfInstruments.reserve(static_cast<size_t>(ins_count));
    CmfInstrument inst;

    while(fr.tell() < musOffset && !fr.eof())
    {
        fsize = fr.read(inst.data, 1, 11);
        if(fsize < 11)
        {
            fr.close();
            m_cmfInstruments.clear();
            m_errorString.set("Unexpected file ending on attempt to read KLM instruments raw data!");
            return false;
        }
        m_cmfInstruments.push_back(inst);
    }

    if(fr.tell() != musOffset)
    {
        fr.close();
        m_cmfInstruments.clear();
        m_errorString.set("Invalid KLM file: instrument data goes after the song offset!");
        return false;
    }

    // Define the draft for IMF events
    event.type = MidiEvent::T_SPECIAL;
    event.subtype = MidiEvent::ST_RAWOPL;
    event.data_loc_size = 2;

#ifdef KLM_DEBUG
    size_t err_off = 0;
#endif
    uint8_t cmd = 0, chan = 0, data[2], eof_reached = 0;
    uint8_t reg_bd_state = 0x00;
    uint8_t reg_b0_state[11] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t reg_43_state[11] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t inst_off_car = 0;
    uint8_t inst_off_mod = 0;

    const uint8_t op_map[12] =
    {
        0x00, 0x03,
        0x01, 0x04,
        0x02, 0x05,
        0x08, 0x0B,
        0x09, 0x0C,
        0x0A, 0x0D
    };

    const uint8_t rm_map[10] =
    {
        0x10, 0x13,
        0xFF, 0x14,
        0x12, 0xFF,
        0xFF, 0x15,
        0x11, 0xFF
    };

    const uint8_t rm_vol_map[5] =
    {
        0x13,
        0x14,
        0x12,
        0x15,
        0x11,
    };

    // Activate rhythm mode
    reg_bd_state = 0x20;
    event.data_loc[0] = 0xBD;
    event.data_loc[1] = reg_bd_state;
    event.isValid = 1;
    addEventToBank(evtPos, event);
    evtPos.delay = 0;

    // Initial rhythm frequencies
    const int rhythm_a0[] = {0x57, 0x03, 0x57};
    const int rhythm_b0[] = {0x0A, 0x0A, 0x09};

    for(int c = 6; c <= 8; ++c)
    {
        event.data_loc[0] = 0xA0 + c;
        event.data_loc[1] = rhythm_a0[c - 6];
        addEventToBank(evtPos, event);

        reg_b0_state[c] = rhythm_b0[c - 6] & 0xDF;
        event.data_loc[0] = 0xB0 + c;
        event.data_loc[1] = rhythm_b0[c - 6] & 0xDF;
        addEventToBank(evtPos, event);
    }

#ifdef KLM_DEBUG
    err_off = fr.tell();
    printf("Instriments in KML: %u\n", static_cast<unsigned>(m_cmfInstruments.size()));
    fflush(stdout);
#endif

    while(!eof_reached && !fr.eof())
    {
#ifdef KLM_DEBUG
        err_off = fr.tell();
#endif
        fsize = fr.read(&cmd, 1, 1);
        if(fsize < 1)
        {
            fr.close();
            m_cmfInstruments.clear();
            m_errorString.set("Unexpected file ending on attempt to read KLM song command data!");
            return false;
        }

        chan = cmd & 0x0F;

#ifdef KLM_DEBUG
        printf("0x%X: CMD=0x%02X, chan=0x%02X (Full byte 0x%02X)\n", static_cast<unsigned>(err_off), cmd & 0xF0, chan, cmd);
        fflush(stdout);
#endif

        if((cmd & 0xF0) != 0xF0 && chan >= 11)
        {
            fr.close();
            m_cmfInstruments.clear();
            m_errorString.set("Channel out of range!");
            return false;
        }

        switch(cmd & 0xF0)
        {
        case 0x00: // Note OFF;
            switch(chan)
            {
            case 0: case 1: case 2: case 3: case 4: case 5:
                reg_b0_state[chan] &= 0xDF;
                event.data_loc[0] = 0xB0 + chan;
                event.data_loc[1] = reg_b0_state[chan];
                event.isValid = 1;
                addEventToBank(evtPos, event);
                break;

            default:
                switch(chan)
                {
                case 6:
                    reg_bd_state &= ~0x10;
                    break;
                case 7:
                    reg_bd_state &= ~0x08;
                    break;
                case 8:
                    reg_bd_state &= ~0x04;
                    break;
                case 9:
                    reg_bd_state &= ~0x02;
                    break;
                case 0x0A:
                    reg_bd_state &= ~0x01;
                    break;
                }

                event.data_loc[0] = 0xBD;
                event.data_loc[1] = reg_bd_state;
                event.isValid = 1;
                addEventToBank(evtPos, event);
                break;
            }

            break;
        case 0x10: // Note ON with frequency (only channels 0 - 5, and bass drum at 6, other channels cmd gets replaced with 0x40)
            if(chan > 6)
            {
                switch(chan)
                {
                case 6:
                    reg_bd_state |= 0x10;
                    break;
                case 7:
                    reg_bd_state |= 0x08;
                    break;
                case 8:
                    reg_bd_state |= 0x04;
                    break;
                case 9:
                    reg_bd_state |= 0x02;
                    break;
                case 0x0A:
                    reg_bd_state |= 0x01;
                    break;
                }

                event.data_loc[0] = 0xBD;
                event.data_loc[1] = reg_bd_state;
                addEventToBank(evtPos, event);
                break;
            }

            fsize = fr.read(data, 1, 2);
            if(fsize < 2)
            {
                fr.close();
                m_cmfInstruments.clear();
                m_errorString.set("Unexpected file ending on attempt to read KLM song note-on frequency data!");
                return false;
            }

#ifdef KLM_DEBUG
            printf(" -- Data 2 byte\n");
            fflush(stdout);
#endif

            event.isValid = 1;

            event.data_loc[0] = 0xA0 + chan;
            event.data_loc[1] = data[0];
            addEventToBank(evtPos, event);

            if(chan < 6)
            {
                reg_b0_state[chan] = data[1] & 0xDF;
                reg_b0_state[chan] |= 0x20;
            }
            else if(chan <= 8)
                reg_b0_state[chan] = data[1] & 0xDF;

            event.data_loc[0] = 0xB0 + chan;
            event.data_loc[1] = reg_b0_state[chan];
            addEventToBank(evtPos, event);

            break;

        case 0x20: // Volume
            fsize = fr.read(data, 1, 1);
            if(fsize < 1)
            {
                fr.close();
                m_cmfInstruments.clear();
                m_errorString.set("Unexpected file ending on attempt to read KLM song volume data!");
                return false;
            }

#ifdef KLM_DEBUG
            printf(" -- Data 1 byte\n");
            fflush(stdout);
#endif

            reg_43_state[chan] &= 0xC0;
            reg_43_state[chan] |= 0x3F & ((127 - data[0]) / 2);

            if(chan < 6)
                event.data_loc[0] = 0x40 + op_map[(chan * 2) + 1];
            else if(chan <= 11)
                event.data_loc[0] = 0x40 + rm_vol_map[chan - 6];

            event.data_loc[1] = reg_43_state[chan];
            event.isValid = 1;
            addEventToBank(evtPos, event);
            break;

        case 0x30: // Set Instrument
            fsize = fr.read(data, 1, 1);
            if(fsize < 1)
            {
                fr.close();
                m_cmfInstruments.clear();
                m_errorString.set("Unexpected file ending on attempt to read KLM song instrument select data!");
                return false;
            }

#ifdef KLM_DEBUG
            printf(" -- Data 1 byte (0x%02X)\n", data[0]);
            fflush(stdout);
#endif

            if(data[0] >= m_cmfInstruments.size())
            {
                fr.close();
                m_cmfInstruments.clear();
                m_errorString.set("Selected instrument in KLM file is out of range!");
                return false;
            }

            if(chan < 6)
            {
                inst_off_mod = op_map[chan * 2];
                inst_off_car = op_map[(chan * 2) + 1];
            }
            else
            {
                inst_off_mod = rm_map[(chan - 6) * 2];
                inst_off_car = rm_map[((chan - 6) * 2) + 1];
            }

            event.isValid = 1;

            if(inst_off_mod != 0xFF)
            {
                uint8_t *ins = m_cmfInstruments[data[0]].data;
                event.data_loc[0] = 0x40 + inst_off_mod;
                event.data_loc[1] = ins[0];
                addEventToBank(evtPos, event);

                event.data_loc[0] = 0x60 + inst_off_mod;
                event.data_loc[1] = ins[2];
                addEventToBank(evtPos, event);

                event.data_loc[0] = 0x80 + inst_off_mod;
                event.data_loc[1] = ins[4];
                addEventToBank(evtPos, event);

                event.data_loc[0] = 0x20 + inst_off_mod;
                event.data_loc[1] = ins[6];
                addEventToBank(evtPos, event);

                event.data_loc[0] = 0xE0 + inst_off_mod;
                event.data_loc[1] = ins[8];
                addEventToBank(evtPos, event);
            }

            if(inst_off_car != 0xFF)
            {
                uint8_t *ins = m_cmfInstruments[data[0]].data;

                reg_43_state[chan] = ins[1];
                event.data_loc[0] = 0x40 + inst_off_car;
                event.data_loc[1] = reg_43_state[chan];
                addEventToBank(evtPos, event);

                event.data_loc[0] = 0x60 + inst_off_car;
                event.data_loc[1] = ins[3];
                addEventToBank(evtPos, event);

                event.data_loc[0] = 0x80 + inst_off_car;
                event.data_loc[1] = ins[5];
                addEventToBank(evtPos, event);

                event.data_loc[0] = 0x20 + inst_off_car;
                event.data_loc[1] = ins[7];
                addEventToBank(evtPos, event);

                event.data_loc[0] = 0xE0 + inst_off_car;
                event.data_loc[1] = ins[9];
                addEventToBank(evtPos, event);
            }

            if(chan <= 6) // Only melodic and bass drum!
            {
                uint8_t *ins = m_cmfInstruments[data[0]].data;
                event.data_loc[0] = 0xC0 + chan;
                event.data_loc[1] = ins[10] | 0x30;
                addEventToBank(evtPos, event);
            }
            break;

        case 0x40: // Note ON without frequency
            event.isValid = 1;

            if(chan < 6)
            {
                reg_b0_state[chan] |= 0x20;
                event.data_loc[0] = 0xB0 + chan;
                event.data_loc[1] = reg_b0_state[chan];
                addEventToBank(evtPos, event);
            }
            else
            {
                switch(chan)
                {
                case 6:
                    reg_bd_state |= 0x10;
                    break;
                case 7:
                    reg_bd_state |= 0x08;
                    break;
                case 8:
                    reg_bd_state |= 0x04;
                    break;
                case 9:
                    reg_bd_state |= 0x02;
                    break;
                case 0x0A:
                    reg_bd_state |= 0x01;
                    break;
                }

                event.data_loc[0] = 0xBD;
                event.data_loc[1] = reg_bd_state;
                addEventToBank(evtPos, event);
            }
            break;

        case 0xF0: // Special event
            switch(cmd)
            {
            case 0xFD: // Delay
                fsize = fr.read(data, 1, 1);
                if(fsize < 1)
                {
                    fr.close();
                    m_cmfInstruments.clear();
                    m_errorString.set("Unexpected file ending on attempt to read KLM song short delay data!");
                    return false;
                }

#ifdef KLM_DEBUG
                printf(" -- DELAY 1 byte (%d)\n", data[0]);
                fflush(stdout);
#endif

                evtPos.delay = data[0];
                evtPos.delay *= 2;
                if(evtPos.delay > 0)
                {
                    evtPos.absPos = abs_position;
                    abs_position += evtPos.delay;
                    m_trackData[0].push_back(evtPos);
                    evtPos.clear();
                }
                break;

            case 0xFE: // Long delay
                fsize = fr.read(data, 1, 2);
                if(fsize < 2)
                {
                    fr.close();
                    m_cmfInstruments.clear();
                    m_errorString.set("Unexpected file ending on attempt to read KLM song short delay data!");
                    return false;
                }

#ifdef KLM_DEBUG
                printf(" -- DELAY 2 bytes (%u)\n", data[0] + (static_cast<uint16_t>(data[1]) << 8));
                fflush(stdout);
#endif

                evtPos.delay = data[0];
                evtPos.delay += static_cast<uint16_t>(data[1]) << 8;
                evtPos.delay *= 2;
                if(evtPos.delay > 0)
                {
                    evtPos.absPos = abs_position;
                    abs_position += evtPos.delay;
                    m_trackData[0].push_back(evtPos);
                    evtPos.clear();
                }
                break;

            case 0xFF: // Song End
                eof_reached = 1;
                if(evtPos.events_begin < evtPos.events_end) // If anything left not written, write!
                {
                    evtPos.absPos = abs_position;
                    abs_position += evtPos.delay;
                    m_trackData[0].push_back(evtPos);
                    evtPos.events_begin = 0;
                    evtPos.events_end = 0;
                    evtPos.clear();
                }
                break;

            default: // Forbidden value!
                fr.close();
                m_cmfInstruments.clear();
                m_errorString.set("Received unsupported special song command value!");
                return false;
            }
            break;

        default: // Forbidden value!
#ifdef KLM_DEBUG
            err_off = fr.tell();
#endif
            fr.close();
            m_cmfInstruments.clear();
            m_errorString.set("Received unsupported normal song command value!");
            return false;
        }
    }

    m_cmfInstruments.clear();

    // Add final row
    evtPos.absPos = abs_position;
    m_trackData[0].push_back(evtPos);

    if(!m_trackData[0].empty())
        m_currentPosition.track[0].pos = m_trackData[0].begin();

    buildTimeLine(std::vector<TempoEvent>());

    return true;
}

bool BW_MidiSequencer::parseRSXX(FileAndMemReader &fr)
{
    const size_t headerSize = 14;
    char headerBuf[headerSize] = "";
    size_t fsize = 0, deltaTicks = 192;
    size_t trackLength, trackPos, totalGotten;
    LoopPointParseState loopState;
    char start;

    std::vector<TempoEvent> temposList;

    std::memset(&loopState, 0, sizeof(loopState));


    fsize = fr.read(headerBuf, 1, headerSize);
    if(fsize < headerSize)
    {
        m_errorString.set("Unexpected end of file at header!\n");
        return false;
    }

    // Try to identify RSXX format
    start = headerBuf[0];
    if(start < 0x5D)
    {
        m_errorString.set("RSXX song too short!\n");
        return false;
    }
    else
    {
        fr.seek(start - 0x10, FileAndMemReader::SET);
        fr.read(headerBuf, 1, 6);
        if(std::memcmp(headerBuf, "rsxx}u", 6) == 0)
        {
            m_format = Format_RSXX;
            fr.seek(start, FileAndMemReader::SET);
            deltaTicks = 60;
        }
        else
        {
            m_errorString.set("Invalid RSXX header!\n");
            return false;
        }
    }

    m_invDeltaTicks = fraction<uint64_t>(1, 1000000l * static_cast<uint64_t>(deltaTicks));
    m_tempo         = fraction<uint64_t>(1,            static_cast<uint64_t>(deltaTicks));

    totalGotten = 0;

    // Read track header
    trackPos = fr.tell();
    fr.seek(0, FileAndMemReader::END);
    trackLength = fr.tell() - trackPos;
    fr.seek(static_cast<long>(trackPos), FileAndMemReader::SET);
    totalGotten += trackLength;

    //Finalize raw track data with a zero
    // rawTrackData[0].push_back(0);

    if(totalGotten == 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Empty track data");
        return false;
    }

    m_smfFormat = 0;
    m_loop.stackLevel   = -1;

    buildSmfSetupReset(1);

    // Build new MIDI events table
    if(!buildSmfTrack(fr, 0, trackLength, temposList, loopState))
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": MIDI data parsing error has occouped!\n");
        m_errorString.append(m_parsingErrorsString.c_str());
        return false;
    }

    installLoop(loopState);
    buildTimeLine(temposList, loopState.loopStartTicks, loopState.loopEndTicks);

    return true;
}

bool BW_MidiSequencer::parseCMF(FileAndMemReader &fr)
{
    const size_t headerSize = 14;
    char headerBuf[headerSize] = "";
    size_t fsize = 0, deltaTicks = 192, totalGotten = 0, trackPos, trackLength;
    uint64_t ins_start, mus_start, ticks, ins_count;
    LoopPointParseState loopState;

    std::vector<TempoEvent> temposList;

    std::memset(&loopState, 0, sizeof(loopState));


    fsize = fr.read(headerBuf, 1, headerSize);
    if(fsize < headerSize)
    {
        m_errorString.set("Unexpected end of file at header!\n");
        return false;
    }

    if(std::memcmp(headerBuf, "CTMF", 4) != 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Invalid format, CTMF signature is not found!\n");
        return false;
    }

    m_format = Format_CMF;

    //unsigned version   = ReadLEint(HeaderBuf+4, 2);
    ins_start = readLEint(headerBuf + 6, 2);
    mus_start = readLEint(headerBuf + 8, 2);
    //unsigned deltas    = ReadLEint(HeaderBuf+10, 2);
    ticks     = readLEint(headerBuf + 12, 2);
    // Read title, author, remarks start offsets in file
    fsize = fr.read(headerBuf, 1, 6);
    if(fsize < 6)
    {
        fr.close();
        m_errorString.set("Unexpected file ending on attempt to read CTMF header!");
        return false;
    }

    //uint64_t notes_starts[3] = {readLEint(headerBuf + 0, 2), readLEint(headerBuf + 0, 4), readLEint(headerBuf + 0, 6)};
    fr.seek(16, FileAndMemReader::CUR); // Skip the channels-in-use table
    fsize = fr.read(headerBuf, 1, 4);
    if(fsize < 4)
    {
        fr.close();
        m_errorString.set("Unexpected file ending on attempt to read CMF instruments block header!");
        return false;
    }

    ins_count =  readLEint(headerBuf + 0, 2);
    fr.seek(static_cast<long>(ins_start), FileAndMemReader::SET);

    m_cmfInstruments.reserve(static_cast<size_t>(ins_count));
    for(uint64_t i = 0; i < ins_count; ++i)
    {
        CmfInstrument inst;
        fsize = fr.read(inst.data, 1, 16);
        if(fsize < 16)
        {
            fr.close();
            m_errorString.set("Unexpected file ending on attempt to read CMF instruments raw data!");
            return false;
        }
        m_cmfInstruments.push_back(inst);
    }

    fr.seeku(mus_start, FileAndMemReader::SET);
    deltaTicks = (size_t)ticks;

    m_invDeltaTicks = fraction<uint64_t>(1, 1000000l * static_cast<uint64_t>(deltaTicks));
    m_tempo         = fraction<uint64_t>(1,            static_cast<uint64_t>(deltaTicks));

    totalGotten = 0;

    // Read track header
    trackPos = fr.tell();
    fr.seek(0, FileAndMemReader::END);
    trackLength = fr.tell() - trackPos;
    fr.seek(static_cast<long>(trackPos), FileAndMemReader::SET);
    totalGotten += trackLength;

    if(totalGotten == 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Empty track data");
        return false;
    }

    buildSmfSetupReset(1);

    // Build new MIDI events table
    if(!buildSmfTrack(fr, 0, trackLength, temposList, loopState))
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": MIDI data parsing error has occouped!\n");
        m_errorString.append(m_parsingErrorsString.c_str());
        return false;
    }

    installLoop(loopState);
    buildTimeLine(temposList, loopState.loopStartTicks, loopState.loopEndTicks);

    return true;
}
#endif // BWMIDI_ENABLE_OPL_MUSIC_SUPPORT

bool BW_MidiSequencer::parseGMF(FileAndMemReader &fr)
{
    const size_t headerSize = 14;
    char headerBuf[headerSize] = "";
    size_t fsize = 0, deltaTicks = 192, totalGotten, trackLength, trackPos;
    LoopPointParseState loopState;

    std::vector<TempoEvent> temposList;

    std::memset(&loopState, 0, sizeof(loopState));


    fsize = fr.read(headerBuf, 1, headerSize);
    if(fsize < headerSize)
    {
        m_errorString.set("Unexpected end of file at header!\n");
        return false;
    }

    if(std::memcmp(headerBuf, "GMF\x1", 4) != 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Invalid format, GMF\\x1 signature is not found!\n");
        return false;
    }

    fr.seek(7 - static_cast<long>(headerSize), FileAndMemReader::CUR);

    m_invDeltaTicks = fraction<uint64_t>(1, 1000000l * static_cast<uint64_t>(deltaTicks));
    m_tempo         = fraction<uint64_t>(1,            static_cast<uint64_t>(deltaTicks) * 2);
    // static const unsigned char EndTag[4] = {0xFF, 0x2F, 0x00, 0x00};
    totalGotten = 0;

    // Read track header
    trackPos = fr.tell();
    fr.seek(0, FileAndMemReader::END);
    trackLength = fr.tell() - trackPos;
    fr.seek(static_cast<long>(trackPos), FileAndMemReader::SET);
    totalGotten += trackLength;

    if(totalGotten == 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Empty track data");
        return false;
    }

    buildSmfSetupReset(1);

    // Build new MIDI events table
    if(!buildSmfTrack(fr, 0, trackLength, temposList, loopState))
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": MIDI data parsing error has occouped!\n");
        m_errorString.append(m_parsingErrorsString.c_str());
        return false;
    }

    installLoop(loopState);
    buildTimeLine(temposList, loopState.loopStartTicks, loopState.loopEndTicks);

    return true;
}

bool BW_MidiSequencer::parseSMF(FileAndMemReader &fr)
{
    const size_t headerSize = 14; // 4 + 4 + 2 + 2 + 2
    char headerBuf[headerSize] = "";
    size_t fsize = 0;
    size_t deltaTicks = 192, trackCount = 1;
    unsigned smfFormat = 0;

    fsize = fr.read(headerBuf, 1, headerSize);
    if(fsize < headerSize)
    {
        m_errorString.set("Unexpected end of file at header!\n");
        return false;
    }

    if(std::memcmp(headerBuf, "MThd\0\0\0\6", 8) != 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Invalid format, MThd signature is not found!\n");
        return false;
    }

    smfFormat  = static_cast<unsigned>(readBEint(headerBuf + 8,  2));
    trackCount = static_cast<size_t>(readBEint(headerBuf + 10, 2));
    deltaTicks = static_cast<size_t>(readBEint(headerBuf + 12, 2));

    if(smfFormat > 2)
        smfFormat = 1;

    m_invDeltaTicks = fraction<uint64_t>(1, 1000000l * static_cast<uint64_t>(deltaTicks));
    m_tempo         = fraction<uint64_t>(1,            static_cast<uint64_t>(deltaTicks) * 2);

    size_t totalGotten = 0;
    size_t tracks_begin = fr.tell();

    // Read track sizes
    for(size_t tk = 0; tk < trackCount; ++tk)
    {
        // Read track header
        size_t trackLength, prev_pos;

        fsize = fr.read(headerBuf, 1, 8);
        if((fsize < 8) || (std::memcmp(headerBuf, "MTrk", 4) != 0))
        {
            m_errorString.set(fr.fileName().c_str());
            m_errorString.append(": Invalid format, MTrk signature is not found!\n");
            return false;
        }

        trackLength = (size_t)readBEint(headerBuf + 4, 4);
        prev_pos = fr.tell();

        fr.seek(trackLength, FileAndMemReader::CUR);

        if(fr.tell() - trackLength != prev_pos)
        {
            m_errorString.set(fr.fileName().c_str());
            m_errorString.append(": Unexpected file ending while getting raw track data!\n");
            return false;
        }

        totalGotten += trackLength;
    }

    if(totalGotten == 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Empty track data");
        return false;
    }

    // Build new MIDI events table
    if(!buildSmfTrackData(fr, tracks_begin, trackCount))
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": MIDI data parsing error has occouped!\n");
        m_errorString.append(m_parsingErrorsString.c_str());
        return false;
    }

    m_smfFormat = smfFormat;
    m_loop.stackLevel   = -1;

    return true;
}

bool BW_MidiSequencer::parseRMI(FileAndMemReader &fr)
{
    const size_t headerSize = 4 + 4 + 2 + 2 + 2; // 14
    char headerBuf[headerSize] = "";

    size_t fsize = fr.read(headerBuf, 1, headerSize);
    if(fsize < headerSize)
    {
        m_errorString.set("Unexpected end of file at header!\n");
        return false;
    }

    if(std::memcmp(headerBuf, "RIFF", 4) != 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Invalid format, RIFF signature is not found!\n");
        return false;
    }

    m_format = Format_MIDI;

    fr.seek(6l, FileAndMemReader::CUR);
    return parseSMF(fr);
}

bool BW_MidiSequencer::parseMUS(FileAndMemReader &fr)
{
    const size_t headerSize = 16;
    size_t fsize = 0;
    uint8_t headerBuf[headerSize];
    uint16_t mus_lenSong, mus_offSong, mus_channels1, /*mus_channels2,*/ mus_numInstr, song_read = 0;
    uint8_t numBuffer[2];
    int8_t  channel_map[16];
    uint8_t channel_volume[16];
    uint8_t channel_cur = 0;
    uint64_t abs_position = 0;
    int32_t delay = 0;
    std::vector<uint16_t> mus_instrs;
    std::vector<TempoEvent> temposList;

    const uint8_t controller_map[15] =
    {
        0,    0,    0x01, 0x07, 0x0A,
        0x0B, 0x5B, 0x5D, 0x40, 0x43,
        0x78, 0x7B, 0x7E, 0x7F, 0x79,
    };

    size_t mus_len = fr.fileSize();

    fsize = fr.read(headerBuf, 1, headerSize);
    if(fsize < headerSize)
    {
        m_errorString.set("MUS: Unexpected end of file at header!\n");
        return false;
    }

    if(std::memcmp(headerBuf, "MUS\x1A", 4) != 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Invalid format, MUS\\x1A signature is not found!\n");
        return false;
    }

    mus_lenSong = readLEint16(headerBuf + 4, 2);
    mus_offSong = readLEint16(headerBuf + 6, 2);
    mus_channels1 = readLEint16(headerBuf + 8, 2);
    // mus_channels2 = readLEint16(headerBuf + 12, 2);
    mus_numInstr = readLEint16(headerBuf + 14, 2);

    if(headerSize + (static_cast<size_t>(mus_numInstr) * 2) > mus_offSong)
    {
        m_errorString.set("MUS file is invalid: instruments list is larger than song offset!\n");
        return false;
    }

    if(mus_len < mus_lenSong + mus_offSong)
    {
        m_errorString.set("MUS file is invalid: song length is longer than file size!\n");
        return false;
    }

    if(mus_channels1 > 15)
    {
        m_errorString.set("MUS file is invalid: more than 15 primary channels!\n");
        return false;
    }

    mus_instrs.reserve(mus_numInstr);

    for(uint16_t i = 0; i < mus_numInstr; ++i)
    {
        fsize = fr.read(numBuffer, 1, 2);
        if(fsize < 2)
        {
            m_errorString.set("Unexpected end of file at instruments numbers list!\n");
            return false;
        }

        mus_instrs.push_back(readLEint16(numBuffer, 2));
    }

    fr.seek(mus_offSong, FileAndMemReader::SET);

    buildSmfSetupReset(1);

    m_invDeltaTicks = fraction<uint64_t>(1, 1000000l * static_cast<uint64_t>(0x101));
    m_tempo         = m_invDeltaTicks * fraction<uint64_t>(0x101) * 2; // MUS has the fixed tempo

    for(int i = 0; i < 16; ++i)
    {
        channel_map[i] = -1;
        channel_volume[i] = 0x40;
    }

    channel_map[15] = 9; // 15'th channel is mapped to 9'th

    MidiTrackRow evtPos;
    MidiEvent event;
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
    event.data_loc[0] = 0x1B;
    event.data_loc[1] = 0x8A;
    event.data_loc[2] = 0x06;
    addEventToBank(evtPos, event);
    TempoEvent tempoEvent = {readBEint(event.data_loc, event.data_loc_size), abs_position};
    temposList.push_back(tempoEvent);

    // Begin percussion channel with volume 100
    std::memset(&event, 0, sizeof(event));
    event.isValid = 1;
    event.type = MidiEvent::T_CTRLCHANGE;
    event.channel = 9;
    event.data_loc_size = 2;
    event.data_loc[0] = 7;
    event.data_loc[1] = 100;
    addEventToBank(evtPos, event);

    delay = 0;
    channel_cur = 0;

    while(song_read < mus_lenSong)
    {
        uint8_t mus_event, mus_channel, bytes[2];
        std::memset(&event, 0, sizeof(event));
        event.isValid = 1;

        fsize = fr.read(&mus_event, 1, 1);
        if(fsize < 1)
        {
            m_errorString.set("Failed to read MUS data: Failed to read event type!\n");
            return false;
        }

        ++song_read;
        mus_channel = (mus_event & 15);
        evtPos.delay = delay;

        // Insert volume for every used channel
        if(channel_map[mus_channel] < 0)
        {
            event.type = MidiEvent::T_CTRLCHANGE;
            event.data_loc_size = 2;
            event.data_loc[0] = 7;
            event.data_loc[1] = 100;
            addEventToBank(evtPos, event);
            std::memset(&event, 0, sizeof(event));
            event.isValid = 1;

            channel_map[mus_channel] = channel_cur++;
            if(channel_cur == 9)
                ++channel_cur;
        }

        event.channel = channel_map[mus_channel];

        switch((mus_event >> 4) & 0x07)
        {
        case 0: // Note off
            fsize = fr.read(&bytes, 1, 1);
            if(fsize < 1)
            {
                m_errorString.set("Failed to read MUS data: Can't read Note OFF event data!\n");
                return false;
            }

            ++song_read;
            event.type = MidiEvent::T_NOTEOFF;
            event.data_loc_size = 2;
            event.data_loc[0] = bytes[0] & 0x7F;
            event.data_loc[1] = 0;
            addEventToBank(evtPos, event);
            break;

        case 1: // Note on
            fsize = fr.read(&bytes, 1, 1);
            if(fsize < 1)
            {
                m_errorString.set("Failed to read MUS data: Can't read Note ON event data!\n");
                return false;
            }

            ++song_read;
            event.type = MidiEvent::T_NOTEON;
            event.data_loc_size = 2;
            event.data_loc[0] = bytes[0] & 0x7F;

            if((bytes[0] & 0x80) != 0)
            {
                fsize = fr.read(&bytes, 1, 1);
                if(fsize < 1)
                {
                    m_errorString.set("Failed to read MUS data: Can't read Note ON's velocity data!\n");
                    return false;
                }

                ++song_read;
                channel_volume[event.channel] = bytes[0];
            }

            event.data_loc[1] = channel_volume[event.channel];
            addEventToBank(evtPos, event);
            break;

        case 2: // Pitch bend
            fsize = fr.read(&bytes, 1, 1);
            if(fsize < 1)
            {
                m_errorString.set("Failed to read MUS data: Can't read Pitch Bend event data!\n");
                return false;
            }

            ++song_read;
            event.type = MidiEvent::T_WHEEL;
            event.data_loc_size = 2;
            event.data_loc[0] = (bytes[0] & 1) >> 6;
            event.data_loc[1] = (bytes[0] >> 1);
            addEventToBank(evtPos, event);
            break;

        case 3: // System event
            fsize = fr.read(&bytes, 1, 1);
            if(fsize < 1)
            {
                m_errorString.set("Failed to read MUS data: Can't read System Event data!\n");
                return false;
            }

            ++song_read;
            event.type = MidiEvent::T_CTRLCHANGE;
            event.data_loc_size = 2;

            if((bytes[0] & 0x7F) >= 15)
                break; // Skip invalid controller

            event.data_loc[0] = controller_map[bytes[0] & 0x7F];
            addEventToBank(evtPos, event);
            break;

        case 4: // Controller
            fsize = fr.read(&bytes, 1, 2);
            if(fsize < 1)
            {
                m_errorString.set("Failed to read MUS data: Can't read System Event data!\n");
                return false;
            }

            song_read += 2;
            event.type = MidiEvent::T_CTRLCHANGE;
            event.data_loc_size = 2;

            if((bytes[0] & 0x7F) >= 15)
                break; // Skip invalid controller

            if((bytes[0] & 0x7F) == 0)
            {
                size_t j = 0;
                event.type = MidiEvent::T_PATCHCHANGE;

                for( ; j < mus_instrs.size(); ++j)
                {
                    if((bytes[1] & 0x7F) == mus_instrs[j])
                        break;
                }

                if(mus_numInstr > 0 && j >= mus_numInstr)
                {
                    m_errorString.set("Failed to read MUS data: Instrument number is not presented in the list!\n");
                    return false;
                }

                event.data_loc[0] = bytes[1] & 0x7F;
                event.data_loc[1] = 0;
                event.data_loc_size = 1;
            }
            else
            {
                event.data_loc[0] = controller_map[bytes[0] & 0x7F];
                event.data_loc[1] = bytes[1] & 0x7F;
            }

            addEventToBank(evtPos, event);
            break;
        case 5: // End of measure
            break;

        case 6: // Track finished
            event.type = MidiEvent::T_SPECIAL;
            event.subtype = MidiEvent::ST_ENDTRACK;
            addEventToBank(evtPos, event);
            break;

        case 7: // Unused event;
            break;
        }

        delay = 0;

        if((mus_event & 0x80) != 0)
        {
            do
            {
                fsize = fr.read(&bytes, 1, 1);
                if(fsize < 1)
                {
                    m_errorString.set("Failed to read MUS data: Can't read one of delay bytes!\n");
                    return false;
                }

                ++song_read;
                delay = (delay * 128) + (bytes[0] & 0x7F);

            } while((bytes[0] & 0x80) != 0);
        }

        if(delay > 0 || event.subtype == MidiEvent::ST_ENDTRACK || song_read >= mus_lenSong)
        {
            evtPos.delay = delay;
            evtPos.absPos = abs_position;
            abs_position += evtPos.delay;
            m_trackData[0].push_back(evtPos);
            evtPos.clear();
        }
    }

    if(!m_trackData[0].empty())
        m_currentPosition.track[0].pos = m_trackData[0].begin();

    buildTimeLine(temposList);

    m_smfFormat = 0;
    m_loop.stackLevel = -1;

    return true;
}


struct HMITrackDir
{
    size_t start;
    size_t len;
    size_t end;
    size_t offset;
    size_t midichan;
    uint16_t designations[8];
};

bool BW_MidiSequencer::parseHMI(FileAndMemReader &fr)
{
    char readBuf[20];
    size_t fsize, tracksCount = 0, division = 0, track_dir, tracks_offset, file_size,
           file_length, totalGotten, abs_position, track_number;
    uint64_t (*readHmiVarLen)(FileAndMemReader &fr, const size_t end, bool &ok) = NULL;
    bool isHMP = false, ok = false;
    uint8_t byte;
    int status = 0;
    MidiTrackRow evtPos;
    LoopPointParseState loopState;
    MidiEvent event;

    std::vector<TempoEvent> temposList;
    std::vector<HMITrackDir> dir;

    std::memset(&loopState, 0, sizeof(loopState));

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
        isHMP = false;
        readHmiVarLen = readVarLenEx; // Same as SMF!

        fr.seek(0xD4, FileAndMemReader::SET);
        if(!readUInt16LE(division, fr))
        {
            m_errorString.set("HMI/HMP: Failed to read division value!\n");
            return false;
        }

        division <<= 2;

        fr.seek(0xE4, FileAndMemReader::SET);
        if(!readUInt16LE(tracksCount, fr))
        {
            m_errorString.set("HMI/HMP: Failed to read tracks count!\n");
            return false;
        }

        fr.seek(0xE8, FileAndMemReader::SET); // Already here
        if(!readUInt32LE(track_dir, fr))
        {
            m_errorString.set("HMI/HMP: Failed to read track dir pointer!\n");
            return false;
        }

#ifdef DEBUG_HMI_PARSE
        printf("== Division: %lu\n", division);
        fflush(stdout);
#endif

        m_invDeltaTicks = fraction<uint64_t>(1, 1000000l * static_cast<uint64_t>(division));
        m_tempo         = fraction<uint64_t>(1, static_cast<uint64_t>(division));

        dir.resize(tracksCount);
        std::memset(dir.data(), 0, sizeof(HMITrackDir) * tracksCount);

        // Read track sizes
        for(size_t tk = 0; tk < tracksCount; ++tk)
        {
            HMITrackDir &d = dir[tk];

            fr.seek(track_dir + (tk * 4), FileAndMemReader::SET);
            if(!readUInt32LE(d.start, fr))
            {
                m_errorString.set("HMI: Failed to read track start offset!\n");
                return false;
            }

            if(d.start > file_size - 0x99 - 4)
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

            if(tk == tracksCount - 1)
            {
                d.len = file_size - d.start;
                d.end = d.start + d.len;
            }
            else
            {
                fr.seek(track_dir + (tk * 4) + 4, FileAndMemReader::SET);
                if(!readUInt32LE(d.len, fr))
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

            fr.seek(d.start + 0x57, FileAndMemReader::SET);
            if(!readUInt32LE(d.offset, fr))
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

            fr.seek(d.start + 0x99, FileAndMemReader::SET);
            for(size_t j = 0; j < 8; ++j)
            {
                if(!readUInt16LE(d.designations[j], fr))
                {
                    m_errorString.set("HMI: Failed to read track destignation value!\n");
                    return false;
                }
            }
        }
    }
    else if(std::memcmp(readBuf, "HMIMIDIP", 8) == 0)
    {
        isHMP = true;
        readHmiVarLen = readHMPVarLenEx; // Has different format!

        if(readBuf[8] == 0)
            tracks_offset = 0x308;
        else if(std::memcmp(readBuf + 8, "013195", 6) == 0)
            tracks_offset = 0x388;
        else
        {
            m_errorString.set("HMP: Unknown version of the HMIMIDIP!\n");
            return false;
        }

        // File length value
        fr.seek(0x20, FileAndMemReader::SET);
        if(!readUInt32LE(file_length, fr))
        {
            m_errorString.set("HMP: Failed to read file length field value!\n");
            return false;
        }

        // Tracks count value
        fr.seek(0x30, FileAndMemReader::SET);
        if(!readUInt32LE(tracksCount, fr))
        {
            m_errorString.set("HMP: Failed to read tracks count!\n");
            return false;
        }

        // Beats per minute
        fr.seek(0x38, FileAndMemReader::SET);
        if(!readUInt32LE(division, fr))
        {
            m_errorString.set("HMP: Failed to read division value!\n");
            return false;
        }

        m_invDeltaTicks = fraction<uint64_t>(1, 1000000l * static_cast<uint64_t>(division));
        m_tempo         = fraction<uint64_t>(1, static_cast<uint64_t>(division));

        dir.resize(tracksCount);
        std::memset(dir.data(), 0, sizeof(HMITrackDir) * tracksCount);

        for(size_t tk = 0; tk < tracksCount; ++tk)
        {
            HMITrackDir &d = dir[tk];

            fr.seek(tracks_offset, FileAndMemReader::SET);
            d.start = tracks_offset;

            if(d.start > file_size - 12)
            {
                d.len = 0;
                break; // Track is incomplete
            }

            if(!readUInt32LE(track_number, fr))
            {
                m_errorString.set("HMP: Failed to read track number value!\n");
                return false;
            }

            if(track_number != tk)
            {
                m_errorString.set("HMP: Captured track number value is not matching!\n");
                return false;
            }

            if(!readUInt32LE(d.len, fr))
            {
                m_errorString.set("HMP: Failed to read track start offset!\n");
                return false;
            }

            if(!readUInt32LE(d.midichan, fr))
            {
                m_errorString.set("HMP: Failed to read track's MIDI channel value!\n");
                return false;
            }

            tracks_offset += d.len;
            d.end = d.start + d.len;

            if(file_size - d.start < d.len)
                d.len = file_size - d.start;

            if(d.len <= 12)
            {
                d.len = 0;
                continue;
            }

            d.offset = 12;
            d.len -= 12; // Track header size!

            d.designations[0] = 0xA000;
            d.designations[1] = 0xA00A;
            d.designations[2] = 0xA002;
            d.designations[3] = 0;

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


    buildSmfSetupReset(tracksCount);

    m_loopFormat = Loop_HMI;

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
    if(isHMP)
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

#ifdef DEBUG_HMI_PARSE
    printf("==Tempo %g, Div %g=========================\n", m_tempo.value(), m_invDeltaTicks.value());
    fflush(stdout);
#endif

    size_t tk_v = 0;

    for(size_t tk = 0; tk < tracksCount; ++tk)
    {
        HMITrackDir &d = dir[tk];

        if(d.len == 0)
            continue; // This track is broken

        status = 0;
        fr.seek(d.start + d.offset, FileAndMemReader::SET);

#ifdef DEBUG_HMI_PARSE
        printf("==Track %lu=(de-facto %lu)=============================\n", (unsigned long)tk, (unsigned long)tk_v);
        fflush(stdout);
#endif

        // Time delay that follows the first event in the track
        abs_position = readHmiVarLen(fr, d.end, ok);
        if(!ok)
        {
            m_errorString.set("HMI/HMP: Failed to read first event's delay\n");
            return false;
        }

        do
        {
            std::memset(&event, 0, sizeof(event));
            event.isValid = 1;

            if(fr.tell() + 1 > d.end)
            {
                // When track doesn't ends on the middle of event data, it's must be fine
                event.type = MidiEvent::T_SPECIAL;
                event.subtype = MidiEvent::ST_ENDTRACK;
                status = -1;
            }
            else
            {
                if(fr.read(&byte, 1, 1) != 1)
                {
                    m_errorString.set("HMI/HMP: Failed to read first byte of the event\n");
                    return false;
                }

                if(byte == MidiEvent::T_SYSEX || byte == MidiEvent::T_SYSEX2)
                {
                    uint64_t length = readHmiVarLen(fr, d.end, ok);
                    if(!ok || (fr.tell() + length > d.end))
                    {
                        m_errorString.append("HMI/HMP: Can't read SysEx event - Unexpected end of track data.\n");
                        return false;
                    }

#ifdef DEBUG_HMI_PARSE
                    printf("-- SysEx event\n");
                    fflush(stdout);
#endif

                    event.type = MidiEvent::T_SYSEX;
                    insertDataToBankWithByte(event, m_dataBank, byte, fr, length);
                }
                else if(byte == MidiEvent::T_SPECIAL)
                {
                    // Special event FF
                    uint8_t  evtype;
                    uint64_t length;

                    if(fr.read(&evtype, 1, 1) != 1)
                    {
                        m_errorString.append("HMI/HMP: Failed to read event type!\n");
                        event.isValid = 0;
                        return false;
                    }

#ifdef DEBUG_HMI_PARSE
                    printf("-- Special event 0x%02X\n", evtype);
                    fflush(stdout);
#endif

                    length = (evtype != MidiEvent::ST_ENDTRACK) ? readHmiVarLen(fr, d.end, ok) : 0;

                    if(!ok || (fr.tell() + length > d.end))
                    {
                        m_errorString.append("HMI/HMP: Can't read Special event - Unexpected end of track data.\n");
                        return false;
                    }

                    event.type = byte;
                    event.subtype = evtype;

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
                    else if(evtype == MidiEvent::ST_ENDTRACK)
                    {
                        status = -1; // Finalize track
                    }
                    else
                    {
                        m_errorString.append("HMI/HMP: Unsupported meta event sub-type.\n");
                        return false;
                    }
                }
                else if(byte == MidiEvent::T_SYSCOMSPOSPTR)
                {
                    if(fr.tell() + 2 > d.end)
                    {
                        m_errorString.append("HMI/HMP: Can't read System Command Position Pointer event - Unexpected end of track data.\n");
                        return false;
                    }

                    event.type = byte;
                    fr.read(event.data_loc, 1, 2);
                    event.data_loc_size = 2;
                }
                else if(byte == MidiEvent::T_SYSCOMSNGSEL)
                {
                    if(fr.tell() + 1 > d.end)
                    {
                        m_errorString.append("HMI/HMP: Can't read System Command Song Select event - Unexpected end of track data.\n");
                        return false;
                    }

                    event.type = byte;
                    fr.read(event.data_loc, 1, 1);
                    event.data_loc_size = 1;
                }
                else if(byte == 0xFE) // Unknown but valid HMI events to skip
                {
                    event.isValid = 0;
                    uint8_t evtype;
                    uint8_t skipSize;

                    if(fr.read(&evtype, 1, 1) != 1)
                    {
                        m_errorString.append("HMI/HMP: Failed to read event type!\n");
                        return false;
                    }
#ifdef DEBUG_HMI_PARSE
                    printf("-- HMI-specific tricky event 0x%02X\n", evtype);
                    fflush(stdout);
#endif

                    switch(evtype)
                    {
                    case 0x13:
                    case 0x15:
                        fr.seek(6, FileAndMemReader::CUR); // Skip 6 bytes
                        break;

                    case 0x12:
                    case 0x14:
                        fr.seek(2, FileAndMemReader::CUR); // Skip 2 bytes
                        break;

                    case 0x10:
                        fr.seek(2, FileAndMemReader::CUR); // Skip 2 bytes

                        if(fr.read(&skipSize, 1, 1) != 1)
                        {
                            m_errorString.append("HMI/HMP: Failed to read unknown event length!\n");
                            return false;
                        }

                        fr.seek(skipSize + 5, FileAndMemReader::CUR); // Skip bytes of gotten length
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
                    uint8_t midCh, evType;

#ifdef DEBUG_HMI_PARSE
                    printf("Byte=0x%02X (off=%ld = 0x%lX) : ", byte, fr.tell(), fr.tell());
#endif

                    // Any normal event (80..EF)
                    if(byte < 0x80)
                    {
                        byte = static_cast<uint8_t>(status | 0x80);
                        fr.seek(-1, FileAndMemReader::CUR);
                    }

                    midCh = byte & 0x0F;
                    evType = (byte >> 4) & 0x0F;
                    status = byte;
                    event.channel = midCh;
                    event.type = evType;

                    switch(evType)
                    {
                    case MidiEvent::T_NOTEOFF: // 2 byte length
                    case MidiEvent::T_NOTEON:
                    case MidiEvent::T_NOTETOUCH:
                    case MidiEvent::T_CTRLCHANGE:
                    case MidiEvent::T_WHEEL:
                        if(fr.tell() + 2 > d.end)
                        {
                            m_errorString.append("HMI/HMP: Can't read regular 2-byte event - Unexpected end of track data.\n");
                            return false;
                            // event.type = MidiEvent::T_SPECIAL;
                            // event.subtype = MidiEvent::ST_ENDTRACK;
                            // event.data_loc_size = 0;
                            // break;
                        }

                        fr.read(event.data_loc, 1, 2);
                        event.data_loc_size = 2;

#ifdef DEBUG_HMI_PARSE
                        printf("-- Regular 2-byte event 0x%02X, chan=%u\n", evType, midCh);
                        fflush(stdout);
#endif

                        if(!isHMP && evType == MidiEvent::T_NOTEON)
                        {
                            uint64_t duration;
                            duration = readHmiVarLen(fr, d.end, ok);
                            if(!ok)
                            {
                                m_errorString.append("HMI/HMP: Can't read the duration of timed note.\n");
                                return false;
                            }

                            event.type = MidiEvent::T_NOTEON_DURATED;
                            if(duration > 0xFFFFFF)
                                duration = 0xFFFFFF; // Fit to 3 bytes

                            event.data_loc_size = 5;
                            event.data_loc[2] = ((duration << 16) & 0xFF0000);
                            event.data_loc[3] = ((duration << 8) & 0x00FF00);
                            event.data_loc[4] = (duration & 0x0000FF);
                        }
                        else if((evType == MidiEvent::T_NOTEON) && (event.data_loc[1] == 0))
                        {
                            event.type = MidiEvent::T_NOTEOFF; // Note ON with zero velocity is Note OFF!
                        }
                        else if(evType == MidiEvent::T_CTRLCHANGE)
                        {
                            switch(event.data_loc[0])
                            {
                            case 103: // Enable controller restaration when branching and looping
                                event.isValid = 0; // Skip this event for now
                                break;
                            case 104: // Disable controller restaration when branching and looping
                                event.isValid = 0; // Skip this event for now
                                break;

                            case 106: // Lock the channel
                                event.isValid = 0; // Skip this event for now
                                break;
                            case 107: // Set the channel priority
                                event.isValid = 0; // Skip this event for now
                                break;

                            case 108: // Local branch location
                                event.isValid = 0; // Skip this event for now
                                break;
                            case 109: // branch to local branch location
                                event.isValid = 0; // Skip this event for now
                                break;

                            case 110: // Global loop start
                                event.type = MidiEvent::T_SPECIAL;
                                event.subtype = MidiEvent::ST_LOOPSTART;
                                event.data_loc_size = 0;
                                break;
                            case 111: // Global loop end
                                event.type = MidiEvent::T_SPECIAL;
                                event.subtype = MidiEvent::ST_LOOPEND;
                                event.data_loc_size = 0;
                                break;

                            case 113: // Global branch location
                                event.isValid = 0; // Skip this event for now
                                break;
                            case 114: // branch to global branch location
                                event.isValid = 0; // Skip this event for now
                                break;

                            case 116: // Local loop start
                                event.type = MidiEvent::T_SPECIAL;
                                event.subtype = MidiEvent::ST_LOOPSTACK_BEGIN;
                                event.data_loc[0] = event.data_loc[1];
                                event.data_loc_size = 1;
                                break;
                            case 117: // Local loop end
                                event.type = MidiEvent::T_SPECIAL;
                                event.subtype = MidiEvent::ST_LOOPSTACK_END;
                                event.data_loc_size = 0;
                                break;

                            case 119:  // Callback Trigger
                                event.type = MidiEvent::T_SPECIAL;
                                event.subtype = MidiEvent::ST_CALLBACK_TRIGGER;
                                event.data_loc[0] = event.data_loc[1];
                                event.data_loc_size = 1;
                                break;
                            }
                        }

                        break;
                    case MidiEvent::T_PATCHCHANGE: // 1 byte length
                    case MidiEvent::T_CHANAFTTOUCH:
                        if(fr.tell() + 1 > d.end)
                        {
                            m_errorString.append("HMI/HMP: Can't read regular 1-byte event - Unexpected end of track data.\n");
                            return false;
                        }

#ifdef DEBUG_HMI_PARSE
                        printf("-- Regular 1-byte event 0x%02X, chan=%u\n", evType, midCh);
                        fflush(stdout);
#endif

                        fr.read(event.data_loc, 1, 1);
                        event.data_loc_size = 1;
                        break;

                    default:
                        m_errorString.append("HMI/HMP: Unsupported normal event type.\n");
                        return false;
                    }
                }

                if(event.isValid)
                    addEventToBank(evtPos, event);

                if(event.type == MidiEvent::T_SPECIAL)
                {
                    if(event.subtype == MidiEvent::ST_TEMPOCHANGE)
                    {
                        TempoEvent t = {readBEint(event.data_loc, event.data_loc_size), abs_position};
                        temposList.push_back(t);
                    }
                    else
                        analyseLoopEvent(loopState, event, abs_position);
                }

                if(event.subtype != MidiEvent::ST_ENDTRACK)
                {
                    evtPos.delay = readHmiVarLen(fr, d.end, ok);
                    if(!ok)
                    {
                        /* End of track has been reached! However, there is no EOT event presented */
                        event.type = MidiEvent::T_SPECIAL;
                        event.subtype = MidiEvent::ST_ENDTRACK;
                        event.isValid = 1;
                    }
                }

                if((evtPos.delay > 0) || (event.subtype == MidiEvent::ST_ENDTRACK))
                {
#ifdef DEBUG_HMI_PARSE
                    printf("- Delay %lu, Position %lu, stored events: %lu, time: %g seconds\n",
                            (unsigned long)evtPos.delay, abs_position, evtPos.events_end - evtPos.events_begin,
                            (abs_position * m_tempo).value());
                    fflush(stdout);
#endif

                    evtPos.absPos = abs_position;
                    abs_position += evtPos.delay;
                    m_trackData[tk_v].push_back(evtPos);
                    evtPos.clear();
                    loopState.gotLoopEventsInThisRow = false;
                }

                if(status < 0 && evtPos.events_begin != evtPos.events_end) // Last row in the track
                {
                    evtPos.delay = 0;
                    evtPos.absPos = abs_position;
                    m_trackData[tk_v].push_back(evtPos);
                    evtPos.clear();
                    loopState.gotLoopEventsInThisRow = false;
                }
            }
        } while((fr.tell() <= d.end) && (event.subtype != MidiEvent::ST_ENDTRACK));

#ifdef DEBUG_HMI_PARSE
        printf("==Track %lu==END=========================\n", (unsigned long)tk_v);
        fflush(stdout);
#endif

        if(loopState.ticksSongLength < abs_position)
            loopState.ticksSongLength = abs_position;

        // Set the chain of events begin
        if(m_trackData[tk_v].size() > 0)
            m_currentPosition.track[tk_v].pos = m_trackData[tk_v].begin();

        ++tk_v;
    }

    // Shrink tracks store if real number of tracks is smaller
    if(m_trackData.size() != tk_v)
    {
        m_trackData.resize(tk_v);
        m_trackDisable.resize(tk_v);
        m_trackDuratedNotes.resize(tk_v);
    }

    installLoop(loopState);
    buildTimeLine(temposList, loopState.loopStartTicks, loopState.loopEndTicks);

    return true;
}

#ifndef BWMIDI_DISABLE_XMI_SUPPORT
bool BW_MidiSequencer::parseXMI(FileAndMemReader &fr)
{
    const size_t headerSize = 14;
    char headerBuf[headerSize] = "";
    size_t fsize = 0;
//    BufferGuard<uint8_t> cvt_buf;
    std::vector<std::vector<uint8_t > > song_buf;
    bool ret;

    (void)Convert_xmi2midi; /* Shut up the warning */

    fsize = fr.read(headerBuf, 1, headerSize);
    if(fsize < headerSize)
    {
        m_errorString.set("Unexpected end of file at header!\n");
        return false;
    }

    if(std::memcmp(headerBuf, "FORM", 4) != 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Invalid format, FORM signature is not found!\n");
        return false;
    }

    if(std::memcmp(headerBuf + 8, "XDIR", 4) != 0)
    {
        m_errorString.set(fr.fileName().c_str());
        m_errorString.append(": Invalid format\n");
        return false;
    }

    size_t mus_len = fr.fileSize();
    fr.seek(0, FileAndMemReader::SET);

    uint8_t *mus = (uint8_t*)std::malloc(mus_len + 20);
    if(!mus)
    {
        m_errorString.set("Out of memory!");
        return false;
    }

    std::memset(mus, 0, mus_len + 20);

    fsize = fr.read(mus, 1, mus_len);
    if(fsize < mus_len)
    {
        m_errorString.set("Failed to read XMI file data!\n");
        return false;
    }

    // Close source stream
    fr.close();

//    uint8_t *mid = NULL;
//    uint32_t mid_len = 0;
    int m2mret = Convert_xmi2midi_multi(mus, static_cast<uint32_t>(mus_len + 20),
                                        song_buf, XMIDI_CONVERT_NOCONVERSION);
    if(mus)
        free(mus);

    if(m2mret < 0)
    {
        song_buf.clear();
        m_errorString.set("Invalid XMI data format!");
        return false;
    }

    if(m_loadTrackNumber >= (int)song_buf.size())
        m_loadTrackNumber = song_buf.size() - 1;

    for(size_t i = 0; i < song_buf.size(); ++i)
    {
        m_rawSongsData.push_back(song_buf[i]);
    }

    song_buf.clear();

    // cvt_buf.set(mid);
    // Open converted MIDI file
    fr.openData(m_rawSongsData[m_loadTrackNumber].data(),
                m_rawSongsData[m_loadTrackNumber].size());
    // Set format as XMIDI
    m_format = Format_XMIDI;

    ret = parseSMF(fr);

    return ret;
}
#endif
