/*
 * ADLMIDI Player is a free MIDI player based on a libADLMIDI,
 * a Software MIDI synthesizer library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2026 Vitaly Novichkov <admin@wohlnet.ru>
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

#include <cstdio>

#include "playback.h"
#include "../dev_setup.h"
#include "../misc.h"
#include "../time_counter.h"
#include "wave_writer.h"


int runWaveOutLoopLoop(ADL_MIDIPlayer *myDevice, const std::string &musPath, const AudioOutputSpec &obtained, unsigned sampleRate)
{
    std::string wave_out = musPath + ".wav";

    fillAudioFormat(obtained);

    s_fprintf(stdout, " - Output WAV spec (format=%s,samples=%d,rate=%u,channels=%u);\n",
              audio_format_to_str(obtained.format, obtained.is_msb), obtained.samples, obtained.freq, obtained.channels);

    int wav_format = obtained.format == ADLMIDI_SampleType_F32 ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
    int wav_has_sign = obtained.format != ADLMIDI_SampleType_U8 && obtained.format != ADLMIDI_SampleType_U16;

    void *wav_ctx = ctx_wave_open(obtained.channels,
                                  static_cast<long>(sampleRate),
                                  g_audioFormat.containerSize,
                                  wav_format,
                                  wav_has_sign,
                                  (int)obtained.is_msb,
                                  wave_out.c_str()
                                  );

    if(wav_ctx)
    {
        uint8_t buff[16384];

        s_fprintf(stdout, " - Recording WAV file %s...\n", wave_out.c_str());
        s_fprintf(stdout, "\n==========================================\n");
        flushout(stdout);

        setCursorVisibility(false);

        while(!stop)
        {
            size_t got = (size_t)adl_playFormat(myDevice, 4096,
                                                 buff,
                                                 buff + g_audioFormat.containerSize,
                                                 &g_audioFormat) * g_audioFormat.containerSize;
            if(got <= 0)
                break;

            applyGain(buff, got);

            ctx_wave_write(wav_ctx, buff, static_cast<long>(got));

            s_timeCounter.printProgress(adl_positionTell(myDevice));
        }

        setCursorVisibility(true);

        ctx_wave_close(wav_ctx);
        s_timeCounter.clearLine();

        if(stop)
            s_fprintf(stdout, "Interrupted! Recorded WAV is incomplete, but playable!\n");
        else
            s_fprintf(stdout, "Completed!\n");
        flushout(stdout);
    }
    else
    {
        adl_close(myDevice);
        return 1;
    }

    return 0;
}
