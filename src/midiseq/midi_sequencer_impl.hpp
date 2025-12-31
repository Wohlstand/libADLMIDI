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

#include "midi_sequencer.hpp"
#include <stdio.h>
#include <cstring>
#include <cerrno>
#include <cstdarg>
#include <assert.h>

// IWYU pragma: begin_exports
#include "impl/platform_impl.hpp"

#include "impl/common.hpp"
#include "impl/tempo_fraction.hpp"

#include "impl/err_string_impl.hpp"
#include "impl/durated_note_impl.hpp"
#include "impl/loop_impl.hpp"
#include "impl/databank_impl.hpp"

#include "impl/miditrack_impl.hpp"
#include "impl/mididata_impl.hpp"

#include "impl/process_impl.hpp"

#include "impl/io_impl.hpp"
#include "impl/load_music_impl.hpp"
#ifdef BWMIDI_ENABLE_DEBUG_SONG_DUMP
#include "impl/debug_songdump.hpp"
#endif

// Generic formats
#include "impl/read_smf_impl.hpp"
#include "impl/read_gmf_impl.hpp"
#include "impl/read_mus_impl.hpp"
#include "impl/read_hmi_impl.hpp"
#include "impl/read_xmi_impl.hpp"

#ifdef BWMIDI_ENABLE_OPL_MUSIC_SUPPORT
// OPL2/OPL3 chip specific formats
#   include "impl/read_imf_impl.hpp"
#   include "impl/read_klm_impl.hpp"
#   include "impl/read_rsxx_impl.hpp"
#   include "impl/read_cmf_impl.hpp"
#endif
// IWYU pragma: end_exports


/**********************************************************************************
 *                               SequencerTime                                    *
 **********************************************************************************/

void BW_MidiSequencer::SequencerTime::init()
{
    sampleRate = 44100;
    frameSize = 2;
    reset();
}

void BW_MidiSequencer::SequencerTime::reset()
{
    timeRest = 0.0;
    minDelay = 1.0 / static_cast<double>(sampleRate);
    delay = 0.0;
}


/**********************************************************************************
 *                             BW_MidiSequencer                                   *
 **********************************************************************************/


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
    m_deviceMask(Device_ANY),
    m_deviceMaskAvailable(Device_ANY),
    m_trackSolo(~static_cast<size_t>(0)),
    m_tempoMultiplier(1.0)
{
    m_loop.reset();
    m_loop.invalidLoop = false;

    m_time.init();

    m_tempo.nom = 0;
    m_tempo.denom = 1;
    m_invDeltaTicks.nom = 0;
    m_invDeltaTicks.denom = 1;
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

    m_trackState[track].disabled = !enable;
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

void BW_MidiSequencer::setDeviceMask(uint32_t devMask)
{
    m_deviceMask = devMask;
}

static void devmask2string(char *masks_list, size_t max_length, uint32_t mask)
{
    size_t masks_list_len = 0;
    size_t line_len = 0;

    if(mask == BW_MidiSequencer::Device_ANY)
    {
        snprintf(masks_list, max_length, "<ANY>");
        return;
    }

    static const char *const masks[] =
    {
        "General MIDI",
        "OPL2",
        "OPL3",
        "Roland MT-32",
        "AWE32",
        "WaveBlaster",
        "ProAudio Spectrum",
        "SoundMan16",
        "Digital MIDI",
        "Ensoniq SoundScape",
        "WaveTable",
        "Gravis Ultrasound",
        "PC Speaker",
        "Callback",
        "Sound Master II",
        NULL
    };

    masks_list[0] = 0;

    for(size_t off = 0; masks[off]; ++off)
    {
        if(((mask >> off) & 0x01) != 0)
        {
            int out_len, new_len = strlen(masks[off]) + (line_len > 0 ? 2 : 4);

            if(line_len + new_len >= 79)
            {
                masks_list_len += snprintf(masks_list + masks_list_len, max_length - masks_list_len, ",\n");
                line_len = 0;
            }

            out_len = snprintf(masks_list + masks_list_len, max_length - masks_list_len, line_len > 0 ? ", %s" : "    %s", masks[off]);
            line_len += out_len;
            masks_list_len += out_len;
        }
    }
}

void BW_MidiSequencer::debugPrintDevices()
{
    if(m_deviceMaskAvailable == Device_ANY || !m_interface->onDebugMessage)
    {
        if(m_interface->onDebugMessage)
            m_interface->onDebugMessage(m_interface->onDebugMessage_userData, "Available device names to filter tracks: <ANY>");
        return;
    }

    const size_t masks_list_max = 200;
    char masks_list[masks_list_max] = "";

    devmask2string(masks_list, masks_list_max, m_deviceMaskAvailable);
    m_interface->onDebugMessage(m_interface->onDebugMessage_userData, "Available device names to filter tracks:\n%s", masks_list);

    devmask2string(masks_list, masks_list_max, m_deviceMask);
    m_interface->onDebugMessage(m_interface->onDebugMessage_userData, "Requested filter by devices:\n%s", masks_list);
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
