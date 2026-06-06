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

#ifndef ADLMIDI_DOS_H
#define ADLMIDI_DOS_H

/**
 * \file adlmidi_dos.h
 * \brief DOS-specific public API functions
 *
 * There are functions exclusive for the DOS support
 * and they are not available in other platforms.
 */

#include <stddef.h>

#if defined(__cplusplus) || (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L))
#include <stdint.h>
typedef uintptr_t       ADL_UIntPtr;
typedef intptr_t        ADL_SIntPtr;
#else
typedef unsigned long   ADL_UIntPtr;
typedef long            ADL_SIntPtr;
#endif
typedef unsigned char   ADL_Bool;
#define ADL_TRUE    1
#define ADL_FALSE   0

#ifdef __cplusplus
extern "C" {
#endif

/*! Typical void function pointer */
typedef void(*ADL_LockFnPtr)(void);


extern ADL_Bool adl_dpmi_lock_memory(void *address, size_t size);

extern ADL_Bool adl_dpmi_lock_memory_code(void *address, size_t size);

extern ADL_Bool adl_dpmi_lock_region(void *begin, void *end);

extern ADL_Bool adl_dpmi_lock_code_region(void *begin, void *end);

extern ADL_Bool adl_dpmi_lock_code_region_fn(ADL_LockFnPtr fn_begin, ADL_LockFnPtr fn_end);


extern ADL_Bool adl_dpmi_unlock_memory(void *address, size_t size);

extern ADL_Bool adl_dpmi_unlock_memory_code(void *address, size_t size);

extern ADL_Bool adl_dpmi_unlock_region(void *begin, void *end);

extern ADL_Bool adl_dpmi_unlock_code_region(void *begin, void *end);

extern ADL_Bool adl_dpmi_unlock_code_region_fn(ADL_LockFnPtr fn_begin, ADL_LockFnPtr fn_end);

#define adl_dpmi_lock(obj)\
    (adl_dpmi_lock_memory((void*)&obj, sizeof(obj)))

#define adl_dpmi_unlock(obj)\
    (adl_dpmi_unlock_memory((void*)&obj, sizeof(obj)))

#ifdef __cplusplus
}
#endif

/* C++-only extras */
#ifdef __cplusplus

template<class T>
class DPMILocker
{
    T *m_self;

public:
    explicit DPMILocker(T *ptr)
    {
        /* Lock data */
        adl_dpmi_lock_memory(ptr, sizeof(T));
    }

    ~DPMILocker()
    {
        adl_dpmi_unlock_memory(this, sizeof(T));
    }
};

template<class T>
void adl_dpmi_lock_class_code()
{
    void (T::* lock_begin)() = &T::dpmi_lock_begin;
    void (T::* lock_end)() = &T::dpmi_lock_end;
    adl_dpmi_lock_code_region((void*&)lock_begin, (void*&)lock_end);
}

template<class T>
void adl_dpmi_unlock_class_code()
{
    void (T::* lock_begin)() = &T::dpmi_lock_begin;
    void (T::* lock_end)() = &T::dpmi_lock_end;
    adl_dpmi_unlock_code_region((void*&)lock_begin, (void*&)lock_end);
}

#endif /* __cplusplus */

#endif /* ADLMIDI_DOS_H */
