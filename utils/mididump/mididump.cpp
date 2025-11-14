#include <cstring>
#include "midi_sequencer_impl.hpp"
#include "utf8main.h" // IWYU pragma: keep

// DUMMY!


static void rtNoteOn(void *, uint8_t, uint8_t, uint8_t)
{}

static void rtNoteOff(void *, uint8_t, uint8_t)
{}

static void rtNoteAfterTouch(void *, uint8_t, uint8_t, uint8_t)
{}

static void rtChannelAfterTouch(void *, uint8_t, uint8_t)
{}

static void rtControllerChange(void *, uint8_t, uint8_t, uint8_t)
{}

static void rtPatchChange(void *, uint8_t, uint8_t)
{}

static void rtPitchBend(void *, uint8_t, uint8_t, uint8_t)
{}

static void rtSysEx(void *, const uint8_t *, size_t)
{}


/* NonStandard calls */
static void rtRawOPL(void *, uint8_t, uint8_t)
{}

static void rtDeviceSwitch(void *, size_t, const char *, size_t)
{}

static size_t rtCurrentDevice(void *, size_t)
{
    return 0;
}

int main(int argc, char **argv)
{
    BW_MidiRtInterface iface;
    BW_MidiSequencer midiSeq;

    std::memset(&iface, 0, sizeof(iface));

    /* MIDI Real-Time calls */
    iface.rtUserData = NULL;
    iface.rt_noteOn  = rtNoteOn;
    iface.rt_noteOff = rtNoteOff;
    iface.rt_noteAfterTouch = rtNoteAfterTouch;
    iface.rt_channelAfterTouch = rtChannelAfterTouch;
    iface.rt_controllerChange = rtControllerChange;
    iface.rt_patchChange = rtPatchChange;
    iface.rt_pitchBend = rtPitchBend;
    iface.rt_systemExclusive = rtSysEx;

    /* NonStandard calls */
    iface.rt_rawOPL = rtRawOPL;
    iface.rt_deviceSwitch = rtDeviceSwitch;
    iface.rt_currentDevice = rtCurrentDevice;

    midiSeq.setInterface(&iface);

    if(argc < 2)
    {
        std::fprintf(stderr, "Too few arguments!\n\nSyntax: %s <path-to-file>\n\n", argv[0]);
        std::fflush(stderr);
        return 1;
    }

    if(!midiSeq.loadMIDI(argv[1]))
    {
        std::fprintf(stderr, "\n\nFailed to load the file: %s\n\n", midiSeq.getErrorString());
        std::fflush(stderr);
        return 1;
    }

    if(!midiSeq.debugDumpContents(stdout))
    {
        std::fprintf(stderr, "\n\nFailed to dump the output: %s\n\n", midiSeq.getErrorString());
        std::fflush(stderr);
        return 1;
    }

    return 0;
}
