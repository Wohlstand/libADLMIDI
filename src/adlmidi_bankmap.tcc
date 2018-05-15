/*
 * libADLMIDI is a free MIDI to WAV conversion library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2018 Vitaly Novichkov <admin@wohlnet.ru>
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

#include "adlmidi_bankmap.h"
#include <cassert>

template <class T>
inline BasicBankMap<T>::BasicBankMap()
{
    m_buckets.reset(new Slot *[hash_buckets]());
}

template <class T>
inline size_t BasicBankMap<T>::hash(key_type key)
{
    // disregard the 0 high bit in LSB
    key = (key & 127) | ((key >> 8) << 7);
    // take low part as hash value
    return key & (hash_buckets - 1);
}

template <class T>
void BasicBankMap<T>::reserve(size_t capacity)
{
    if(m_capacity >= capacity)
        return;

    size_t need = capacity - m_capacity;
    need = (need < minimum_allocation) ? minimum_allocation : need;

    Slot *slots = new Slot[need];
    m_allocations.push_back(std::unique_ptr<Slot[]>(slots));
    m_capacity += need;

    for(size_t i = need; i-- > 0;)
        free_slot(&slots[i]);
}

template <class T>
typename BasicBankMap<T>::iterator
BasicBankMap<T>::begin() const
{
    iterator it(m_buckets.get(), nullptr, 0);
    while(it.index < hash_buckets && !(it.slot = m_buckets[it.index]))
        ++it.index;
    return it;
}

template <class T>
typename BasicBankMap<T>::iterator
BasicBankMap<T>::end() const
{
    iterator it(m_buckets.get(), nullptr, hash_buckets);
    return it;
}

template <class T>
typename BasicBankMap<T>::iterator BasicBankMap<T>::find(key_type key)
{
    size_t index = hash(key);
    Slot *slot = bucket_find(index, key);
    if(!slot)
        return end();
    return iterator(m_buckets.get(), slot, index);
}

template <class T>
void BasicBankMap<T>::erase(iterator it)
{
    bucket_remove(it.index, it.slot);
    free_slot(it.slot);
    --m_size;
}

template <class T>
BasicBankMap<T>::iterator::iterator(Slot **buckets, Slot *slot, size_t index)
    : buckets(buckets), slot(slot), index(index)
{
}

template <class T>
typename BasicBankMap<T>::iterator &
BasicBankMap<T>::iterator::operator++()
{
    if(slot->next)
        slot = slot->next;
    else {
        Slot *slot = nullptr;
        ++index;
        while(index < hash_buckets && !(slot = buckets[index]))
            ++index;
        this->slot = slot;
    }
    return *this;
}

template <class T>
bool BasicBankMap<T>::iterator::operator==(const iterator &o) const
{
    return buckets == o.buckets && slot == o.slot && index == o.index;
}

template <class T>
inline bool BasicBankMap<T>::iterator::operator!=(const iterator &o) const
{
    return !operator==(o);
}

template <class T>
std::pair<typename BasicBankMap<T>::iterator, bool>
BasicBankMap<T>::insert(const value_type &value)
{
    size_t index = hash(value.first);
    Slot *slot = bucket_find(index, value.first);
    if(slot)
        return {iterator(m_buckets.get(), slot, index), false};
    slot = allocate_slot();
    if(!slot) {
        reserve(m_capacity + minimum_allocation);
        slot = ensure_allocate_slot();
    }
    slot->value = value;
    bucket_add(index, slot);
    ++m_size;
    return {iterator(m_buckets.get(), slot, index), true};
}

template <class T>
std::pair<typename BasicBankMap<T>::iterator, bool>
BasicBankMap<T>::insert(const value_type &value, do_not_expand_t)
{
    size_t index = hash(value.first);
    Slot *slot = bucket_find(index, value.first);
    if(slot)
        return {iterator(m_buckets.get(), slot, index), false};
    slot = allocate_slot();
    if(!slot)
        return {end(), false};
    slot->value = value;
    bucket_add(index, slot);
    ++m_size;
    return {iterator(m_buckets.get(), slot, index), true};
}

template <class T>
void BasicBankMap<T>::clear()
{
    for(size_t i = 0; i < hash_buckets; ++i) {
        Slot *slot = m_buckets[i];
        while (Slot *cur = slot) {
            slot = slot->next;
            free_slot(cur);
        }
        m_buckets[i] = nullptr;
    }
    m_size = 0;
}

template <class T>
inline T &BasicBankMap<T>::operator[](key_type key)
{
    return insert({key, T()}).first->second;
}

template <class T>
typename BasicBankMap<T>::Slot *
BasicBankMap<T>::allocate_slot()
{
    Slot *slot = m_freeslots;
    if(!slot)
        return nullptr;
    Slot *next = slot->next;
    if(next)
        next->prev = nullptr;
    m_freeslots = next;
    return slot;
}

template <class T>
inline typename BasicBankMap<T>::Slot *
BasicBankMap<T>::ensure_allocate_slot()
{
    Slot *slot = allocate_slot();
    assert(slot);
    return slot;
}

template <class T>
void BasicBankMap<T>::free_slot(Slot *slot)
{
    Slot *next = m_freeslots;
    if(next)
        next->prev = slot;
    slot->prev = nullptr;
    slot->next = next;
    m_freeslots = slot;
    m_freeslots->value.second = T();
}

template <class T>
typename BasicBankMap<T>::Slot *
BasicBankMap<T>::bucket_find(size_t index, key_type key)
{
    Slot *slot = m_buckets[index];
    while(slot && slot->value.first != key)
        slot = slot->next;
    return slot;
}

template <class T>
void BasicBankMap<T>::bucket_add(size_t index, Slot *slot)
{
    assert(slot);
    Slot *next = m_buckets[index];
    if(next)
        next->prev = slot;
    slot->next = next;
    m_buckets[index] = slot;
}

template <class T>
void BasicBankMap<T>::bucket_remove(size_t index, Slot *slot)
{
    assert(slot);
    Slot *prev = slot->prev;
    Slot *next = slot->next;
    if(!prev)
        m_buckets[index] = next;
    else
        prev->next = next;
    if(next)
        next->prev = prev;
}
