/*
 * libADLMIDI is a free Software MIDI synthesizer library with OPL3 emulation
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

#ifndef MEASURER_H
#define MEASURER_H

#include <atomic>
#include <map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <array>

#include "progs_cache.h"

struct DurationInfo
{
    uint64_t    peak_amplitude_time;
    double      peak_amplitude_value;
    double      quarter_amplitude_time;
    double      begin_amplitude;
    double      interval;
    double      keyoff_out_time;
    int64_t     ms_sound_kon;
    int64_t     ms_sound_koff;
    bool        nosound;
    uint8_t     padding[7];
};

class Semaphore
{
public:
    Semaphore(int count_ = 0)
        : m_count(count_) {}

    inline void notify()
    {
        std::unique_lock<std::mutex> lock(mtx);
        m_count++;
        cv.notify_one();
    }

    inline void wait()
    {
        std::unique_lock<std::mutex> lock(mtx);
        while(m_count == 0)
        {
            cv.wait(lock);
        }
        m_count--;
    }

private:
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic_int m_count;
};

struct MeasureThreaded
{
    typedef std::array<int_fast32_t, 10> OperatorsKey;
    typedef std::map<OperatorsKey, DurationInfo> DurationInfoCacheX;

    MeasureThreaded();

    Semaphore           m_semaphore;
    std::mutex          m_durationInfo_mx;
    DurationInfoCacheX  m_durationInfo;
    std::atomic_bool    m_delete_tail;
    size_t              m_total = 0;
    std::atomic<size_t> m_done;
    std::atomic<size_t> m_cache_matches;

    void LoadCache(const char *fileName);
    bool SaveCache(const char *fileName);

    struct destData
    {
        destData()
        {
            m_works = true;
        }
        ~destData()
        {
            m_work.join();
        }
        MeasureThreaded  *myself;
        BanksDump *bd;
        BanksDump::InstrumentEntry *bd_ins;
        std::thread       m_work;
        std::atomic_bool  m_works;

        void start();
        static void callback(void *myself);
    };

    std::vector<destData *>  m_threads;

    void printProgress();
    void printFinal();

    void run(BanksDump &bd, BanksDump::InstrumentEntry &e);
    void waitAll();
};

class OPLChipBase;
extern DurationInfo MeasureDurations(const BanksDump &db, const BanksDump::InstrumentEntry &ins, OPLChipBase *chip);

#endif // MEASURER_H
