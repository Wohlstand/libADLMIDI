/*
 * Task manager with timer for DOS
 *
 * Copyright (c) 2025-2026 Vitaly Novichkov (Wohlstand)
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
#ifndef DOS_TMAN_H
#define DOS_TMAN_H

#include <stddef.h>
#include <stdio.h>

#define DOS_TASK_CLOCK_BASE 1192030L

struct DosTask;

class DosTaskman
{
    void dpmi_lock_begin() {}
    bool m_running;
    static DosTaskman *self;
    volatile long m_timerRate;
    volatile long m_counter;
    volatile long m_clock;
    volatile bool m_suspend;
    volatile bool m_insideInterrupt;

    static void process();

    long adjustTimer(long base);
    void setClockToMax();

    void resetClockRate();

public:
    DosTaskman();
    ~DosTaskman();

    static void* task_getData(DosTask *task);
    static long task_getFreq(DosTask *task);
    static long task_getCount(DosTask *task);
    static long task_getRate(DosTask *task);

    static bool isInsideInterrupt();
    static int reserve_fprintf(FILE *stream, const char *format, va_list args);
    static void reserve_flush(FILE *stream);

    void setClockRate(long frequency);
    void setTimer(long tickBase);

    static unsigned long getCurTicks();
    unsigned long getCurClockRate() const;

    static void suspend();
    static void resume();

    /*!
     * \brief Add a callback function as a task
     * \param freq How many times callback function should be called
     * \param priority Priority in the tasks list, otherwise will be inserted into end of list
     * \param userData Extra user data that callback should process
     * \return pointer to the created task entry, otherwise NULL on error
     */
    DosTask *addTask(void(*callback)(DosTask*), int freq, int priority, void *userData);

    /*!
     * \brief Change the frequency of the task calling
     * \param task Pointer to previously added task
     * \param freq New frequency value in herz
     */
    void changeTaskRate(DosTask *task, int freq);

    /*!
     * \brief Start the processingtask manager
     *
     * The hook for the interrupt 8 will be installed
     */
    void start();

    /*!
     * \brief Stop the processing of task manager
     *
     * The hook for the interrupt 8 will be removed and all running tasks will be removed
     */
    void close();

    /*!
     * \brief Enable all currently disabled tasks
     */
    void dispatch();

    bool terminate(DosTask *task);

private:
    void clearTasks();
    DosTask *addTask(DosTask &task);

    DosTask *m_tasks_begin;
    DosTask *m_tasks_end;
    size_t   m_tasks_size;

    DosTask *task_erase(DosTask *it);
    DosTask *task_insert(DosTask *pos, DosTask *o);
    void task_clean();

    DosTask *task_make();
    DosTask *task_make_at(DosTask *pos);

    void dpmi_lock_end() {}
};

#endif // DOS_TMAN_H
