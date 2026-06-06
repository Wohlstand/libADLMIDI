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
#ifndef MIDITRAC_LIST_HPP
#define MIDITRAC_LIST_HPP

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if defined(__DJGPP__)
#   include "dpmi_alloc.hpp"
#endif

template<class T>
struct TrackQueueList_t
{
    void dpmi_lock_begin() {}

    struct Leaf_t
    {
        Leaf_t *prev;
        Leaf_t *next;
        T data;
    };

    Leaf_t *m_begin;
    Leaf_t *m_last;
    size_t m_size;

    size_t size() const
    {
        return m_size;
    }

    bool empty() const
    {
        return m_size == 0;
    }

    TrackQueueList_t() :
        m_begin(NULL), m_last(NULL), m_size(0)
    {}

    ~TrackQueueList_t()
    {
        clean();
    }

    T &make()
    {
        Leaf_t *prev_last = m_last;

        if(!m_begin && !m_last)
        {
            m_last = m_begin = (Leaf_t*)malloc(sizeof(Leaf_t));
            memset(m_last, 0, sizeof(Leaf_t));
        }
        else
        {
            m_last->next = (Leaf_t*)malloc(sizeof(Leaf_t));
            memset(m_last->next, 0, sizeof(Leaf_t));
            m_last->next->prev = m_last;
            m_last = m_last->next;
        }

#if defined(__DJGPP__)
        dpmi_allocator_impl::dpmi_lock_memory(m_last, sizeof(Leaf_t));
#endif

        m_last->prev = prev_last;
        ++m_size;

        return m_last->data;
    }

    T &makeAt(Leaf_t *pos)
    {
        Leaf_t *cur = NULL;

        if(!pos || (!m_begin && !m_last))
            return make(); // Empty list!

        cur = (Leaf_t*)malloc(sizeof(Leaf_t));
        memset(cur, 0, sizeof(Leaf_t));

        if(pos->next)
            pos->next->prev = cur;

        cur->next = pos->next;
        pos->next = cur;
        cur->prev = pos;

#if defined(__DJGPP__)
        dpmi_allocator_impl::dpmi_lock_memory(cur, sizeof(Leaf_t));
#endif

        memset(cur, 0, sizeof(Leaf_t));
        ++m_size;

        return cur->data;
    }

    T &insert(Leaf_t *pos, const T &o)
    {
        T &dst = makeAt(pos);
        memcpy(&dst, &o, sizeof(T));
        return dst;
    }

    void push_back(const T &o)
    {
        T &dst = make();
        memcpy(&dst, &o, sizeof(T));
    }

    // TODO: Verify the correctness of this!
    Leaf_t *erase(Leaf_t *it)
    {
        Leaf_t *ret = NULL, *old_prev, *old_next;

        old_prev = it->prev;
        old_next = it->next;

        if(old_prev)
            old_prev->next = old_next;
        else
            m_begin = old_next;

        if(old_next)
            ret = old_next->prev = old_prev;
        else
            m_last = old_prev;

#if defined(__DJGPP__)
        dpmi_allocator_impl::dpmi_unlock_memory(it, sizeof(Leaf_t));
#endif
        free(it);

        --m_size;

        return ret;
    }

    void clean()
    {
        if(m_begin && m_last && m_size > 0)
        {
            Leaf_t *cur;

            while(m_size > 0)
            {
                cur = m_begin;
                m_begin = cur->next;

#if defined(__DJGPP__)
                dpmi_allocator_impl::dpmi_unlock_memory(cur, sizeof(Leaf_t));
#endif
                free(cur);
                --m_size;
            }

            m_last = NULL;
        }
    }

    void dpmi_lock_end() {}
};

#endif // MIDITRAC_LIST_HPP
