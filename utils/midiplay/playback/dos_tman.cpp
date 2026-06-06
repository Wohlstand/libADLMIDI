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

#include <dpmi.h>
#include <pc.h>
#include <go32.h>
#include <sys/farptr.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "dos_tman.h"
#include "dos_tman_c.h"

#include <adlmidi_dos.h>

#define TIMER_IRQ               8
#define BIOS_TIMER()            _farpeekl(_dos_ds, 0x46C)
#define BIOS_TIMER_OVERFLOW()   _farpeekl(_dos_ds, 0x470)
#define BIOS_MAX_TICKS          0x1800B0

static _go32_dpmi_seginfo s_oldIRQ8, s_newIRQ8;
DosTaskman *DosTaskman::self = NULL;

static char s_snpirntf[300];
static char s_reserve_print_buffer_out[2048] = "";
static char s_reserve_print_buffer_err[2048] = "";

void dos_taskman_dpmi_lock_head(void) {}
void dos_taskman_dpmi_lock_tail(void);

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
        "popfl \n"
        "sti"
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

struct DosTask
{
    /* List */
    struct DosTask *prev;
    struct DosTask *next;

    void (*callback)(struct DosTask*);
    void *data;
    long freq;
    long rate_real;
    volatile long count;
    int priority;
    bool active;
};


void DosTaskman::process()
{
    self->m_insideInterrupt = true;

    ++self->m_clock;
    if(self->m_clock >= 0x10000000)
        self->m_clock = 0;

    for(DosTask *it = self->m_tasks_begin; it != NULL; it = it->next)
    {
        DosTask &t = *it;
        if(t.active)
        {
            t.count += self->m_timerRate;
            while(!self->m_suspend && t.count >= t.rate_real)
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

    self->m_insideInterrupt = false;
}

DosTaskman::DosTaskman()
{
    m_tasks_begin = NULL;
    m_tasks_end = NULL;
    m_tasks_size = 0;

    m_suspend = false;
    m_running = false;
    m_insideInterrupt = false;
    m_timerRate  = 0x10000L;
    m_counter = 0;
    m_clock = 0;

    if(self)
    {
        fprintf(stderr, "Only one sample of DosTaskman can exist!\n");
        fflush(stderr);
        exit(1);
    }

    self = this;

    void (DosTaskman::* lock_begin)() = &DosTaskman::dpmi_lock_begin;
    void (DosTaskman::* lock_end)() = &DosTaskman::dpmi_lock_end;
    adl_dpmi_lock_region((void*&)lock_begin, (void*&)lock_end);

    // setSystemClockRate();
    adl_dpmi_lock_code_region_fn(dos_taskman_dpmi_lock_head, dos_taskman_dpmi_lock_tail);
    adl_dpmi_lock_memory(this, sizeof(DosTaskman));
    adl_dpmi_lock_memory(&s_oldIRQ8, sizeof(s_oldIRQ8));
    adl_dpmi_lock_memory(&s_newIRQ8, sizeof(s_newIRQ8));
    adl_dpmi_lock_memory(&self, sizeof(DosTaskman*));
    adl_dpmi_lock_memory(s_snpirntf, sizeof(s_snpirntf));
    adl_dpmi_lock_memory(s_reserve_print_buffer_out, sizeof(s_reserve_print_buffer_out));
    adl_dpmi_lock_memory(s_reserve_print_buffer_err, sizeof(s_reserve_print_buffer_err));
}

DosTaskman::~DosTaskman()
{
    close();
    self = NULL;

    adl_dpmi_unlock_code_region_fn(dos_taskman_dpmi_lock_head, dos_taskman_dpmi_lock_tail);

    adl_dpmi_unlock_memory(this, sizeof(DosTaskman));
    void (*c_lock_begin)() = &dos_taskman_dpmi_lock_head;
    void (*c_lock_end)() = &dos_taskman_dpmi_lock_tail;
    adl_dpmi_unlock_region((void*&)c_lock_begin, (void*&)c_lock_end);
    adl_dpmi_unlock_memory(this, sizeof(DosTaskman));
    adl_dpmi_unlock_memory(&s_oldIRQ8, sizeof(s_oldIRQ8));
    adl_dpmi_unlock_memory(&s_newIRQ8, sizeof(s_newIRQ8));
    adl_dpmi_unlock_memory(&self, sizeof(DosTaskman*));
    adl_dpmi_unlock_memory(s_snpirntf, sizeof(s_snpirntf));
    adl_dpmi_unlock_memory(s_reserve_print_buffer_out, sizeof(s_reserve_print_buffer_out));
    adl_dpmi_unlock_memory(s_reserve_print_buffer_err, sizeof(s_reserve_print_buffer_err));
}

void *DosTaskman::task_getData(DosTask *task)
{
    return task->data;
}

long DosTaskman::task_getFreq(DosTask *task)
{
    return task->freq;
}

long DosTaskman::task_getCount(DosTask *task)
{
    return task->count;
}

long DosTaskman::task_getRate(DosTask *task)
{
    return task->rate_real;
}

bool DosTaskman::isInsideInterrupt()
{
    if(self)
        return self->m_insideInterrupt;
    else
        return false;
}

int DosTaskman::reserve_fprintf(FILE *stream, const char *format, va_list args)
{
    int ret = vsnprintf(s_snpirntf, 300, format, args);

    if(stream == stderr)
        strncat(s_reserve_print_buffer_err, s_snpirntf, 2047);
    else if(stream == stdout)
        strncat(s_reserve_print_buffer_out, s_snpirntf, 2047);

    return ret;
}

void DosTaskman::reserve_flush(FILE *stream)
{
    if(stream == stderr && s_reserve_print_buffer_err[0])
    {
        fprintf(stderr, "%s", s_reserve_print_buffer_err);
        s_reserve_print_buffer_err[0] = 0;
    }
    else if(stream == stdout && s_reserve_print_buffer_out[0])
    {
        fprintf(stdout, "%s", s_reserve_print_buffer_out);
        s_reserve_print_buffer_out[0] = 0;
    }
}

void DosTaskman::setClockRate(long frequency)
{
    uint32_t flags = disableInterrupts();

    m_timerRate = (frequency > 0 && frequency < 0x10000L) ? frequency : 0x10000L;
    outportb(0x43, 0x36);
    outportb(0x40, m_timerRate & 0xFF);
    outportb(0x40, m_timerRate >> 8);

    restoreInterrupts(flags);
}

void DosTaskman::setTimer(long tickBase)
{
    long speed = DOS_TASK_CLOCK_BASE / tickBase;

    if(speed < m_timerRate)
        setClockRate(speed);
}

void DosTaskman::suspend()
{
    if(self)
        self->m_suspend = true;
}

void DosTaskman::resume()
{
    if(self)
        self->m_suspend = false;
}

unsigned long DosTaskman::getCurTicks()
{
    if(self)
        return self->m_clock;
    else
        return 0;
}

unsigned long DosTaskman::getCurClockRate() const
{
    return m_timerRate;
}

DosTask *DosTaskman::addTask(void (*callback)(DosTask *), int freq, int priority, void *userData)
{
    DosTask task;

    memset(&task, 0, sizeof(DosTask));

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

void DosTaskman::changeTaskRate(DosTask *task, int freq)
{
    uint32_t flags = disableInterrupts();

    for(DosTask *t = m_tasks_begin; t != NULL; t = t->next)
    {
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

    for(DosTask *it = m_tasks_begin; it != NULL; it = it->next)
    {
        if(it->rate_real < maxRate)
            maxRate = it->rate_real;
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

    for(DosTask *it = m_tasks_begin; it != NULL; it = it->next)
        it->active = true;

    restoreInterrupts(flags);
}

bool DosTaskman::terminate(DosTask *task)
{
    uint32_t flags = disableInterrupts();

    for(DosTask *it = m_tasks_begin; it != NULL; it = it->next)
    {
        if(it == task)
        {
            task_erase(it);
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
    task_clean();
    restoreInterrupts(flags);
}

DosTask *DosTaskman::addTask(DosTask &task)
{
    DosTask *it = m_tasks_begin;

    for(; it != NULL; it = it->next)
    {
        if(it->priority < task.priority)
            return task_insert(it, &task);
    }

    return task_insert(m_tasks_end, &task);
}

DosTask *DosTaskman::task_erase(DosTask *it)
{
    DosTask *ret = NULL, *old_prev, *old_next;

    old_prev = it->prev;
    old_next = it->next;

    if(old_prev)
        old_prev->next = old_next;
    else
        m_tasks_begin = old_next;

    if(old_next)
        ret = old_next->prev = old_prev;
    else
        m_tasks_end = old_prev;

    adl_dpmi_unlock_memory(it, sizeof(DosTask));
    free(it);

    --m_tasks_size;

    return ret;
}

DosTask *DosTaskman::task_insert(DosTask *pos, DosTask *o)
{
    DosTask *dst = task_make_at(pos);

    if(!dst)
        return NULL;

    memcpy(dst, o, sizeof(DosTask));
    return dst;
}

void DosTaskman::task_clean()
{
    if(m_tasks_begin && m_tasks_end && m_tasks_size > 0)
    {
        DosTask *cur;

        while(m_tasks_size > 0)
        {
            cur = m_tasks_begin;
            m_tasks_begin = cur->next;

            adl_dpmi_unlock_memory(cur, sizeof(DosTask));
            free(cur);
            --m_tasks_size;
        }

        m_tasks_end = NULL;
    }
}

DosTask *DosTaskman::task_make()
{
    DosTask *prev_last = m_tasks_end;

    if(m_tasks_size == 0)
    {
        m_tasks_end = m_tasks_begin = (DosTask*)malloc(sizeof(DosTask));

        if(!m_tasks_begin)
            return NULL; /* Out of Memory */

        memset(m_tasks_end, 0, sizeof(DosTask));
    }
    else
    {
        if(!m_tasks_end)
            return NULL; //! ERROR!

        m_tasks_end->next = (DosTask*)malloc(sizeof(DosTask));
        memset(m_tasks_end->next, 0, sizeof(DosTask));
        m_tasks_end->next->prev = m_tasks_end;
        m_tasks_end = m_tasks_end->next;
    }

    adl_dpmi_lock_memory(m_tasks_end, sizeof(DosTask));

    m_tasks_end->prev = prev_last;
    ++m_tasks_size;

    return m_tasks_end;
}

DosTask *DosTaskman::task_make_at(DosTask *pos)
{
    DosTask *cur = NULL;

    if(!pos || m_tasks_size == 0)
        return task_make(); // Empty list!

    cur = (DosTask*)malloc(sizeof(DosTask));
    memset(cur, 0, sizeof(DosTask));

    if(pos->next)
        pos->next->prev = cur;

    cur->next = pos->next;
    pos->next = cur;
    cur->prev = pos;

    adl_dpmi_lock_memory(cur, sizeof(DosTask));
    memset(cur, 0, sizeof(DosTask));
    ++m_tasks_size;

    return cur;
}


/* C bindings */

void dos_taskman_suspend(void)
{
    DosTaskman::suspend();
}

void dos_taskman_resume(void)
{
    DosTaskman::resume();
}

void dos_taskman_reserve_flush(FILE *stream)
{
    DosTaskman::reserve_flush(stream);
}

int dos_taskman_is_inside_interrupt(void)
{
    return DosTaskman::isInsideInterrupt() ? 1 : 0;
}

int dos_taskman_reserve_fprintf(FILE *stream, const char *format, va_list args)
{
    return DosTaskman::reserve_fprintf(stream, format, args);
}

void dos_taskman_dpmi_lock_tail(void) {}
