/*
 *    This file is part of KONNEKTING Knx Device Library.
 *
 *    The KONNEKTING Knx Device Library is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// File : RingBuff.h
// Initial Author : Alexander Christian <info(at)root1.de>
// Description : Implementation of a ring buffer
// Module dependencies : none

#ifndef RINGBUFF_H
#define RINGBUFF_H

#include "Arduino.h"
#include "DebugUtil.h"

template<typename T, uint16_t size>
class RingBuff {
    byte _head;
    byte _tail;
    T _buffer[size]; // item buffer
    byte _size;
    byte _itemCount;

public:

    /**
     * Constructor
     */
    RingBuff() {
        _head = 0;
        _tail = 0;
        _itemCount = 0;
        _size = size;
    };


    /**
     * Append data to tail
     * @param appendedData
     */
    void append(const T& data) {
        if (_itemCount == _size) { // buffer full, overwriting oldest data
            incHead();
        } else {
            _itemCount++;
        }
        _buffer[_tail] = data;
        incTail();
    }

    /**
     * Pop data from head
     * @param data the popped data
     * @return false, if no items available
     */
    boolean pop(T& data) {
        if (_itemCount==0) return false;
        data = _buffer[_head];
        incHead();
        _itemCount--;
        return true;
    }

    /**
     * Returns number of items in buffer
     * @return item count
     */
    byte getItemCount(void) const {
        return _itemCount;
    }

private:

    void incHead(void) {
        _head = (_head + 1) % _size;
    }

    void incTail(void) {
        _tail = (_tail + 1) % _size;
    }
};

#endif // RINGBUFF_H
