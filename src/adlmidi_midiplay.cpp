/*
 * libADLMIDI is a free MIDI to WAV conversion library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2018 Vitaly Novichkov <admin@wohlnet.ru>
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

#include "adlmidi_private.hpp"

// Mapping from MIDI volume level to OPL level value.

static const uint8_t DMX_volume_mapping_table[128] =
{
    0,  1,  3,  5,  6,  8,  10, 11,
    13, 14, 16, 17, 19, 20, 22, 23,
    25, 26, 27, 29, 30, 32, 33, 34,
    36, 37, 39, 41, 43, 45, 47, 49,
    50, 52, 54, 55, 57, 59, 60, 61,
    63, 64, 66, 67, 68, 69, 71, 72,
    73, 74, 75, 76, 77, 79, 80, 81,
    82, 83, 84, 84, 85, 86, 87, 88,
    89, 90, 91, 92, 92, 93, 94, 95,
    96, 96, 97, 98, 99, 99, 100, 101,
    101, 102, 103, 103, 104, 105, 105, 106,
    107, 107, 108, 109, 109, 110, 110, 111,
    112, 112, 113, 113, 114, 114, 115, 115,
    116, 117, 117, 118, 118, 119, 119, 120,
    120, 121, 121, 122, 122, 123, 123, 123,
    124, 124, 125, 125, 126, 126, 127, 127,
};

static const uint8_t W9X_volume_mapping_table[32] =
{
    63, 63, 40, 36, 32, 28, 23, 21,
    19, 17, 15, 14, 13, 12, 11, 10,
    9,  8,  7,  6,  5,  5,  4,  4,
    3,  3,  2,  2,  1,  1,  0,  0
};


//static const char MIDIsymbols[256+1] =
//"PPPPPPhcckmvmxbd"  // Ins  0-15
//"oooooahoGGGGGGGG"  // Ins 16-31
//"BBBBBBBBVVVVVHHM"  // Ins 32-47
//"SSSSOOOcTTTTTTTT"  // Ins 48-63
//"XXXXTTTFFFFFFFFF"  // Ins 64-79
//"LLLLLLLLpppppppp"  // Ins 80-95
//"XXXXXXXXGGGGGTSS"  // Ins 96-111
//"bbbbMMMcGXXXXXXX"  // Ins 112-127
//"????????????????"  // Prc 0-15
//"????????????????"  // Prc 16-31
//"???DDshMhhhCCCbM"  // Prc 32-47
//"CBDMMDDDMMDDDDDD"  // Prc 48-63
//"DDDDDDDDDDDDDDDD"  // Prc 64-79
//"DD??????????????"  // Prc 80-95
//"????????????????"  // Prc 96-111
//"????????????????"; // Prc 112-127

static const uint8_t PercussionMap[256] =
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"//GM
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // 3 = bass drum
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // 4 = snare
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // 5 = tom
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // 6 = cymbal
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // 7 = hihat
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"//GP0
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"//GP16
    //2 3 4 5 6 7 8 940 1 2 3 4 5 6 7
    "\0\0\0\3\3\0\0\7\0\5\7\5\0\5\7\5"//GP32
    //8 950 1 2 3 4 5 6 7 8 960 1 2 3
    "\5\6\5\0\6\0\5\6\0\6\0\6\5\5\5\5"//GP48
    //4 5 6 7 8 970 1 2 3 4 5 6 7 8 9
    "\5\0\0\0\0\0\7\0\0\0\0\0\0\0\0\0"//GP64
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

inline bool isXgPercChannel(uint8_t msb, uint8_t lsb)
{
    return (msb == 0x7E || msb == 0x7F) && (lsb == 0);
}

void MIDIplay::AdlChannel::AddAge(int64_t ms)
{
    const int64_t neg = static_cast<int64_t>(-0x1FFFFFFFl);
    if(users_empty())
        koff_time_until_neglible = std::max(int64_t(koff_time_until_neglible - ms), neg);
    else
    {
        koff_time_until_neglible = 0;
        for(LocationData *i = users_first; i; i = i->next)
        {
            if(!i->fixed_sustain)
                i->kon_time_until_neglible = std::max(i->kon_time_until_neglible - ms, neg);
            i->vibdelay += ms;
        }
    }
}

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER

MIDIplay::MidiEvent::MidiEvent() :
    type(T_UNKNOWN),
    subtype(T_UNKNOWN),
    channel(0),
    isValid(1),
    absPosition(0)
{}


MIDIplay::MidiTrackRow::MidiTrackRow() :
    time(0.0),
    delay(0),
    absPos(0),
    timeDelay(0.0)
{}

void MIDIplay::MidiTrackRow::reset()
{
    time = 0.0;
    delay = 0;
    absPos = 0;
    timeDelay = 0.0;
    events.clear();
}

void MIDIplay::MidiTrackRow::sortEvents(bool *noteStates)
{
    typedef std::vector<MidiEvent> EvtArr;
    EvtArr metas;
    EvtArr noteOffs;
    EvtArr controllers;
    EvtArr anyOther;

    metas.reserve(events.size());
    noteOffs.reserve(events.size());
    controllers.reserve(events.size());
    anyOther.reserve(events.size());

    for(size_t i = 0; i < events.size(); i++)
    {
        if(events[i].type == MidiEvent::T_NOTEOFF)
            noteOffs.push_back(events[i]);
        else if((events[i].type == MidiEvent::T_CTRLCHANGE)
                || (events[i].type == MidiEvent::T_PATCHCHANGE)
                || (events[i].type == MidiEvent::T_WHEEL)
                || (events[i].type == MidiEvent::T_CHANAFTTOUCH))
        {
            controllers.push_back(events[i]);
        }
        else if((events[i].type == MidiEvent::T_SPECIAL) && (events[i].subtype == MidiEvent::ST_MARKER))
            metas.push_back(events[i]);
        else
            anyOther.push_back(events[i]);
    }

    /*
     * If Note-Off and it's Note-On is on the same row - move this damned note off down!
     */
    if(noteStates)
    {
        std::set<size_t> markAsOn;
        for(size_t i = 0; i < anyOther.size(); i++)
        {
            const MidiEvent e = anyOther[i];
            if(e.type == MidiEvent::T_NOTEON)
            {
                const size_t note_i = (e.channel * 255) + (e.data[0] & 0x7F);
                //Check, was previously note is on or off
                bool wasOn = noteStates[note_i];
                markAsOn.insert(note_i);
                // Detect zero-length notes are following previously pressed note
                int noteOffsOnSameNote = 0;
                for(EvtArr::iterator j = noteOffs.begin(); j != noteOffs.end();)
                {
                    //If note was off, and note-off on same row with note-on - move it down!
                    if(
                        ((*j).channel == e.channel) &&
                        ((*j).data[0] == e.data[0])
                    )
                    {
                        //If note is already off OR more than one note-off on same row and same note
                        if(!wasOn || (noteOffsOnSameNote != 0))
                        {
                            anyOther.push_back(*j);
                            j = noteOffs.erase(j);
                            markAsOn.erase(note_i);
                            continue;
                        }
                        else
                        {
                            //When same row has many note-offs on same row
                            //that means a zero-length note follows previous note
                            //it must be shuted down
                            noteOffsOnSameNote++;
                        }
                    }
                    j++;
                }
            }
        }

        //Mark other notes as released
        for(EvtArr::iterator j = noteOffs.begin(); j != noteOffs.end(); j++)
        {
            size_t note_i = (j->channel * 255) + (j->data[0] & 0x7F);
            noteStates[note_i] = false;
        }

        for(std::set<size_t>::iterator j = markAsOn.begin(); j != markAsOn.end(); j++)
            noteStates[*j] = true;
    }
    /***********************************************************************************/

    events.clear();
    events.insert(events.end(), noteOffs.begin(), noteOffs.end());
    events.insert(events.end(), metas.begin(), metas.end());
    events.insert(events.end(), controllers.begin(), controllers.end());
    events.insert(events.end(), anyOther.begin(), anyOther.end());
}
#endif //ADLMIDI_DISABLE_MIDI_SEQUENCER

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
bool MIDIplay::buildTrackData()
{
    fullSongTimeLength = 0.0;
    loopStartTime = -1.0;
    loopEndTime = -1.0;
    musTitle.clear();
    musCopyright.clear();
    musTrackTitles.clear();
    musMarkers.clear();
    caugh_missing_instruments.clear();
    caugh_missing_banks_melodic.clear();
    caugh_missing_banks_percussion.clear();
    trackDataNew.clear();
    const size_t    trackCount = TrackData.size();
    trackDataNew.resize(trackCount, MidiTrackQueue());

    invalidLoop = false;
    bool gotLoopStart = false, gotLoopEnd = false, gotLoopEventInThisRow = false;
    //! Tick position of loop start tag
    uint64_t loopStartTicks = 0;
    //! Tick position of loop end tag
    uint64_t loopEndTicks = 0;
    //! Full length of song in ticks
    uint64_t ticksSongLength = 0;
    //! Cache for error message strign
    char error[150];

    CurrentPositionNew.track.clear();
    CurrentPositionNew.track.resize(trackCount);

    //! Caches note on/off states.
    bool noteStates[16 * 255];
    /* This is required to carefully detect zero-length notes           *
     * and avoid a move of "note-off" event over "note-on" while sort.  *
     * Otherwise, after sort those notes will play infinite sound       */

    //Tempo change events
    std::vector<MidiEvent> tempos;

    /*
     * TODO: Make this be safer for memory in case of broken input data
     * which may cause going away of available track data (and then give a crash!)
     *
     * POST: Check this more carefully for possible vulnuabilities are can crash this
     */
    for(size_t tk = 0; tk < trackCount; ++tk)
    {
        uint64_t abs_position = 0;
        int status = 0;
        MidiEvent event;
        bool ok = false;
        uint8_t *end      = TrackData[tk].data() + TrackData[tk].size();
        uint8_t *trackPtr = TrackData[tk].data();
        std::memset(noteStates, 0, sizeof(noteStates));

        //Time delay that follows the first event in the track
        {
            MidiTrackRow evtPos;
            if(opl.m_musicMode == OPL3::MODE_RSXX)
                ok = true;
            else
                evtPos.delay = ReadVarLenEx(&trackPtr, end, ok);
            if(!ok)
            {
                int len = snprintf(error, 150, "buildTrackData: Can't read variable-length value at begin of track %d.\n", (int)tk);
                if((len > 0) && (len < 150))
                    errorString += std::string(error, (size_t)len);
                return false;
            }

            //HACK: Begin every track with "Reset all controllers" event to avoid controllers state break came from end of song
            for(uint8_t chan = 0; chan < 16; chan++)
            {
                MidiEvent event;
                event.type = MidiEvent::T_CTRLCHANGE;
                event.channel = chan;
                event.data.push_back(121);
                event.data.push_back(0);
                evtPos.events.push_back(event);
            }

            evtPos.absPos = abs_position;
            abs_position += evtPos.delay;
            trackDataNew[tk].push_back(evtPos);
        }

        MidiTrackRow evtPos;
        do
        {
            event = parseEvent(&trackPtr, end, status);
            if(!event.isValid)
            {
                int len = snprintf(error, 150, "buildTrackData: Fail to parse event in the track %d.\n", (int)tk);
                if((len > 0) && (len < 150))
                    errorString += std::string(error, (size_t)len);
                return false;
            }

            evtPos.events.push_back(event);
            if(event.type == MidiEvent::T_SPECIAL)
            {
                if(event.subtype == MidiEvent::ST_TEMPOCHANGE)
                {
                    event.absPosition = abs_position;
                    tempos.push_back(event);
                }
                else if(!invalidLoop && (event.subtype == MidiEvent::ST_LOOPSTART))
                {
                    /*
                     * loopStart is invalid when:
                     * - starts together with loopEnd
                     * - appears more than one time in same MIDI file
                     */
                    if(gotLoopStart || gotLoopEventInThisRow)
                        invalidLoop = true;
                    else
                    {
                        gotLoopStart = true;
                        loopStartTicks = abs_position;
                    }
                    //In this row we got loop event, register this!
                    gotLoopEventInThisRow = true;
                }
                else if(!invalidLoop && (event.subtype == MidiEvent::ST_LOOPEND))
                {
                    /*
                     * loopEnd is invalid when:
                     * - starts before loopStart
                     * - starts together with loopStart
                     * - appars more than one time in same MIDI file
                     */
                    if(gotLoopEnd || gotLoopEventInThisRow)
                        invalidLoop = true;
                    else
                    {
                        gotLoopEnd = true;
                        loopEndTicks = abs_position;
                    }
                    //In this row we got loop event, register this!
                    gotLoopEventInThisRow = true;
                }
            }

            if(event.subtype != MidiEvent::ST_ENDTRACK)//Don't try to read delta after EndOfTrack event!
            {
                evtPos.delay = ReadVarLenEx(&trackPtr, end, ok);
                if(!ok)
                {
                    /* End of track has been reached! However, there is no EOT event presented */
                    event.type = MidiEvent::T_SPECIAL;
                    event.subtype = MidiEvent::ST_ENDTRACK;
                }
            }

            if((evtPos.delay > 0) || (event.subtype == MidiEvent::ST_ENDTRACK))
            {
                evtPos.absPos = abs_position;
                abs_position += evtPos.delay;
                evtPos.sortEvents(noteStates);
                trackDataNew[tk].push_back(evtPos);
                evtPos.reset();
                gotLoopEventInThisRow = false;
            }
        }
        while((trackPtr <= end) && (event.subtype != MidiEvent::ST_ENDTRACK));

        if(ticksSongLength < abs_position)
            ticksSongLength = abs_position;
        //Set the chain of events begin
        if(trackDataNew[tk].size() > 0)
            CurrentPositionNew.track[tk].pos = trackDataNew[tk].begin();
    }

    if(gotLoopStart && !gotLoopEnd)
    {
        gotLoopEnd = true;
        loopEndTicks = ticksSongLength;
    }

    //loopStart must be located before loopEnd!
    if(loopStartTicks >= loopEndTicks)
        invalidLoop = true;

    /********************************************************************************/
    //Calculate time basing on collected tempo events
    /********************************************************************************/
    for(size_t tk = 0; tk < trackCount; ++tk)
    {
        fraction<uint64_t> currentTempo = Tempo;
        double  time = 0.0;
        uint64_t abs_position = 0;
        size_t tempo_change_index = 0;
        MidiTrackQueue &track = trackDataNew[tk];
        if(track.empty())
            continue;//Empty track is useless!

#ifdef DEBUG_TIME_CALCULATION
        std::fprintf(stdout, "\n============Track %" PRIuPTR "=============\n", tk);
        std::fflush(stdout);
#endif

        MidiTrackRow *posPrev = &(*(track.begin()));//First element
        for(MidiTrackQueue::iterator it = track.begin(); it != track.end(); it++)
        {
#ifdef DEBUG_TIME_CALCULATION
            bool tempoChanged = false;
#endif
            MidiTrackRow &pos = *it;
            if((posPrev != &pos) &&  //Skip first event
               (!tempos.empty()) && //Only when in-track tempo events are available
               (tempo_change_index < tempos.size())
              )
            {
                // If tempo event is going between of current and previous event
                if(tempos[tempo_change_index].absPosition <= pos.absPos)
                {
                    //Stop points: begin point and tempo change points are before end point
                    std::vector<TempoChangePoint> points;
                    fraction<uint64_t> t;
                    TempoChangePoint firstPoint = {posPrev->absPos, currentTempo};
                    points.push_back(firstPoint);

                    //Collect tempo change points between previous and current events
                    do
                    {
                        TempoChangePoint tempoMarker;
                        MidiEvent &tempoPoint = tempos[tempo_change_index];
                        tempoMarker.absPos = tempoPoint.absPosition;
                        tempoMarker.tempo = InvDeltaTicks * fraction<uint64_t>(ReadBEint(tempoPoint.data.data(), tempoPoint.data.size()));
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
                        //Delay between points
                        midDelay  = points[j].absPos - points[i].absPos;
                        //Time delay between points
                        t = midDelay * currentTempo;
                        posPrev->timeDelay += t.value();

                        //Apply next tempo
                        currentTempo = points[j].tempo;
#ifdef DEBUG_TIME_CALCULATION
                        tempoChanged = true;
#endif
                    }
                    //Then calculate time between last tempo change point and end point
                    TempoChangePoint tailTempo = points.back();
                    uint64_t postDelay = pos.absPos - tailTempo.absPos;
                    t = postDelay * currentTempo;
                    posPrev->timeDelay += t.value();

                    //Store Common time delay
                    posPrev->time = time;
                    time += posPrev->timeDelay;
                }
            }

            fraction<uint64_t> t = pos.delay * currentTempo;
            pos.timeDelay = t.value();
            pos.time = time;
            time += pos.timeDelay;

            //Capture markers after time value calculation
            for(size_t i = 0; i < pos.events.size(); i++)
            {
                MidiEvent &e = pos.events[i];
                if((e.type == MidiEvent::T_SPECIAL) && (e.subtype == MidiEvent::ST_MARKER))
                {
                    MIDI_MarkerEntry marker;
                    marker.label = std::string((char *)e.data.data(), e.data.size());
                    marker.pos_ticks = pos.absPos;
                    marker.pos_time = pos.time;
                    musMarkers.push_back(marker);
                }
            }

            //Capture loop points time positions
            if(!invalidLoop)
            {
                // Set loop points times
                if(loopStartTicks == pos.absPos)
                    loopStartTime = pos.time;
                else if(loopEndTicks == pos.absPos)
                    loopEndTime = pos.time;
            }

#ifdef DEBUG_TIME_CALCULATION
            std::fprintf(stdout, "= %10" PRId64 " = %10f%s\n", pos.absPos, pos.time, tempoChanged ? " <----TEMPO CHANGED" : "");
            std::fflush(stdout);
#endif

            abs_position += pos.delay;
            posPrev = &pos;
        }

        if(time > fullSongTimeLength)
            fullSongTimeLength = time;
    }

    fullSongTimeLength += postSongWaitDelay;
    //Set begin of the music
    trackBeginPositionNew = CurrentPositionNew;
    //Initial loop position will begin at begin of track until passing of the loop point
    LoopBeginPositionNew  = CurrentPositionNew;

    /********************************************************************************/
    //Resolve "hell of all times" of too short drum notes:
    //move too short percussion note-offs far far away as possible
    /********************************************************************************/
#if 1 //Use this to record WAVEs for comparison before/after implementing of this
    if(opl.m_musicMode == OPL3::MODE_MIDI)//Percussion fix is needed for MIDI only, not for IMF/RSXX or CMF
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
        uint16_t banks[16];

        for(size_t tk = 0; tk < trackCount; ++tk)
        {
            std::memset(drNotes, 0, sizeof(drNotes));
            std::memset(banks, 0, sizeof(banks));
            MidiTrackQueue &track = trackDataNew[tk];
            if(track.empty())
                continue;//Empty track is useless!

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
                            banks[et->channel] = uint16_t(uint16_t(value) << 8) | uint16_t(banks[et->channel] & 0x00FF);
                            break;
                        case 32: // Set bank lsb (XG bank)
                            banks[et->channel] = (banks[et->channel] & 0xFF00) | (uint16_t(value) & 0x00FF);
                            break;
                        }
                        continue;
                    }

                    bool percussion = (et->channel == 9) ||
                                      banks[et->channel] == 0x7E00 || //XG SFX1/SFX2 channel (16128 signed decimal)
                                      banks[et->channel] == 0x7F00;   //XG Percussion channel (16256 signed decimal)
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
                            if(ns.delayTicks < DRUM_NOTE_MIN_TICKS || ns.delay < DRUM_NOTE_MIN_TIME)//If note is too short
                            {
                                //Move it into next event position if that possible
                                for(MidiTrackQueue::iterator itNext = it;
                                    itNext != track.end();
                                    itNext++)
                                {
                                    MidiTrackRow &posN = *itNext;
                                    if(ns.delayTicks > DRUM_NOTE_MIN_TICKS && ns.delay > DRUM_NOTE_MIN_TIME)
                                    {
                                        //Put note-off into begin of next event list
                                        posN.events.insert(posN.events.begin(), pos.events[(size_t)e]);
                                        //Renive this event from a current row
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

    return true;
}
#endif


MIDIplay::MIDIplay(unsigned long sampleRate):
    cmf_percussion_mode(false),
    m_arpeggioCounter(0),
    m_audioTickCounter(0)
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    , fullSongTimeLength(0.0),
    postSongWaitDelay(1.0),
    loopStartTime(-1.0),
    loopEndTime(-1.0),
    tempoMultiplier(1.0),
    atEnd(false),
    loopStart(false),
    loopEnd(false),
    invalidLoop(false)
#endif
{
    devices.clear();

    m_setup.emulator = ADLMIDI_EMU_NUKED;
    m_setup.runAtPcmRate = false;

    m_setup.PCM_RATE   = sampleRate;
    m_setup.mindelay = 1.0 / (double)m_setup.PCM_RATE;
    m_setup.maxdelay = 512.0 / (double)m_setup.PCM_RATE;

    m_setup.AdlBank    = 0;
    m_setup.NumFourOps = 7;
    m_setup.NumCards   = 2;
    m_setup.HighTremoloMode     = -1;
    m_setup.HighVibratoMode     = -1;
    m_setup.AdlPercussionMode   = -1;
    m_setup.LogarithmicVolumes  = false;
    m_setup.VolumeModel = ADLMIDI_VolumeModel_AUTO;
    //m_setup.SkipForward = 0;
    m_setup.loopingIsEnabled = false;
    m_setup.ScaleModulators     = -1;
    m_setup.fullRangeBrightnessCC74 = false;
    m_setup.delay = 0.0;
    m_setup.carry = 0.0;
    m_setup.tick_skip_samples_delay = 0;

    applySetup();
    ChooseDevice("none");
    realTime_ResetState();
}

void MIDIplay::applySetup()
{
    m_setup.tick_skip_samples_delay = 0;

    opl.runAtPcmRate = m_setup.runAtPcmRate;

    if(opl.AdlBank != ~0u)
        opl.dynamic_bank_setup = adlbanksetup[m_setup.AdlBank];

    opl.HighTremoloMode     = m_setup.HighTremoloMode < 0 ?
                              opl.dynamic_bank_setup.deepTremolo :
                              (m_setup.HighTremoloMode != 0);
    opl.HighVibratoMode     = m_setup.HighVibratoMode < 0 ?
                              opl.dynamic_bank_setup.deepVibrato :
                              (m_setup.HighVibratoMode != 0);
    opl.AdlPercussionMode   = m_setup.AdlPercussionMode < 0 ?
                              opl.dynamic_bank_setup.adLibPercussions :
                              (m_setup.AdlPercussionMode != 0);
    opl.ScaleModulators     = m_setup.ScaleModulators < 0 ?
                              opl.dynamic_bank_setup.scaleModulators :
                              (m_setup.ScaleModulators != 0);
    if(m_setup.LogarithmicVolumes)
        opl.ChangeVolumeRangesModel(ADLMIDI_VolumeModel_NativeOPL3);
    opl.m_musicMode = OPL3::MODE_MIDI;
    opl.ChangeVolumeRangesModel(static_cast<ADLMIDI_VolumeModels>(m_setup.VolumeModel));
    if(m_setup.VolumeModel == ADLMIDI_VolumeModel_AUTO)//Use bank default volume model
        opl.m_volumeScale = (OPL3::VolumesScale)opl.dynamic_bank_setup.volumeModel;

    opl.NumCards    = m_setup.NumCards;
    opl.NumFourOps  = m_setup.NumFourOps;
    cmf_percussion_mode = false;

    opl.Reset(m_setup.emulator, m_setup.PCM_RATE, this);
    ch.clear();
    ch.resize(opl.NumChannels);

    // Reset the arpeggio counter
    m_arpeggioCounter = 0;
}

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
uint64_t MIDIplay::ReadVarLen(uint8_t **ptr)
{
    uint64_t result = 0;
    for(;;)
    {
        uint8_t byte = *((*ptr)++);
        result = (result << 7) + (byte & 0x7F);
        if(!(byte & 0x80))
            break;
    }
    return result;
}

uint64_t MIDIplay::ReadVarLenEx(uint8_t **ptr, uint8_t *end, bool &ok)
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

double MIDIplay::Tick(double s, double granularity)
{
    s *= tempoMultiplier;
#ifdef ENABLE_BEGIN_SILENCE_SKIPPING
    if(CurrentPositionNew.began)
#endif
        CurrentPositionNew.wait -= s;
    CurrentPositionNew.absTimePosition += s;

    int antiFreezeCounter = 10000;//Limit 10000 loops to avoid freezing
    while((CurrentPositionNew.wait <= granularity * 0.5) && (antiFreezeCounter > 0))
    {
        //std::fprintf(stderr, "wait = %g...\n", CurrentPosition.wait);
        if(!ProcessEventsNew())
            break;
        if(CurrentPositionNew.wait <= 0.0)
            antiFreezeCounter--;
    }

    if(antiFreezeCounter <= 0)
        CurrentPositionNew.wait += 1.0;/* Add extra 1 second when over 10000 events
                                           with zero delay are been detected */

    for(uint16_t c = 0; c < opl.NumChannels; ++c)
        ch[c].AddAge(static_cast<int64_t>(s * 1000.0));

    UpdateVibrato(s);
    UpdateArpeggio(s);

    if(CurrentPositionNew.wait < 0.0)//Avoid negative delay value!
        return 0.0;

    return CurrentPositionNew.wait;
}
#endif /* ADLMIDI_DISABLE_MIDI_SEQUENCER */

void MIDIplay::TickIteratos(double s)
{
    for(uint16_t c = 0; c < opl.NumChannels; ++c)
        ch[c].AddAge(static_cast<int64_t>(s * 1000.0));
    UpdateVibrato(s);
    UpdateArpeggio(s);
}

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER

void MIDIplay::seek(double seconds)
{
    if(seconds < 0.0)
        return;//Seeking negative position is forbidden! :-P
    const double granularity = m_setup.mindelay,
                 granualityHalf = granularity * 0.5,
                 s = seconds;//m_setup.delay < m_setup.maxdelay ? m_setup.delay : m_setup.maxdelay;

    /* Attempt to go away out of song end must rewind position to begin */
    if(seconds > fullSongTimeLength)
    {
        rewind();
        return;
    }

    bool loopFlagState = m_setup.loopingIsEnabled;
    // Turn loop pooints off because it causes wrong position rememberin on a quick seek
    m_setup.loopingIsEnabled = false;

    /*
     * Seeking search is similar to regular ticking, except of next things:
     * - We don't processsing arpeggio and vibrato
     * - To keep correctness of the state after seek, begin every search from begin
     * - All sustaining notes must be killed
     * - Ignore Note-On events
     */
    rewind();

    /*
     * Set "loop Start" to false to prevent overwrite of loopStart position with
     * seek destinition position
     *
     * TODO: Detect & set loopStart position on load time to don't break loop while seeking
     */
    loopStart   = false;

    while((CurrentPositionNew.absTimePosition < seconds) &&
          (CurrentPositionNew.absTimePosition < fullSongTimeLength))
    {
        CurrentPositionNew.wait -= s;
        CurrentPositionNew.absTimePosition += s;
        int antiFreezeCounter = 10000;//Limit 10000 loops to avoid freezing
        double dstWait = CurrentPositionNew.wait + granualityHalf;
        while((CurrentPositionNew.wait <= granualityHalf)/*&& (antiFreezeCounter > 0)*/)
        {
            //std::fprintf(stderr, "wait = %g...\n", CurrentPosition.wait);
            if(!ProcessEventsNew(true))
                break;
            //Avoid freeze because of no waiting increasing in more than 10000 cycles
            if(CurrentPositionNew.wait <= dstWait)
                antiFreezeCounter--;
            else
            {
                dstWait = CurrentPositionNew.wait + granualityHalf;
                antiFreezeCounter = 10000;
            }
        }
        if(antiFreezeCounter <= 0)
            CurrentPositionNew.wait += 1.0;/* Add extra 1 second when over 10000 events
                                               with zero delay are been detected */
    }

    if(CurrentPositionNew.wait < 0.0)
        CurrentPositionNew.wait = 0.0;

    m_setup.loopingIsEnabled = loopFlagState;
    m_setup.delay = CurrentPositionNew.wait;
    m_setup.carry = 0.0;
}

double MIDIplay::tell()
{
    return CurrentPositionNew.absTimePosition;
}

double MIDIplay::timeLength()
{
    return fullSongTimeLength;
}

double MIDIplay::getLoopStart()
{
    return loopStartTime;
}

double MIDIplay::getLoopEnd()
{
    return loopEndTime;
}

void MIDIplay::rewind()
{
    Panic();
    KillSustainingNotes(-1, -1);
    CurrentPositionNew   = trackBeginPositionNew;
    atEnd            = false;
    loopStart        = true;
    loopEnd          = false;
    //invalidLoop      = false;//No more needed here as this flag is set on load time
}

void MIDIplay::setTempo(double tempo)
{
    tempoMultiplier = tempo;
}
#endif /* ADLMIDI_DISABLE_MIDI_SEQUENCER */

void MIDIplay::realTime_ResetState()
{
    for(size_t ch = 0; ch < Ch.size(); ch++)
    {
        MIDIchannel &chan = Ch[ch];
        chan.resetAllControllers();
        chan.volume = (opl.m_musicMode == OPL3::MODE_RSXX) ? 127 : 100;
        chan.vibpos = 0.0;
        chan.lastlrpn = 0;
        chan.lastmrpn = 0;
        chan.nrpn = false;
        NoteUpdate_All(uint16_t(ch), Upd_All);
        NoteUpdate_All(uint16_t(ch), Upd_Off);
    }
}

bool MIDIplay::realTime_NoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
    if(note >= 128)
        note = 127;

    if((opl.m_musicMode == OPL3::MODE_RSXX) && (velocity != 0))
    {
        // Check if this is just a note after-touch
        MIDIchannel::activenoteiterator i = Ch[channel].activenotes_find(note);
        if(i)
        {
            i->vol = velocity;
            NoteUpdate(channel, i, Upd_Volume);
            return false;
        }
    }

    channel = channel % 16;
    NoteOff(channel, note);
    // On Note on, Keyoff the note first, just in case keyoff
    // was omitted; this fixes Dance of sugar-plum fairy
    // by Microsoft. Now that we've done a Keyoff,
    // check if we still need to do a Keyon.
    // vol=0 and event 8x are both Keyoff-only.
    if(velocity == 0)
        return false;

    MIDIchannel &midiChan = Ch[channel];

    size_t midiins = midiChan.patch;
    bool isPercussion = (channel % 16 == 9);
    bool isXgPercussion = false;

    uint16_t bank = 0;
    if(midiChan.bank_msb || midiChan.bank_lsb)
    {
        bank = (uint16_t(midiChan.bank_msb) * 256) + uint16_t(midiChan.bank_lsb);
        //0x7E00 - XG SFX1/SFX2 channel (16128 signed decimal)
        //0x7F00 - XG Percussion channel (16256 signed decimal)
        if(bank == 0x7E00 || bank == 0x7F00)
        {
            //Let XG SFX1/SFX2 bank will have LSB==1 (128...255 range in WOPN file)
            //Let XG Percussion bank will use (0...127 range in WOPN file)
            bank = (uint16_t)midiins + ((bank == 0x7E00) ? 128 : 0); // MIDI instrument defines the patch
            midiins = note; // Percussion instrument
            isXgPercussion = true;
            isPercussion = false;
        }
    }

    if(isPercussion)
    {
        bank = (uint16_t)midiins; // MIDI instrument defines the patch
        midiins = note; // Percussion instrument
    }
    if(isPercussion || isXgPercussion)
        bank += OPL3::PercussionTag;

    const adlinsdata2 *ains = &OPL3::emptyInstrument;

    //Set bank bank
    const OPL3::Bank *bnk = NULL;
    if((bank & ~(uint16_t)OPL3::PercussionTag) > 0)
    {
        OPL3::BankMap::iterator b = opl.dynamic_banks.find(bank);
        if(b != opl.dynamic_banks.end())
            bnk = &b->second;

        if(bnk)
            ains = &bnk->ins[midiins];
        else if(hooks.onDebugMessage)
        {
            std::set<uint16_t> &missing = (isPercussion || isXgPercussion) ?
                                          caugh_missing_banks_percussion : caugh_missing_banks_melodic;
            const char *text = (isPercussion || isXgPercussion) ?
                               "percussion" : "melodic";
            if(missing.insert(bank).second)
                hooks.onDebugMessage(hooks.onDebugMessage_userData, "[%i] Playing missing %s MIDI bank %i (patch %i)", channel, text, bank, midiins);
        }
    }
    //Or fall back to first bank
    if(ains->flags & adlinsdata::Flag_NoSound)
    {
        OPL3::BankMap::iterator b = opl.dynamic_banks.find(bank & OPL3::PercussionTag);
        if(b != opl.dynamic_banks.end())
            bnk = &b->second;

        if(bnk)
            ains = &bnk->ins[midiins];
    }

    /*
        if(MidCh%16 == 9 || (midiins != 32 && midiins != 46 && midiins != 48 && midiins != 50))
            break; // HACK
        if(midiins == 46) vol = (vol*7)/10;          // HACK
        if(midiins == 48 || midiins == 50) vol /= 4; // HACK
        */
    //if(midiins == 56) vol = vol*6/10; // HACK
    //int meta = banks[opl.AdlBank][midiins];

    int16_t tone = note;

    if(!isPercussion && !isXgPercussion && (bank > 0)) // For non-zero banks
    {
        if(ains->flags & adlinsdata::Flag_NoSound)
        {
            if(hooks.onDebugMessage)
            {
                if(caugh_missing_instruments.insert(static_cast<uint8_t>(midiins)).second)
                    hooks.onDebugMessage(hooks.onDebugMessage_userData, "[%i] Caugh a blank instrument %i (offset %i) in the MIDI bank %u", channel, Ch[channel].patch, midiins, bank);
            }
            bank = 0;
            midiins = midiChan.patch;
        }
    }

    if(ains->tone)
    {
        /*if(ains->tone < 20)
            tone += ains->tone;
        else*/
        if(ains->tone < 128)
            tone = ains->tone;
        else
            tone -= ains->tone - 128;
    }

    //uint16_t i[2] = { ains->adlno1, ains->adlno2 };
    bool pseudo_4op = ains->flags & adlinsdata::Flag_Pseudo4op;
#ifndef __WATCOMC__
    MIDIchannel::NoteInfo::Phys voices[MIDIchannel::NoteInfo::MaxNumPhysChans] =
    {
        {0, ains->adl[0], false},
        {0, ains->adl[1], pseudo_4op}
    };
#else /* Unfortunately, WatCom can't brace-initialize structure that incluses structure fields */
    MIDIchannel::NoteInfo::Phys voices[MIDIchannel::NoteInfo::MaxNumPhysChans];
    voices[0].chip_chan = 0;
    voices[0].ains = ains->adl[0];
    voices[0].pseudo4op = false;
    voices[1].chip_chan = 0;
    voices[1].ains = ains->adl[1];
    voices[1].pseudo4op = pseudo_4op;
#endif /* __WATCOMC__ */

    if((opl.AdlPercussionMode == 1) && PercussionMap[midiins & 0xFF])
        voices[1] = voices[0];//i[1] = i[0];

    if(hooks.onDebugMessage)
    {
        if((ains->flags & adlinsdata::Flag_NoSound) &&
           caugh_missing_instruments.insert(static_cast<uint8_t>(midiins)).second)
            hooks.onDebugMessage(hooks.onDebugMessage_userData, "[%i] Playing missing instrument %i", channel, midiins);
    }

    // Allocate AdLib channel (the physical sound channel for the note)
    int32_t adlchannel[MIDIchannel::NoteInfo::MaxNumPhysChans] = { -1, -1 };

    for(uint32_t ccount = 0; ccount < MIDIchannel::NoteInfo::MaxNumPhysChans; ++ccount)
    {
        if(ccount == 1)
        {
            if(voices[0] == voices[1])
                break; // No secondary channel
            if(adlchannel[0] == -1)
                break; // No secondary if primary failed
        }

        int32_t c = -1;
        int32_t bs = -0x7FFFFFFFl;

        for(size_t a = 0; a < (size_t)opl.NumChannels; ++a)
        {
            if(ccount == 1 && static_cast<int32_t>(a) == adlchannel[0]) continue;
            // ^ Don't use the same channel for primary&secondary

            if(voices[0].ains == voices[1].ains || pseudo_4op/*i[0] == i[1] || pseudo_4op*/)
            {
                // Only use regular channels
                uint8_t expected_mode = 0;

                if(opl.AdlPercussionMode == 1)
                {
                    if(cmf_percussion_mode)
                        expected_mode = channel  < 11 ? 0 : (3 + channel  - 11); // CMF
                    else
                        expected_mode = PercussionMap[midiins & 0xFF];
                }

                if(opl.four_op_category[a] != expected_mode)
                    continue;
            }
            else
            {
                if(ccount == 0)
                {
                    // Only use four-op master channels
                    if(opl.four_op_category[a] != 1)
                        continue;
                }
                else
                {
                    // The secondary must be played on a specific channel.
                    if(a != static_cast<uint32_t>(adlchannel[0]) + 3)
                        continue;
                }
            }

            int64_t s = CalculateAdlChannelGoodness(a, voices[ccount], channel);
            if(s > bs)
            {
                bs = (int32_t)s;    // Best candidate wins
                c = static_cast<int32_t>(a);
            }
        }

        if(c < 0)
        {
            if(hooks.onDebugMessage)
                hooks.onDebugMessage(hooks.onDebugMessage_userData,
                                     "ignored unplaceable note [bank %i, inst %i, note %i, MIDI channel %i]",
                                     bank, midiChan.patch, note, channel);
            continue; // Could not play this note. Ignore it.
        }

        PrepareAdlChannelForNewNote(static_cast<size_t>(c), voices[ccount]);
        adlchannel[ccount] = c;
    }

    if(adlchannel[0] < 0 && adlchannel[1] < 0)
    {
        // The note could not be played, at all.
        return false;
    }

    //if(hooks.onDebugMessage)
    //    hooks.onDebugMessage(hooks.onDebugMessage_userData, "i1=%d:%d, i2=%d:%d", i[0],adlchannel[0], i[1],adlchannel[1]);

    // Allocate active note for MIDI channel
    std::pair<MIDIchannel::activenoteiterator, bool>
    ir = midiChan.activenotes_insert(note);
    ir.first->vol     = velocity;
    ir.first->vibrato = midiChan.noteAftertouch[note];
    ir.first->noteTone = tone;
    ir.first->currentTone = tone;
    ir.first->glideRate = HUGE_VAL;
    ir.first->midiins = midiins;
    ir.first->ains = ains;
    ir.first->chip_channels_count = 0;

    int8_t currentPortamentoSource = midiChan.portamentoSource;
    bool portamentoEnable = midiChan.portamentoEnable &&
        !isPercussion && !isXgPercussion;
    // Record the last note on MIDI channel as source of portamento
    midiChan.portamentoSource = portamentoEnable ? (int8_t)note : (int8_t)-1;

    // Enable gliding on portamento note
    if (portamentoEnable && currentPortamentoSource >= 0)
    {
        ir.first->currentTone = currentPortamentoSource;
        ir.first->glideRate = midiChan.portamentoRate;
    }

    for(unsigned ccount = 0; ccount < MIDIchannel::NoteInfo::MaxNumPhysChans; ++ccount)
    {
        int32_t c = adlchannel[ccount];
        if(c < 0)
            continue;
        uint16_t chipChan = static_cast<uint16_t>(adlchannel[ccount]);
        ir.first->phys_ensure_find_or_create(chipChan)->assign(voices[ccount]);
    }

    NoteUpdate(channel, ir.first, Upd_All | Upd_Patch);
    return true;
}

void MIDIplay::realTime_NoteOff(uint8_t channel, uint8_t note)
{
    channel = channel % 16;
    NoteOff(channel, note);
}

void MIDIplay::realTime_NoteAfterTouch(uint8_t channel, uint8_t note, uint8_t atVal)
{
    channel = channel % 16;
    MIDIchannel &chan = Ch[channel];
    MIDIchannel::activenoteiterator i = Ch[channel].activenotes_find(note);
    if(i)
    {
        i->vibrato = atVal;
    }

    uint8_t oldAtVal = chan.noteAftertouch[note % 128];
    if(atVal != oldAtVal)
    {
        chan.noteAftertouch[note % 128] = atVal;
        bool inUse = atVal != 0;
        for(unsigned n = 0; !inUse && n < 128; ++n)
            inUse = chan.noteAftertouch[n] != 0;
        chan.noteAfterTouchInUse = inUse;
    }
}

void MIDIplay::realTime_ChannelAfterTouch(uint8_t channel, uint8_t atVal)
{
    channel = channel % 16;
    Ch[channel].aftertouch = atVal;
}

void MIDIplay::realTime_Controller(uint8_t channel, uint8_t type, uint8_t value)
{
    channel = channel % 16;
    switch(type)
    {
    case 1: // Adjust vibrato
        //UI.PrintLn("%u:vibrato %d", MidCh,value);
        Ch[channel].vibrato = value;
        break;

    case 0: // Set bank msb (GM bank)
        Ch[channel].bank_msb = value;
        Ch[channel].is_xg_percussion = isXgPercChannel(Ch[channel].bank_msb, Ch[channel].bank_lsb);
        break;

    case 32: // Set bank lsb (XG bank)
        Ch[channel].bank_lsb = value;
        Ch[channel].is_xg_percussion = isXgPercChannel(Ch[channel].bank_msb, Ch[channel].bank_lsb);
        break;

    case 5: // Set portamento msb
        Ch[channel].portamento = static_cast<uint16_t>((Ch[channel].portamento & 0x7F) | (value << 7));
        UpdatePortamento(channel);
        break;

    case 37: // Set portamento lsb
        Ch[channel].portamento = (Ch[channel].portamento & 0x3F80) | (value);
        UpdatePortamento(channel);
        break;

    case 65: // Enable/disable portamento
        Ch[channel].portamentoEnable = value >= 64;
        UpdatePortamento(channel);
        break;

    case 7: // Change volume
        Ch[channel].volume = value;
        NoteUpdate_All(channel, Upd_Volume);
        break;

    case 74: // Change brightness
        Ch[channel].brightness = value;
        NoteUpdate_All(channel, Upd_Volume);
        break;

    case 64: // Enable/disable sustain
        Ch[channel].sustain = value;
        if(!value) KillSustainingNotes(channel);
        break;

    case 11: // Change expression (another volume factor)
        Ch[channel].expression = value;
        NoteUpdate_All(channel, Upd_Volume);
        break;

    case 10: // Change panning
        Ch[channel].panning = 0x00;
        if(value  < 64 + 32) Ch[channel].panning |= OPL_PANNING_LEFT;
        if(value >= 64 - 32) Ch[channel].panning |= OPL_PANNING_RIGHT;

        NoteUpdate_All(channel, Upd_Pan);
        break;

    case 121: // Reset all controllers
        Ch[channel].resetAllControllers();
        NoteUpdate_All(channel, Upd_Pan + Upd_Volume + Upd_Pitch);
        // Kill all sustained notes
        KillSustainingNotes(channel);
        break;

    case 120: // All sounds off
        NoteUpdate_All(channel, Upt_OffMute);
        break;

    case 123: // All notes off
        NoteUpdate_All(channel, Upd_Off);
        break;

    case 91:
        break; // Reverb effect depth. We don't do per-channel reverb.

    case 92:
        break; // Tremolo effect depth. We don't do...

    case 93:
        break; // Chorus effect depth. We don't do.

    case 94:
        break; // Celeste effect depth. We don't do.

    case 95:
        break; // Phaser effect depth. We don't do.

    case 98:
        Ch[channel].lastlrpn = value;
        Ch[channel].nrpn = true;
        break;

    case 99:
        Ch[channel].lastmrpn = value;
        Ch[channel].nrpn = true;
        break;

    case 100:
        Ch[channel].lastlrpn = value;
        Ch[channel].nrpn = false;
        break;

    case 101:
        Ch[channel].lastmrpn = value;
        Ch[channel].nrpn = false;
        break;

    case 113:
        break; // Related to pitch-bender, used by missimp.mid in Duke3D

    case  6:
        SetRPN(channel, value, true);
        break;

    case 38:
        SetRPN(channel, value, false);
        break;

    case 103:
        cmf_percussion_mode = (value != 0);
        break; // CMF (ctrl 0x67) rhythm mode

    default:
        break;
        //UI.PrintLn("Ctrl %d <- %d (ch %u)", ctrlno, value, MidCh);
    }
}

void MIDIplay::realTime_PatchChange(uint8_t channel, uint8_t patch)
{
    channel = channel % 16;
    Ch[channel].patch = patch;
}

void MIDIplay::realTime_PitchBend(uint8_t channel, uint16_t pitch)
{
    channel = channel % 16;
    Ch[channel].bend = int(pitch) - 8192;
    NoteUpdate_All(channel, Upd_Pitch);
}

void MIDIplay::realTime_PitchBend(uint8_t channel, uint8_t msb, uint8_t lsb)
{
    channel = channel % 16;
    Ch[channel].bend = int(lsb) + int(msb) * 128 - 8192;
    NoteUpdate_All(channel, Upd_Pitch);
}

void MIDIplay::realTime_BankChangeLSB(uint8_t channel, uint8_t lsb)
{
    channel = channel % 16;
    Ch[channel].bank_lsb = lsb;
}

void MIDIplay::realTime_BankChangeMSB(uint8_t channel, uint8_t msb)
{
    channel = channel % 16;
    Ch[channel].bank_msb = msb;
}

void MIDIplay::realTime_BankChange(uint8_t channel, uint16_t bank)
{
    channel = channel % 16;
    Ch[channel].bank_lsb = uint8_t(bank & 0xFF);
    Ch[channel].bank_msb = uint8_t((bank >> 8) & 0xFF);
}

void MIDIplay::realTime_panic()
{
    Panic();
    KillSustainingNotes(-1, -1);
}

void MIDIplay::AudioTick(uint32_t chipId, uint32_t rate)
{
    if(chipId != 0)  // do first chip ticks only
        return;

    uint32_t tickNumber = m_audioTickCounter++;
    double timeDelta = 1.0 / rate;

    enum { portamentoInterval = 32 }; // for efficiency, set rate limit on pitch updates

    if(tickNumber % portamentoInterval == 0)
    {
        double portamentoDelta = timeDelta * portamentoInterval;

        for(unsigned channel = 0; channel < 16; ++channel)
        {
            MIDIchannel &midiChan = Ch[channel];
            for(MIDIchannel::activenoteiterator it = midiChan.activenotes_begin();
                it; ++it)
            {
                double finalTone = it->noteTone;
                double previousTone = it->currentTone;

                bool directionUp = previousTone < finalTone;
                double toneIncr = portamentoDelta * (directionUp ? +it->glideRate : -it->glideRate);

                double currentTone = previousTone + toneIncr;
                bool glideFinished = !(directionUp ? (currentTone < finalTone) : (currentTone > finalTone));
                currentTone = glideFinished ? finalTone : currentTone;

                if(currentTone != previousTone)
                {
                    it->currentTone = currentTone;
                    NoteUpdate(channel, it, Upd_Pitch);
                }
            }
        }
    }
}

void MIDIplay::NoteUpdate(uint16_t MidCh,
                          MIDIplay::MIDIchannel::activenoteiterator i,
                          unsigned props_mask,
                          int32_t select_adlchn)
{
    MIDIchannel::NoteInfo &info = *i;
    const int16_t noteTone    = info.noteTone;
    const double currentTone    = info.currentTone;
    const uint8_t vol     = info.vol;
    const int midiins     = static_cast<int>(info.midiins);
    const adlinsdata2 &ains = *info.ains;
    AdlChannel::Location my_loc;
    my_loc.MidCh = MidCh;
    my_loc.note  = info.note;

    for(unsigned ccount = 0, ctotal = info.chip_channels_count; ccount < ctotal; ccount++)
    {
        const MIDIchannel::NoteInfo::Phys &ins = info.chip_channels[ccount];
        uint16_t c   = ins.chip_chan;

        if(select_adlchn >= 0 && c != select_adlchn) continue;

        if(props_mask & Upd_Patch)
        {
            opl.Patch(c, ins.ains);
            AdlChannel::LocationData *d = ch[c].users_find_or_create(my_loc);
            if(d)    // inserts if necessary
            {
                d->sustained = false;
                d->vibdelay  = 0;
                d->fixed_sustain = (ains.ms_sound_kon == static_cast<uint16_t>(adlNoteOnMaxTime));
                d->kon_time_until_neglible = ains.ms_sound_kon;
                d->ins       = ins;
            }
        }
    }

    for(unsigned ccount = 0; ccount < info.chip_channels_count; ccount++)
    {
        const MIDIchannel::NoteInfo::Phys &ins = info.chip_channels[ccount];
        uint16_t c   = ins.chip_chan;

        if(select_adlchn >= 0 && c != select_adlchn)
            continue;

        if(props_mask & Upd_Off) // note off
        {
            if(Ch[MidCh].sustain == 0)
            {
                AdlChannel::LocationData *k = ch[c].users_find(my_loc);

                if(k)
                    ch[c].users_erase(k);

                if(hooks.onNote)
                    hooks.onNote(hooks.onNote_userData, c, noteTone, midiins, 0, 0.0);

                if(ch[c].users_empty())
                {
                    opl.NoteOff(c);
                    if(props_mask & Upd_Mute) // Mute the note
                    {
                        opl.Touch_Real(c, 0);
                        ch[c].koff_time_until_neglible = 0;
                    }
                    else
                    {
                        ch[c].koff_time_until_neglible = ains.ms_sound_koff;
                    }
                }
            }
            else
            {
                // Sustain: Forget about the note, but don't key it off.
                //          Also will avoid overwriting it very soon.
                AdlChannel::LocationData *d = ch[c].users_find_or_create(my_loc);
                if(d)
                    d->sustained = true; // note: not erased!
                if(hooks.onNote)
                    hooks.onNote(hooks.onNote_userData, c, noteTone, midiins, -1, 0.0);
            }

            info.phys_erase_at(&ins);  // decrements channel count
            --ccount;  // adjusts index accordingly
            continue;
        }

        if(props_mask & Upd_Pan)
            opl.Pan(c, Ch[MidCh].panning);

        if(props_mask & Upd_Volume)
        {
            uint32_t volume;
            bool is_percussion = (MidCh == 9) || Ch[MidCh].is_xg_percussion;
            uint8_t brightness = is_percussion ? 127 : Ch[MidCh].brightness;

            if(!m_setup.fullRangeBrightnessCC74)
            {
                // Simulate post-High-Pass filter result which affects sounding by half level only
                if(brightness >= 64)
                    brightness = 127;
                else
                    brightness *= 2;
            }

            switch(opl.m_volumeScale)
            {

            case OPL3::VOLUME_Generic:
            {
                volume = vol * Ch[MidCh].volume * Ch[MidCh].expression;

                /* If the channel has arpeggio, the effective volume of
                     * *this* instrument is actually lower due to timesharing.
                     * To compensate, add extra volume that corresponds to the
                     * time this note is *not* heard.
                     * Empirical tests however show that a full equal-proportion
                     * increment sounds wrong. Therefore, using the square root.
                     */
                //volume = (int)(volume * std::sqrt( (double) ch[c].users.size() ));

                // The formula below: SOLVE(V=127^3 * 2^( (A-63.49999) / 8), A)
                volume = volume > 8725 ? static_cast<uint32_t>(std::log(static_cast<double>(volume)) * 11.541561 + (0.5 - 104.22845)) : 0;
                // The incorrect formula below: SOLVE(V=127^3 * (2^(A/63)-1), A)
                //opl.Touch_Real(c, volume>11210 ? 91.61112 * std::log(4.8819E-7*volume + 1.0)+0.5 : 0);

                opl.Touch_Real(c, volume, brightness);
                //opl.Touch(c, volume);
            }
            break;

            case OPL3::VOLUME_NATIVE:
            {
                volume = vol * Ch[MidCh].volume * Ch[MidCh].expression;
                volume = volume * 127 / (127 * 127 * 127) / 2;
                opl.Touch_Real(c, volume, brightness);
            }
            break;

            case OPL3::VOLUME_DMX:
            {
                volume = 2 * ((Ch[MidCh].volume * Ch[MidCh].expression) * 127 / 16129) + 1;
                //volume = 2 * (Ch[MidCh].volume) + 1;
                volume = (DMX_volume_mapping_table[(vol < 128) ? vol : 127] * volume) >> 9;
                opl.Touch_Real(c, volume, brightness);
            }
            break;

            case OPL3::VOLUME_APOGEE:
            {
                volume = ((Ch[MidCh].volume * Ch[MidCh].expression) * 127 / 16129);
                volume = ((64 * (vol + 0x80)) * volume) >> 15;
                //volume = ((63 * (vol + 0x80)) * Ch[MidCh].volume) >> 15;
                opl.Touch_Real(c, volume, brightness);
            }
            break;

            case OPL3::VOLUME_9X:
            {
                //volume = 63 - W9X_volume_mapping_table[(((vol * Ch[MidCh].volume /** Ch[MidCh].expression*/) * 127 / 16129 /*2048383*/) >> 2)];
                volume = 63 - W9X_volume_mapping_table[(((vol * Ch[MidCh].volume * Ch[MidCh].expression) * 127 / 2048383) >> 2)];
                //volume = W9X_volume_mapping_table[vol >> 2] + volume;
                opl.Touch_Real(c, volume, brightness);
            }
            break;
            }

            /* DEBUG ONLY!!!
                    static uint32_t max = 0;

                    if(volume == 0)
                        max = 0;

                    if(volume > max)
                        max = volume;

                    printf("%d\n", max);
                    fflush(stdout);
                    */
        }

        if(props_mask & Upd_Pitch)
        {
            AdlChannel::LocationData *d = ch[c].users_find(my_loc);

            // Don't bend a sustained note
            if(!d || !d->sustained)
            {
                double midibend = Ch[MidCh].bend * Ch[MidCh].bendsense;
                double bend = midibend + ins.ains.finetune;
                double phase = 0.0;
                uint8_t vibrato = std::max(Ch[MidCh].vibrato, Ch[MidCh].aftertouch);
                vibrato = std::max(vibrato, i->vibrato);

                if((ains.flags & adlinsdata::Flag_Pseudo4op) && ins.pseudo4op)
                {
                    phase = ains.voice2_fine_tune;//0.125; // Detune the note slightly (this is what Doom does)
                }

                if(vibrato && (!d || d->vibdelay >= Ch[MidCh].vibdelay))
                    bend += static_cast<double>(vibrato) * Ch[MidCh].vibdepth * std::sin(Ch[MidCh].vibpos);

#ifdef ADLMIDI_USE_DOSBOX_OPL
#   define BEND_COEFFICIENT 172.00093
#else
#   define BEND_COEFFICIENT 172.4387
#endif
                opl.NoteOn(c, BEND_COEFFICIENT * std::exp(0.057762265 * (currentTone + bend + phase)));
#undef BEND_COEFFICIENT
                if(hooks.onNote)
                    hooks.onNote(hooks.onNote_userData, c, noteTone, midiins, vol, midibend);
            }
        }
    }

    if(info.chip_channels_count == 0)
        Ch[MidCh].activenotes_erase(i);
}

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
bool MIDIplay::ProcessEventsNew(bool isSeek)
{
    if(CurrentPositionNew.track.size() == 0)
        atEnd = true;//No MIDI track data to play
    if(atEnd)
        return false;//No more events in the queue

    loopEnd = false;
    const size_t        TrackCount = CurrentPositionNew.track.size();
    const PositionNew   RowBeginPosition(CurrentPositionNew);

#ifdef DEBUG_TIME_CALCULATION
    double maxTime = 0.0;
#endif

    for(size_t tk = 0; tk < TrackCount; ++tk)
    {
        PositionNew::TrackInfo &track = CurrentPositionNew.track[tk];
        if((track.status >= 0) && (track.delay <= 0))
        {
            //Check is an end of track has been reached
            if(track.pos == trackDataNew[tk].end())
            {
                track.status = -1;
                break;
            }

            // Handle event
            for(size_t i = 0; i < track.pos->events.size(); i++)
            {
                const MidiEvent &evt = track.pos->events[i];
#ifdef ENABLE_BEGIN_SILENCE_SKIPPING
                if(!CurrentPositionNew.began && (evt.type == MidiEvent::T_NOTEON))
                    CurrentPositionNew.began = true;
#endif
                if(isSeek && (evt.type == MidiEvent::T_NOTEON))
                    continue;
                HandleEvent(tk, evt, track.status);
                if(loopEnd)
                    break;//Stop event handling on catching loopEnd event!
            }

#ifdef DEBUG_TIME_CALCULATION
            if(maxTime < track.pos->time)
                maxTime = track.pos->time;
#endif
            // Read next event time (unless the track just ended)
            if(track.status >= 0)
            {
                track.delay += track.pos->delay;
                track.pos++;
            }
        }
    }

#ifdef DEBUG_TIME_CALCULATION
    std::fprintf(stdout, "                              \r");
    std::fprintf(stdout, "Time: %10f; Audio: %10f\r", maxTime, CurrentPositionNew.absTimePosition);
    std::fflush(stdout);
#endif

    // Find shortest delay from all track
    uint64_t shortest = 0;
    bool     shortest_no = true;

    for(size_t tk = 0; tk < TrackCount; ++tk)
    {
        PositionNew::TrackInfo &track = CurrentPositionNew.track[tk];
        if((track.status >= 0) && (shortest_no || track.delay < shortest))
        {
            shortest = track.delay;
            shortest_no = false;
        }
    }

    //if(shortest > 0) UI.PrintLn("shortest: %ld", shortest);

    // Schedule the next playevent to be processed after that delay
    for(size_t tk = 0; tk < TrackCount; ++tk)
        CurrentPositionNew.track[tk].delay -= shortest;

    fraction<uint64_t> t = shortest * Tempo;

#ifdef ENABLE_BEGIN_SILENCE_SKIPPING
    if(CurrentPositionNew.began)
#endif
        CurrentPositionNew.wait += t.value();

    //if(shortest > 0) UI.PrintLn("Delay %ld (%g)", shortest, (double)t.valuel());
    if(loopStart)
    {
        LoopBeginPositionNew = RowBeginPosition;
        loopStart = false;
    }

    if(shortest_no || loopEnd)
    {
        //Loop if song end or loop end point has reached
        loopEnd         = false;
        shortest = 0;
        if(!m_setup.loopingIsEnabled)
        {
            atEnd = true; //Don't handle events anymore
            CurrentPositionNew.wait += postSongWaitDelay;//One second delay until stop playing
            return true;//We have caugh end here!
        }
        CurrentPositionNew = LoopBeginPositionNew;
    }

    return true;//Has events in queue
}

MIDIplay::MidiEvent MIDIplay::parseEvent(uint8_t **pptr, uint8_t *end, int &status)
{
    uint8_t *&ptr = *pptr;
    MIDIplay::MidiEvent evt;

    if(ptr + 1 > end)
    {
        //When track doesn't ends on the middle of event data, it's must be fine
        evt.type = MidiEvent::T_SPECIAL;
        evt.subtype = MidiEvent::ST_ENDTRACK;
        return evt;
    }

    unsigned char byte = *(ptr++);
    bool ok = false;

    if(byte == MidiEvent::T_SYSEX || byte == MidiEvent::T_SYSEX2)// Ignore SysEx
    {
        uint64_t length = ReadVarLenEx(pptr, end, ok);
        if(!ok || (ptr + length > end))
        {
            errorString += "parseEvent: Can't read SysEx event - Unexpected end of track data.\n";
            evt.isValid = 0;
            return evt;
        }
        ptr += (size_t)length;
        return evt;
    }

    if(byte == MidiEvent::T_SPECIAL)
    {
        // Special event FF
        uint8_t  evtype = *(ptr++);
        uint64_t length = ReadVarLenEx(pptr, end, ok);
        if(!ok || (ptr + length > end))
        {
            errorString += "parseEvent: Can't read Special event - Unexpected end of track data.\n";
            evt.isValid = 0;
            return evt;
        }
        std::string data(length ? (const char *)ptr : 0, (size_t)length);
        ptr += (size_t)length;

        evt.type = byte;
        evt.subtype = evtype;
        evt.data.insert(evt.data.begin(), data.begin(), data.end());

#if 0 /* Print all tempo events */
        if(evt.subtype == MidiEvent::ST_TEMPOCHANGE)
        {
            if(hooks.onDebugMessage)
                hooks.onDebugMessage(hooks.onDebugMessage_userData, "Temp Change: %02X%02X%02X", evt.data[0], evt.data[1], evt.data[2]);
        }
#endif

        /* TODO: Store those meta-strings separately and give ability to read them
         * by external functions (to display song title and copyright in the player) */
        if(evt.subtype == MidiEvent::ST_COPYRIGHT)
        {
            if(musCopyright.empty())
            {
                musCopyright = std::string((const char *)evt.data.data(), evt.data.size());
                if(hooks.onDebugMessage)
                    hooks.onDebugMessage(hooks.onDebugMessage_userData, "Music copyright: %s", musCopyright.c_str());
            }
            else if(hooks.onDebugMessage)
            {
                std::string str((const char *)evt.data.data(), evt.data.size());
                hooks.onDebugMessage(hooks.onDebugMessage_userData, "Extra copyright event: %s", str.c_str());
            }
        }
        else if(evt.subtype == MidiEvent::ST_SQTRKTITLE)
        {
            if(musTitle.empty())
            {
                musTitle = std::string((const char *)evt.data.data(), evt.data.size());
                if(hooks.onDebugMessage)
                    hooks.onDebugMessage(hooks.onDebugMessage_userData, "Music title: %s", musTitle.c_str());
            }
            else if(hooks.onDebugMessage)
            {
                //TODO: Store track titles and associate them with each track and make API to retreive them
                std::string str((const char *)evt.data.data(), evt.data.size());
                musTrackTitles.push_back(str);
                hooks.onDebugMessage(hooks.onDebugMessage_userData, "Track title: %s", str.c_str());
            }
        }
        else if(evt.subtype == MidiEvent::ST_INSTRTITLE)
        {
            if(hooks.onDebugMessage)
            {
                std::string str((const char *)evt.data.data(), evt.data.size());
                hooks.onDebugMessage(hooks.onDebugMessage_userData, "Instrument: %s", str.c_str());
            }
        }
        else if(evt.subtype == MidiEvent::ST_MARKER)
        {
            //To lower
            for(size_t i = 0; i < data.size(); i++)
            {
                if(data[i] <= 'Z' && data[i] >= 'A')
                    data[i] = data[i] - ('Z' - 'z');
            }

            if(data == "loopstart")
            {
                //Return a custom Loop Start event instead of Marker
                evt.subtype = MidiEvent::ST_LOOPSTART;
                evt.data.clear();//Data is not needed
                return evt;
            }

            if(data == "loopend")
            {
                //Return a custom Loop End event instead of Marker
                evt.subtype = MidiEvent::ST_LOOPEND;
                evt.data.clear();//Data is not needed
                return evt;
            }
        }

        if(evtype == MidiEvent::ST_ENDTRACK)
            status = -1;//Finalize track

        return evt;
    }

    // Any normal event (80..EF)
    if(byte < 0x80)
    {
        byte = static_cast<uint8_t>(status | 0x80);
        ptr--;
    }

    //Sys Com Song Select(Song #) [0-127]
    if(byte == MidiEvent::T_SYSCOMSNGSEL)
    {
        if(ptr + 1 > end)
        {
            errorString += "parseEvent: Can't read System Command Song Select event - Unexpected end of track data.\n";
            evt.isValid = 0;
            return evt;
        }
        evt.type = byte;
        evt.data.push_back(*(ptr++));
        return evt;
    }

    //Sys Com Song Position Pntr [LSB, MSB]
    if(byte == MidiEvent::T_SYSCOMSPOSPTR)
    {
        if(ptr + 2 > end)
        {
            errorString += "parseEvent: Can't read System Command Position Pointer event - Unexpected end of track data.\n";
            evt.isValid = 0;
            return evt;
        }
        evt.type = byte;
        evt.data.push_back(*(ptr++));
        evt.data.push_back(*(ptr++));
        return evt;
    }

    uint8_t midCh = byte & 0x0F, evType = (byte >> 4) & 0x0F;
    status = byte;
    evt.channel = midCh;
    evt.type = evType;

    switch(evType)
    {
    case MidiEvent::T_NOTEOFF://2 byte length
    case MidiEvent::T_NOTEON:
    case MidiEvent::T_NOTETOUCH:
    case MidiEvent::T_CTRLCHANGE:
    case MidiEvent::T_WHEEL:
        if(ptr + 2 > end)
        {
            errorString += "parseEvent: Can't read regular 2-byte event - Unexpected end of track data.\n";
            evt.isValid = 0;
            return evt;
        }

        evt.data.push_back(*(ptr++));
        evt.data.push_back(*(ptr++));

        /* TODO: Implement conversion of RSXX's note volumes out of synthesizer */
        /*if((opl.m_musicMode == OPL3::MODE_RSXX) && (evType == MidiEvent::T_NOTEON) && (evt.data[1] != 0))
        {
            //NOT WORKING YET
            evt.type = MidiEvent::T_NOTETOUCH;
        }
        else */if((evType == MidiEvent::T_NOTEON) && (evt.data[1] == 0))
        {
            evt.type = MidiEvent::T_NOTEOFF; // Note ON with zero velocity is Note OFF!
        } //111'th loopStart controller (RPG Maker and others)
        else if((evType == MidiEvent::T_CTRLCHANGE) && (evt.data[0] == 111))
        {
            //Change event type to custom Loop Start event and clear data
            evt.type = MidiEvent::T_SPECIAL;
            evt.subtype = MidiEvent::ST_LOOPSTART;
            evt.data.clear();
        }

        return evt;
    case MidiEvent::T_PATCHCHANGE://1 byte length
    case MidiEvent::T_CHANAFTTOUCH:
        if(ptr + 1 > end)
        {
            errorString += "parseEvent: Can't read regular 1-byte event - Unexpected end of track data.\n";
            evt.isValid = 0;
            return evt;
        }
        evt.data.push_back(*(ptr++));
        return evt;
    }

    return evt;
}
#endif /* ADLMIDI_DISABLE_MIDI_SEQUENCER */

const std::string &MIDIplay::getErrorString()
{
    return errorStringOut;
}

void MIDIplay::setErrorString(const std::string &err)
{
    errorStringOut = err;
}

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
void MIDIplay::HandleEvent(size_t tk, const MIDIplay::MidiEvent &evt, int &status)
{
    if(hooks.onEvent)
    {
        hooks.onEvent(hooks.onEvent_userData,
                      evt.type,
                      evt.subtype,
                      evt.channel,
                      evt.data.data(),
                      evt.data.size());
    }

    if(evt.type == MidiEvent::T_SYSEX || evt.type == MidiEvent::T_SYSEX2) // Ignore SysEx
    {
        //std::string data( length?(const char*) &TrackData[tk][CurrentPosition.track[tk].ptr]:0, length );
        //UI.PrintLn("SysEx %02X: %u bytes", byte, length/*, data.c_str()*/);
        return;
    }

    if(evt.type == MidiEvent::T_SPECIAL)
    {
        // Special event FF
        uint8_t  evtype = evt.subtype;
        uint64_t length = (uint64_t)evt.data.size();
        std::string data(length ? (const char *)evt.data.data() : 0, (size_t)length);

        if(evtype == MidiEvent::ST_ENDTRACK)//End Of Track
        {
            status = -1;
            return;
        }

        if(evtype == MidiEvent::ST_TEMPOCHANGE)//Tempo change
        {
            Tempo = InvDeltaTicks * fraction<uint64_t>(ReadBEint(evt.data.data(), evt.data.size()));
            return;
        }

        if(evtype == MidiEvent::ST_MARKER)//Meta event
        {
            //Do nothing! :-P
            return;
        }

        if(evtype == MidiEvent::ST_DEVICESWITCH)
        {
            current_device[tk] = ChooseDevice(data);
            return;
        }

        //if(evtype >= 1 && evtype <= 6)
        //    UI.PrintLn("Meta %d: %s", evtype, data.c_str());

        //Turn on Loop handling when loop is enabled
        if(m_setup.loopingIsEnabled && !invalidLoop)
        {
            if(evtype == MidiEvent::ST_LOOPSTART) // Special non-spec ADLMIDI special for IMF playback: Direct poke to AdLib
            {
                loopStart = true;
                return;
            }

            if(evtype == MidiEvent::ST_LOOPEND) // Special non-spec ADLMIDI special for IMF playback: Direct poke to AdLib
            {
                loopEnd = true;
                return;
            }
        }

        if(evtype == MidiEvent::ST_RAWOPL) // Special non-spec ADLMIDI special for IMF playback: Direct poke to AdLib
        {
            uint8_t i = static_cast<uint8_t>(data[0]), v = static_cast<uint8_t>(data[1]);
            if((i & 0xF0) == 0xC0)
                v |= 0x30;
            //std::printf("OPL poke %02X, %02X\n", i, v);
            //std::fflush(stdout);
            opl.Poke(0, i, v);
            return;
        }

        return;
    }

    // Any normal event (80..EF)
    //    if(evt.type < 0x80)
    //    {
    //        byte = static_cast<uint8_t>(CurrentPosition.track[tk].status | 0x80);
    //        CurrentPosition.track[tk].ptr--;
    //    }

    if(evt.type == MidiEvent::T_SYSCOMSNGSEL ||
       evt.type == MidiEvent::T_SYSCOMSPOSPTR)
        return;

    /*UI.PrintLn("@%X Track %u: %02X %02X",
                CurrentPosition.track[tk].ptr-1, (unsigned)tk, byte,
                TrackData[tk][CurrentPosition.track[tk].ptr]);*/
    uint8_t  midCh = evt.channel;//byte & 0x0F, EvType = byte >> 4;
    midCh += (uint8_t)current_device[tk];
    status = evt.type;

    switch(evt.type)
    {
    case MidiEvent::T_NOTEOFF: // Note off
    {
        uint8_t note = evt.data[0];
        realTime_NoteOff(midCh, note);
        break;
    }

    case MidiEvent::T_NOTEON: // Note on
    {
        uint8_t note = evt.data[0];
        uint8_t vol  = evt.data[1];
        /*if(*/ realTime_NoteOn(midCh, note, vol); /*)*/
        //CurrentPosition.began  = true;
        break;
    }

    case MidiEvent::T_NOTETOUCH: // Note touch
    {
        uint8_t note = evt.data[0];
        uint8_t vol =  evt.data[1];
        realTime_NoteAfterTouch(midCh, note, vol);
        break;
    }

    case MidiEvent::T_CTRLCHANGE: // Controller change
    {
        uint8_t ctrlno = evt.data[0];
        uint8_t value =  evt.data[1];
        realTime_Controller(midCh, ctrlno, value);
        break;
    }

    case MidiEvent::T_PATCHCHANGE: // Patch change
        realTime_PatchChange(midCh, evt.data[0]);
        break;

    case MidiEvent::T_CHANAFTTOUCH: // Channel after-touch
    {
        // TODO: Verify, is this correct action?
        uint8_t vol = evt.data[0];
        realTime_ChannelAfterTouch(midCh, vol);
        break;
    }

    case MidiEvent::T_WHEEL: // Wheel/pitch bend
    {
        uint8_t a = evt.data[0];
        uint8_t b = evt.data[1];
        realTime_PitchBend(midCh, b, a);
        break;
    }
    }
}
#endif /* ADLMIDI_DISABLE_MIDI_SEQUENCER */

int64_t MIDIplay::CalculateAdlChannelGoodness(size_t c, const MIDIchannel::NoteInfo::Phys &ins, uint16_t) const
{
    int64_t s = -ch[c].koff_time_until_neglible;

    // Same midi-instrument = some stability
    //if(c == MidCh) s += 4;
    for(AdlChannel::LocationData *j = ch[c].users_first; j; j = j->next)
    {
        s -= 4000;

        if(!j->sustained)
            s -= j->kon_time_until_neglible;
        else
            s -= (j->kon_time_until_neglible / 2);

        MIDIchannel::activenoteiterator
        k = const_cast<MIDIchannel &>(Ch[j->loc.MidCh]).activenotes_find(j->loc.note);

        if(k)
        {
            // Same instrument = good
            if(j->ins == ins)
            {
                s += 300;
                // Arpeggio candidate = even better
                if(j->vibdelay < 70
                   || j->kon_time_until_neglible > 20000)
                    s += 0;
            }

            // Percussion is inferior to melody
            s += 50 * (int64_t)(k->midiins / 128);
            /*
                    if(k->second.midiins >= 25
                    && k->second.midiins < 40
                    && j->second.ins != ins)
                    {
                        s -= 14000; // HACK: Don't clobber the bass or the guitar
                    }
                    */
        }

        // If there is another channel to which this note
        // can be evacuated to in the case of congestion,
        // increase the score slightly.
        unsigned n_evacuation_stations = 0;

        for(size_t c2 = 0; c2 < static_cast<size_t>(opl.NumChannels); ++c2)
        {
            if(c2 == c) continue;

            if(opl.four_op_category[c2]
               != opl.four_op_category[c]) continue;

            for(AdlChannel::LocationData *m = ch[c2].users_first; m; m = m->next)
            {
                if(m->sustained)       continue;
                if(m->vibdelay >= 200) continue;
                if(m->ins != j->ins) continue;
                n_evacuation_stations += 1;
            }
        }

        s += (int64_t)n_evacuation_stations * 4;
    }

    return s;
}


void MIDIplay::PrepareAdlChannelForNewNote(size_t c, const MIDIchannel::NoteInfo::Phys &ins)
{
    if(ch[c].users_empty()) return; // Nothing to do

    //bool doing_arpeggio = false;
    for(AdlChannel::LocationData *jnext = ch[c].users_first; jnext;)
    {
        AdlChannel::LocationData *j = jnext;
        jnext = jnext->next;

        if(!j->sustained)
        {
            // Collision: Kill old note,
            // UNLESS we're going to do arpeggio
            MIDIchannel::activenoteiterator i
            (Ch[j->loc.MidCh].activenotes_ensure_find(j->loc.note));

            // Check if we can do arpeggio.
            if((j->vibdelay < 70
                || j->kon_time_until_neglible > 20000)
               && j->ins == ins)
            {
                // Do arpeggio together with this note.
                //doing_arpeggio = true;
                continue;
            }

            KillOrEvacuate(c, j, i);
            // ^ will also erase j from ch[c].users.
        }
    }

    // Kill all sustained notes on this channel
    // Don't keep them for arpeggio, because arpeggio requires
    // an intact "activenotes" record. This is a design flaw.
    KillSustainingNotes(-1, static_cast<int32_t>(c));

    // Keyoff the channel so that it can be retriggered,
    // unless the new note will be introduced as just an arpeggio.
    if(ch[c].users_empty())
        opl.NoteOff(c);
}

void MIDIplay::KillOrEvacuate(size_t from_channel,
                              AdlChannel::LocationData *j,
                              MIDIplay::MIDIchannel::activenoteiterator i)
{
    // Before killing the note, check if it can be
    // evacuated to another channel as an arpeggio
    // instrument. This helps if e.g. all channels
    // are full of strings and we want to do percussion.
    // FIXME: This does not care about four-op entanglements.
    for(uint32_t c = 0; c < opl.NumChannels; ++c)
    {
        uint16_t cs = static_cast<uint16_t>(c);

        if(c > std::numeric_limits<uint32_t>::max())
            break;
        if(c == from_channel)
            continue;
        if(opl.four_op_category[c] != opl.four_op_category[from_channel])
            continue;

        AdlChannel &adlch = ch[c];
        if(adlch.users_size == AdlChannel::users_max)
            continue;  // no room for more arpeggio on channel

        for(AdlChannel::LocationData *m = adlch.users_first; m; m = m->next)
        {
            if(m->vibdelay >= 200
               && m->kon_time_until_neglible < 10000) continue;
            if(m->ins != j->ins)
                continue;
            if(hooks.onNote)
            {
                hooks.onNote(hooks.onNote_userData,
                             (int)from_channel,
                             i->noteTone,
                             static_cast<int>(i->midiins), 0, 0.0);
                hooks.onNote(hooks.onNote_userData,
                             (int)c,
                             i->noteTone,
                             static_cast<int>(i->midiins),
                             i->vol, 0.0);
            }

            i->phys_erase(static_cast<uint16_t>(from_channel));
            i->phys_ensure_find_or_create(cs)->assign(j->ins);
            if(!ch[cs].users_insert(*j))
                assert(false);
            ch[from_channel].users_erase(j);
            return;
        }
    }

    /*UI.PrintLn(
                "collision @%u: [%ld] <- ins[%3u]",
                c,
                //ch[c].midiins<128?'M':'P', ch[c].midiins&127,
                ch[c].age, //adlins[ch[c].insmeta].ms_sound_kon,
                ins
                );*/
    // Kill it
    NoteUpdate(j->loc.MidCh,
               i,
               Upd_Off,
               static_cast<int32_t>(from_channel));
}

void MIDIplay::Panic()
{
    for(uint8_t chan = 0; chan < Ch.size(); chan++)
    {
        for(uint8_t note = 0; note < 128; note++)
            realTime_NoteOff(chan, note);
    }
}

void MIDIplay::KillSustainingNotes(int32_t MidCh, int32_t this_adlchn)
{
    uint32_t first = 0, last = opl.NumChannels;

    if(this_adlchn >= 0)
    {
        first = static_cast<uint32_t>(this_adlchn);
        last = first + 1;
    }

    for(unsigned c = first; c < last; ++c)
    {
        if(ch[c].users_empty()) continue; // Nothing to do

        for(AdlChannel::LocationData *jnext = ch[c].users_first; jnext;)
        {
            AdlChannel::LocationData *j = jnext;
            jnext = jnext->next;

            if((MidCh < 0 || j->loc.MidCh == MidCh)
               && j->sustained)
            {
                int midiins = '?';
                if(hooks.onNote)
                    hooks.onNote(hooks.onNote_userData, (int)c, j->loc.note, midiins, 0, 0.0);
                ch[c].users_erase(j);
            }
        }

        // Keyoff the channel, if there are no users left.
        if(ch[c].users_empty())
            opl.NoteOff(c);
    }
}

void MIDIplay::SetRPN(unsigned MidCh, unsigned value, bool MSB)
{
    bool nrpn = Ch[MidCh].nrpn;
    unsigned addr = Ch[MidCh].lastmrpn * 0x100 + Ch[MidCh].lastlrpn;

    switch(addr + nrpn * 0x10000 + MSB * 0x20000)
    {
    case 0x0000 + 0*0x10000 + 1*0x20000: // Pitch-bender sensitivity
        Ch[MidCh].bendsense_msb = value;
        Ch[MidCh].updateBendSensitivity();
        break;
    case 0x0000 + 0*0x10000 + 0*0x20000: // Pitch-bender sensitivity LSB
        Ch[MidCh].bendsense_lsb = value;
        Ch[MidCh].updateBendSensitivity();
        break;
    case 0x0108 + 1*0x10000 + 1*0x20000: // Vibrato speed
        if(value == 64)      Ch[MidCh].vibspeed = 1.0;
        else if(value < 100) Ch[MidCh].vibspeed = 1.0 / (1.6e-2 * (value ? value : 1));
        else                 Ch[MidCh].vibspeed = 1.0 / (0.051153846 * value - 3.4965385);
        Ch[MidCh].vibspeed *= 2 * 3.141592653 * 5.0;
        break;
    case 0x0109 + 1*0x10000 + 1*0x20000: // Vibrato depth
        Ch[MidCh].vibdepth = ((value - 64) * 0.15) * 0.01;
        break;
    case 0x010A + 1*0x10000 + 1*0x20000: // Vibrato delay in millisecons
        Ch[MidCh].vibdelay = value ? int64_t(0.2092 * std::exp(0.0795 * (double)value)) : 0;
        break;
    default:/* UI.PrintLn("%s %04X <- %d (%cSB) (ch %u)",
                "NRPN"+!nrpn, addr, value, "LM"[MSB], MidCh);*/
        break;
    }
}

void MIDIplay::UpdatePortamento(unsigned MidCh)
{
    double rate = HUGE_VAL;
    uint16_t midival = Ch[MidCh].portamento;
    if(Ch[MidCh].portamentoEnable && midival > 0)
        rate = 350.0 * std::exp2(-0.062 * (1.0 / 128) * midival);
    Ch[MidCh].portamentoRate = rate;
}

void MIDIplay::NoteUpdate_All(uint16_t MidCh, unsigned props_mask)
{
    for(MIDIchannel::activenoteiterator
        i = Ch[MidCh].activenotes_begin(); i;)
    {
        MIDIchannel::activenoteiterator j(i++);
        NoteUpdate(MidCh, j, props_mask);
    }
}

void MIDIplay::NoteOff(uint16_t MidCh, uint8_t note)
{
    MIDIchannel::activenoteiterator
    i = Ch[MidCh].activenotes_find(note);

    if(i)
        NoteUpdate(MidCh, i, Upd_Off);
}


void MIDIplay::UpdateVibrato(double amount)
{
    for(size_t a = 0, b = Ch.size(); a < b; ++a)
    {
        if(Ch[a].hasVibrato() && !Ch[a].activenotes_empty())
        {
            NoteUpdate_All(static_cast<uint16_t>(a), Upd_Pitch);
            Ch[a].vibpos += amount * Ch[a].vibspeed;
        }
        else
            Ch[a].vibpos = 0.0;
    }
}




uint64_t MIDIplay::ChooseDevice(const std::string &name)
{
    std::map<std::string, uint64_t>::iterator i = devices.find(name);

    if(i != devices.end())
        return i->second;

    size_t n = devices.size() * 16;
    devices.insert(std::make_pair(name, n));
    Ch.resize(n + 16);
    return n;
}

void MIDIplay::UpdateArpeggio(double) // amount = amount of time passed
{
    // If there is an adlib channel that has multiple notes
    // simulated on the same channel, arpeggio them.
#if 0
    const unsigned desired_arpeggio_rate = 40; // Hz (upper limit)
#   if 1
    static unsigned cache = 0;
    amount = amount; // Ignore amount. Assume we get a constant rate.
    cache += MaxSamplesAtTime * desired_arpeggio_rate;

    if(cache < PCM_RATE) return;

    cache %= PCM_RATE;
#   else
    static double arpeggio_cache = 0;
    arpeggio_cache += amount * desired_arpeggio_rate;

    if(arpeggio_cache < 1.0) return;

    arpeggio_cache = 0.0;
#   endif
#endif

    ++m_arpeggioCounter;

    for(uint32_t c = 0; c < opl.NumChannels; ++c)
    {
retry_arpeggio:
        if(c > uint32_t(std::numeric_limits<int32_t>::max()))
            break;

        size_t n_users = ch[c].users_size;

        if(n_users > 1)
        {
            AdlChannel::LocationData *i = ch[c].users_first;
            size_t rate_reduction = 3;

            if(n_users >= 3)
                rate_reduction = 2;

            if(n_users >= 4)
                rate_reduction = 1;

            for(size_t count = (m_arpeggioCounter / rate_reduction) % n_users,
                n = 0; n < count; ++n)
                i = i->next;

            if(i->sustained == false)
            {
                if(i->kon_time_until_neglible <= 0l)
                {
                    NoteUpdate(
                        i->loc.MidCh,
                        Ch[ i->loc.MidCh ].activenotes_ensure_find(i->loc.note),
                        Upd_Off,
                        static_cast<int32_t>(c));
                    goto retry_arpeggio;
                }

                NoteUpdate(
                    i->loc.MidCh,
                    Ch[ i->loc.MidCh ].activenotes_ensure_find(i->loc.note),
                    Upd_Pitch | Upd_Volume | Upd_Pan,
                    static_cast<int32_t>(c));
            }
        }
    }
}


#ifndef ADLMIDI_DISABLE_CPP_EXTRAS

struct AdlInstrumentTester::Impl
{
    uint32_t cur_gm;
    uint32_t ins_idx;
    std::vector<uint32_t> adl_ins_list;
    OPL3 *opl;
    MIDIplay *play;
};

ADLMIDI_EXPORT AdlInstrumentTester::AdlInstrumentTester(ADL_MIDIPlayer *device)
    : P(new Impl)
{
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    P->cur_gm = 0;
    P->ins_idx = 0;
    P->play = play;
    P->opl = play ? &play->opl : NULL;
}

ADLMIDI_EXPORT AdlInstrumentTester::~AdlInstrumentTester()
{
    delete P;
}

ADLMIDI_EXPORT void AdlInstrumentTester::FindAdlList()
{
    const unsigned NumBanks = (unsigned)adl_getBanksCount();
    std::set<unsigned> adl_ins_set;
    for(unsigned bankno = 0; bankno < NumBanks; ++bankno)
        adl_ins_set.insert(banks[bankno][P->cur_gm]);
    P->adl_ins_list.assign(adl_ins_set.begin(), adl_ins_set.end());
    P->ins_idx = 0;
    NextAdl(0);
    P->opl->Silence();
}



ADLMIDI_EXPORT void AdlInstrumentTester::Touch(unsigned c, unsigned volume) // Volume maxes at 127*127*127
{
    OPL3 *opl = P->opl;
    if(opl->m_volumeScale == OPL3::VOLUME_NATIVE)
        opl->Touch_Real(c, volume * 127 / (127 * 127 * 127) / 2);
    else
    {
        // The formula below: SOLVE(V=127^3 * 2^( (A-63.49999) / 8), A)
        opl->Touch_Real(c, volume > 8725 ? static_cast<unsigned int>(std::log((double)volume) * 11.541561 + (0.5 - 104.22845)) : 0);
        // The incorrect formula below: SOLVE(V=127^3 * (2^(A/63)-1), A)
        //Touch_Real(c, volume>11210 ? 91.61112 * std::log(4.8819E-7*volume + 1.0)+0.5 : 0);
    }
}

ADLMIDI_EXPORT void AdlInstrumentTester::DoNote(int note)
{
    MIDIplay *play = P->play;
    OPL3 *opl = P->opl;
    if(P->adl_ins_list.empty()) FindAdlList();
    const unsigned meta = P->adl_ins_list[P->ins_idx];
    const adlinsdata2 ains(adlins[meta]);

    int tone = (P->cur_gm & 128) ? (P->cur_gm & 127) : (note + 50);
    if(ains.tone)
    {
        /*if(ains.tone < 20)
                tone += ains.tone;
            else */
        if(ains.tone < 128)
            tone = ains.tone;
        else
            tone -= ains.tone - 128;
    }
    double hertz = 172.00093 * std::exp(0.057762265 * (tone + 0.0));
    int32_t adlchannel[2] = { 0, 3 };
    if(ains.adl[0] == ains.adl[1])
    {
        adlchannel[1] = -1;
        adlchannel[0] = 6; // single-op
        if(play->hooks.onDebugMessage)
        {
            play->hooks.onDebugMessage(play->hooks.onDebugMessage_userData,
                                       "noteon at %d for %g Hz\n", adlchannel[0], hertz);
        }
    }
    else
    {
        if(play->hooks.onDebugMessage)
        {
            play->hooks.onDebugMessage(play->hooks.onDebugMessage_userData,
                                       "noteon at %d and %d for %g Hz\n", adlchannel[0], adlchannel[1], hertz);
        }
    }

    opl->NoteOff(0);
    opl->NoteOff(3);
    opl->NoteOff(6);
    for(unsigned c = 0; c < 2; ++c)
    {
        if(adlchannel[c] < 0) continue;
        opl->Patch((uint16_t)adlchannel[c], ains.adl[c]);
        opl->Touch_Real((uint16_t)adlchannel[c], 127 * 127 * 100);
        opl->Pan((uint16_t)adlchannel[c], 0x30);
        opl->NoteOn((uint16_t)adlchannel[c], hertz);
    }
}

ADLMIDI_EXPORT void AdlInstrumentTester::NextGM(int offset)
{
    P->cur_gm = (P->cur_gm + 256 + (uint32_t)offset) & 0xFF;
    FindAdlList();
}

ADLMIDI_EXPORT void AdlInstrumentTester::NextAdl(int offset)
{
    //OPL3 *opl = P->opl;
    if(P->adl_ins_list.empty()) FindAdlList();
    const unsigned NumBanks = (unsigned)adl_getBanksCount();
    P->ins_idx = (uint32_t)((int32_t)P->ins_idx + (int32_t)P->adl_ins_list.size() + offset) % P->adl_ins_list.size();

#if 0
    UI.Color(15);
    std::fflush(stderr);
    std::printf("SELECTED G%c%d\t%s\n",
                cur_gm < 128 ? 'M' : 'P', cur_gm < 128 ? cur_gm + 1 : cur_gm - 128,
                "<-> select GM, ^v select ins, qwe play note");
    std::fflush(stdout);
    UI.Color(7);
    std::fflush(stderr);
#endif

    for(unsigned a = 0, n = P->adl_ins_list.size(); a < n; ++a)
    {
        const unsigned i = P->adl_ins_list[a];
        const adlinsdata2 ains(adlins[i]);

        char ToneIndication[8] = "   ";
        if(ains.tone)
        {
            /*if(ains.tone < 20)
                    snprintf(ToneIndication, 8, "+%-2d", ains.tone);
                else*/
            if(ains.tone < 128)
                snprintf(ToneIndication, 8, "=%-2d", ains.tone);
            else
                snprintf(ToneIndication, 8, "-%-2d", ains.tone - 128);
        }
        std::printf("%s%s%s%u\t",
                    ToneIndication,
                    ains.adl[0] != ains.adl[1] ? "[2]" : "   ",
                    (P->ins_idx == a) ? "->" : "\t",
                    i
                   );

        for(unsigned bankno = 0; bankno < NumBanks; ++bankno)
            if(banks[bankno][P->cur_gm] == i)
                std::printf(" %u", bankno);

        std::printf("\n");
    }
}

ADLMIDI_EXPORT bool AdlInstrumentTester::HandleInputChar(char ch)
{
    static const char notes[] = "zsxdcvgbhnjmq2w3er5t6y7ui9o0p";
    //                           c'd'ef'g'a'bC'D'EF'G'A'Bc'd'e
    switch(ch)
    {
    case '/':
    case 'H':
    case 'A':
        NextAdl(-1);
        break;
    case '*':
    case 'P':
    case 'B':
        NextAdl(+1);
        break;
    case '-':
    case 'K':
    case 'D':
        NextGM(-1);
        break;
    case '+':
    case 'M':
    case 'C':
        NextGM(+1);
        break;
    case 3:
#if !((!defined(__WIN32__) || defined(__CYGWIN__)) && !defined(__DJGPP__))
    case 27:
#endif
        return false;
    default:
        const char *p = std::strchr(notes, ch);
        if(p && *p)
            DoNote((int)(p - notes) - 12);
    }
    return true;
}

#endif /* ADLMIDI_DISABLE_CPP_EXTRAS */

// Implement the user map data structure.

bool MIDIplay::AdlChannel::users_empty() const
{
    return !users_first;
}

MIDIplay::AdlChannel::LocationData *MIDIplay::AdlChannel::users_find(Location loc)
{
    LocationData *user = NULL;
    for(LocationData *curr = users_first; !user && curr; curr = curr->next)
        if(curr->loc == loc)
            user = curr;
    return user;
}

MIDIplay::AdlChannel::LocationData *MIDIplay::AdlChannel::users_allocate()
{
    // remove free cells front
    LocationData *user = users_free_cells;
    if(!user)
        return NULL;
    users_free_cells = user->next;
    if(users_free_cells)
        users_free_cells->prev = NULL;
    // add to users front
    if(users_first)
        users_first->prev = user;
    user->prev = NULL;
    user->next = users_first;
    users_first = user;
    ++users_size;
    return user;
}

MIDIplay::AdlChannel::LocationData *MIDIplay::AdlChannel::users_find_or_create(Location loc)
{
    LocationData *user = users_find(loc);
    if(!user)
    {
        user = users_allocate();
        if(!user)
            return NULL;
        LocationData *prev = user->prev, *next = user->next;
        *user = LocationData();
        user->prev = prev;
        user->next = next;
        user->loc = loc;
    }
    return user;
}

MIDIplay::AdlChannel::LocationData *MIDIplay::AdlChannel::users_insert(const LocationData &x)
{
    LocationData *user = users_find(x.loc);
    if(!user)
    {
        user = users_allocate();
        if(!user)
            return NULL;
        LocationData *prev = user->prev, *next = user->next;
        *user = x;
        user->prev = prev;
        user->next = next;
    }
    return user;
}

void MIDIplay::AdlChannel::users_erase(LocationData *user)
{
    if(user->prev)
        user->prev->next = user->next;
    if(user->next)
        user->next->prev = user->prev;
    if(user == users_first)
        users_first = user->next;
    user->prev = NULL;
    user->next = users_free_cells;
    users_free_cells = user;
    --users_size;
}

void MIDIplay::AdlChannel::users_clear()
{
    users_first = NULL;
    users_free_cells = users_cells;
    users_size = 0;
    for(size_t i = 0; i < users_max; ++i)
    {
        users_cells[i].prev = (i > 0) ? &users_cells[i - 1] : NULL;
        users_cells[i].next = (i + 1 < users_max) ? &users_cells[i + 1] : NULL;
    }
}

void MIDIplay::AdlChannel::users_assign(const LocationData *users, size_t count)
{
    ADL_UNUSED(count);//Avoid warning for release builds
    assert(count <= users_max);
    if(users == users_first && users)
    {
        // self assignment
        assert(users_size == count);
        return;
    }
    users_clear();
    const LocationData *src_cell = users;
    // move to the last
    if(src_cell)
    {
        while(src_cell->next)
            src_cell = src_cell->next;
    }
    // push cell copies in reverse order
    while(src_cell)
    {
        LocationData *dst_cell = users_allocate();
        assert(dst_cell);
        LocationData *prev = dst_cell->prev, *next = dst_cell->next;
        *dst_cell = *src_cell;
        dst_cell->prev = prev;
        dst_cell->next = next;
        src_cell = src_cell->prev;
    }
    assert(users_size == count);
}
