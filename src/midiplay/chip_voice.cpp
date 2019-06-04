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

#include "chip_voice.hpp"
#include <algorithm>

void ChipChannel::addAge(int64_t us)
{
    const int64_t neg = 1000 * static_cast<int64_t>(-0x1FFFFFFFl);
    if(users.empty())
    {
        koff_time_until_neglible_us = std::max(koff_time_until_neglible_us - us, neg);
        if(koff_time_until_neglible_us < 0)
            koff_time_until_neglible_us = 0;
    }
    else
    {
        koff_time_until_neglible_us = 0;
        for(users_iterator i = users.begin(); !i.is_end(); ++i)
        {
            LocationData &d = i->value;
            if(!d.fixed_sustain)
                d.kon_time_until_neglible_us = std::max(d.kon_time_until_neglible_us - us, neg);
            d.vibdelay_us += us;
        }
    }
}
