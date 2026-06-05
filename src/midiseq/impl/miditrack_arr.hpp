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

#pragma once
#if !defined(BW_MIDISEQ_MIDITRACK_ARR_HPP)
#define BW_MIDISEQ_MIDITRACK_ARR_HPP

#include <stddef.h>
#include <cstdlib>
#if defined(__DJGPP__)
#include "dpmi_alloc.hpp"
#endif

template<class T, bool is_class=false>
struct miditrack_arr
{
#if defined(__DJGPP__)
    void dpmi_lock_begin() {}
#endif

    T *data;
    size_t size;
    size_t capacity;

    miditrack_arr() :
        data(NULL),
        size(0),
        capacity(0)
    {}

    void move_to(miditrack_arr<T> &dst)
    {
        dst.data = data;
        dst.size = size;
        dst.capacity = capacity;

        data = NULL;
        size = 0;
        capacity = 0;
    }

    virtual ~miditrack_arr()
    {
        clear();
    }

    T *begin()
    {
        return data;
    }

    T *end()
    {
        return data + size;
    }

    const T *begin() const
    {
        return data;
    }

    const T *end() const
    {
        return data + size;
    }

    T *back()
    {
        if(!data)
            return NULL;

        return data + size - 1;
    }

    const T *back() const
    {
        if(!data)
            return NULL;

        return data + size - 1;
    }


    bool empty() const
    {
        return size == 0;
    }

    void reserve_extend(size_t count)
    {
        if(capacity == 0)
        {
            data = (T*)std::malloc(count * sizeof(T));
            capacity = count;
        }
        else
        {
#ifdef ENABLE_HW_OPL_DOS
            dpmi_allocator_impl::dpmi_unlock_memory(data, capacity * sizeof(T));
#endif
            // if(is_class)
            // {
            T *old_data = data;
            data = (T*)std::malloc((capacity + count) * sizeof(T));

            // Copy old data
            for(size_t i = 0; i < size; ++i)
                new (data + i) T(old_data[i]);

            // Delete old data
            for(size_t i = 0; i < size; ++i)
                old_data[i].~T();

            std::free(old_data);
            // }
            // else
            //     data = (T*)std::realloc(data, (capacity + count) * sizeof(T));

            capacity += count;
        }

#ifdef ENABLE_HW_OPL_DOS
        dpmi_allocator_impl::dpmi_lock_memory(data, capacity * sizeof(T));
#endif
    }

    void reserve(size_t count)
    {
        if(count <= capacity)
            return;

        reserve_extend(count - capacity);
    }

    void push_back(const T &value)
    {
        if(size + 1 >= capacity)
            reserve_extend(4096);

        if(is_class)
            new (data + size) T(value);
        else
            data[size] = value;

        ++size;
    }

    void push_back_list(const T*in_data, size_t count)
    {
        if(size + count >= capacity)
            reserve_extend(count + 1024);

        for(size_t i = 0; i < count; ++i)
        {
            if(is_class)
                new (data + (size++)) T(in_data[i]);
            else
                data[size++] = in_data[i];
        }
    }

    void fill(const T &value)
    {
        for(size_t i = 0; i < size; ++i)
            data[i] = value;
    }

    void resize_fill(size_t count, const T &value)
    {
        if(data)
            clear();

        data = (T*)std::malloc(count * sizeof(T));
        capacity = size = count;


#ifdef ENABLE_HW_OPL_DOS
        dpmi_allocator_impl::dpmi_lock_memory(data, count * sizeof(T));
#endif

        for(size_t i = 0; i < size; ++i)
        {
            if(is_class)
                new (data + i) T(value);
            else
                data[i] = value;
        }
    }

    void resize_clean(size_t count)
    {
        if(data)
            clear();

        data = (T*)std::malloc(count * sizeof(T));
        capacity = size = count;
#ifdef ENABLE_HW_OPL_DOS
        dpmi_allocator_impl::dpmi_lock_memory(data, count * sizeof(T));
#endif

        if(is_class)
        {
            for(size_t i = 0; i < size; ++i)
                new (data + i) T();
        }
    }

    void resize(size_t count)
    {
        if(count == 0)
            clear();
        else if(!data)
        {
            data = (T*)std::malloc(count * sizeof(T));
            capacity = size = count;
#ifdef ENABLE_HW_OPL_DOS
            dpmi_allocator_impl::dpmi_lock_memory(data, count * sizeof(T));
#endif
            if(is_class)
            {
                for(size_t i = 0; i < size; ++i)
                    new (data + i) T();
            }
        }
        else if(count > size)
        {
            expand(count);
        }
        else if(count < size)
        {
            if(is_class)
            {
                for(size_t i = count; i < size; ++i)
                    data[i].~T();
            }

            size = count;
        }
    }

    void expand(size_t count)
    {
        T *old_data = data;

        if(size > count)
            return; // Nothing to expand!

        if(size + count <= capacity)
        {
            // If we fit in capacity
            // Just initialize new data
            for(size_t i = size; i < count; ++i)
                new (data + i) T();

            size = count;

            return;
        }

        data = (T*)std::malloc(count * sizeof(T));

#ifdef ENABLE_HW_OPL_DOS
        dpmi_allocator_impl::dpmi_lock_memory(data, count * sizeof(T));
#endif

        if(is_class)
        {
            // Copy old data
            for(size_t i = 0; i < size; ++i)
                new (data + i) T(old_data[i]);

            // Initialize new data
            for(size_t i = size; i < count; ++i)
                new (data + i) T();

            // Delete old data
            for(size_t i = 0; i < size; ++i)
                old_data[i].~T();
        }
        else
        {
            // Copy old data
            for(size_t i = 0; i < size; ++i)
                data[i] = old_data[i];
        }

#ifdef ENABLE_HW_OPL_DOS
        dpmi_allocator_impl::dpmi_unlock_memory(old_data, size * sizeof(T));
#endif
        std::free(old_data);

        capacity = size = count;
    }

    void clear()
    {
        if(data)
        {
            if(is_class)
            {
                for(size_t i = 0; i < size; ++i)
                    data[i].~T();
            }

#ifdef ENABLE_HW_OPL_DOS
            dpmi_allocator_impl::dpmi_unlock_memory(data, size * sizeof(T));
#endif
            std::free(data);
        }

        data = NULL;
        capacity = size = 0;
    }

    T &operator[](size_t i)
    {
        return data[i];
    }

    const T &operator[](size_t i) const
    {
        return data[i];
    }

#if defined(__DJGPP__)
    void dpmi_lock_end() {}
#endif
};

#endif /* BW_MIDISEQ_MIDITRACK_ARR_HPP */
