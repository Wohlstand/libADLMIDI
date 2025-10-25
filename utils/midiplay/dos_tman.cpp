/*
 * Task manager with timer for DOS
 *
 * Copyright (c) 2017-2025 Vitaly Novichkov (Wohlstand)
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

#include <dpmi.h>
#include <pc.h>
#include <go32.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "dos_tman.h"

#define TIMER_IRQ 8

static _go32_dpmi_seginfo s_oldIRQ8, s_newIRQ8;
DosTaskman *DosTaskman::self = NULL;

static __attribute__((always_inline)) inline uint32_t disableInterrupts(void)
{
    uint32_t a;
    asm
    (
        "pushfl \n"
        "popl %0 \n"
        "cli"
        : "=r" (a)
    );

    return a;
}

static __attribute__((always_inline)) inline void restoreInterrupts(uint32_t flags)
{
    asm
    (
        "pushl %0 \n"
        "popfl"
        :
        : "r" (flags)
    );
}

#define replaceInterrupt(oldIRQ, newIRQ, vector, handler) \
    _go32_dpmi_get_protected_mode_interrupt_vector(vector, &oldIRQ); \
\
    newIRQ.pm_selector = _go32_my_cs(); \
    newIRQ.pm_offset = (int32_t)handler; \
    _go32_dpmi_allocate_iret_wrapper(&newIRQ); \
    _go32_dpmi_set_protected_mode_interrupt_vector(vector, &newIRQ)

#define restoreInterrupt( vector, oldIRQ, newIRQ ) \
    _go32_dpmi_set_protected_mode_interrupt_vector(vector, &oldIRQ); \
    _go32_dpmi_free_iret_wrapper(&newIRQ)

#define _chain_intr( oldIRQ ) \
    asm \
    ( \
        "cli \n" \
        "pushfl \n" \
        "lcall *%0" \
        : \
        : "m" (oldIRQ.pm_offset) \
    )

void DosTaskman::process()
{
    for(std::list<DosTask>::iterator it = self->m_tasks.begin(); it != self->m_tasks.end(); ++it)
    {
        DosTask &t = *it;
        if(t.active)
        {
            t.count += self->m_timerRate;
            while(t.count >= t.rate_real)
            {
                t.count -= t.rate_real;
                t.callback(&t);
            }
        }
    }

    self->m_counter += self->m_timerRate;
    if(self->m_counter > 0xffffL)
    {
        self->m_counter &= 0xffff;
        _chain_intr(s_oldIRQ8);
    }

    outportb(0x20, 0x20);
}

DosTaskman::DosTaskman()
{
    m_running = false;
    m_timerRate  = 0x10000L;
    m_counter = 0;

    if(self)
    {
        fprintf(stderr, "Only one sample of DosTaskman can exist!\n");
        exit(1);
    }

    self = this;
}

DosTaskman::~DosTaskman()
{
    close();
    self = NULL;
}

void DosTaskman::setClockRate(long frequency)
{
    uint32_t flags = disableInterrupts();

    m_timerRate = (frequency > 0 && frequency < 0x10000L) ? frequency : 0x10000L;
    outportb(0x43, 0x36);
    outportb(0x40, m_timerRate);
    outportb(0x40, m_timerRate >> 8);

    restoreInterrupts(flags);
}

void DosTaskman::setTimer(long tickBase)
{
    long speed = DOS_TASK_CLOCK_BASE / tickBase;

    if(speed < m_timerRate)
        setClockRate(speed);
}

DosTaskman::DosTask *DosTaskman::addTask(void (*callback)(DosTaskman::DosTask *), int freq, int priority, void *userData)
{
    DosTask task;

    if(!m_running)
        start();

    task.callback = callback;
    task.data = userData;
    task.freq = freq;
    task.rate_real = adjustTimer(freq);
    task.active = false;
    task.count = 0;
    task.priority = priority;

    return addTask(task);
}

void DosTaskman::changeTaskRate(DosTaskman::DosTask *task, int freq)
{
    uint32_t flags = disableInterrupts();

    for(std::list<DosTask>::iterator it = m_tasks.begin(); it != m_tasks.end(); ++it)
    {
        DosTask *t = &*it;
        if(t == task)
        {
            t->rate_real = adjustTimer(freq);
            setClockToMax();
            break;
        }
    }

    restoreInterrupts(flags);
}


long DosTaskman::adjustTimer(long base)
{
    long rate = DOS_TASK_CLOCK_BASE / base;

    if(rate < m_timerRate)
        setClockRate(rate);

    return rate;
}

void DosTaskman::setClockToMax()
{
    uint32_t flags = disableInterrupts();
    long maxRate = 0x10000L;

    for(std::list<DosTask>::iterator it = m_tasks.begin(); it != m_tasks.end(); ++it)
    {
        DosTask &t = *it;
        if(t.rate_real < maxRate)
            maxRate = t.rate_real;
    }

    if(maxRate != m_timerRate)
        setClockRate(maxRate);

    restoreInterrupts(flags);
}

void DosTaskman::resetClockRate()
{
    uint32_t flags = disableInterrupts();
    outportb(0x43, 0x34);
    outportb(0x40, 0);
    outportb(0x40, 0);
    restoreInterrupts(flags);
}

void DosTaskman::start()
{
    if(m_running)
        return;

    m_timerRate  = 0x10000L;
    m_counter = 0;

    replaceInterrupt(s_oldIRQ8, s_newIRQ8, TIMER_IRQ, DosTaskman::process);

    m_running = true;
}

void DosTaskman::close()
{
    if(!m_running)
        return;

    clearTasks();
    resetClockRate();
    restoreInterrupt(TIMER_IRQ, s_oldIRQ8, s_newIRQ8);

    m_running = false;

}

void DosTaskman::dispatch()
{
    uint32_t flags = disableInterrupts();

    for(std::list<DosTask>::iterator it = m_tasks.begin(); it != m_tasks.end(); ++it)
    {
        DosTask &t = *it;
        t.active = true;
    }

    restoreInterrupts(flags);
}

bool DosTaskman::terminate(DosTask *task)
{
    uint32_t flags = disableInterrupts();

    for(std::list<DosTask>::iterator it = m_tasks.begin(); it != m_tasks.end(); ++it)
    {
        DosTask *t = &*it;
        if(t == task)
        {
            m_tasks.erase(it);
            setClockToMax();
            restoreInterrupts(flags);
            return true;
        }
    }

    restoreInterrupts(flags);

    return false;
}

void DosTaskman::clearTasks()
{
    uint32_t flags = disableInterrupts();
    m_tasks.clear();
    restoreInterrupts(flags);
}

DosTaskman::DosTask *DosTaskman::addTask(DosTaskman::DosTask &task)
{
    std::list<DosTask>::iterator it = m_tasks.begin();

    for(; it != m_tasks.end(); ++it)
    {
        DosTask &t = *it;
        if(t.priority < task.priority)
            return &*m_tasks.insert(it, task);
    }

    return &*m_tasks.insert(m_tasks.end(), task);
}
