/*!
 * @file KnxComObject.cpp
 *
 * @section author Author
 *
 * Written by Alexander Christian.
 *
 * @section license License
 *
 *    Copyright (C) 2016 Alexander Christian <info(at)root1.de>. All rights
 * reserved. This file is part of KONNEKTING Device Library.
 *
 *    The KONNEKTING Device Library is free software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * Handling of the KNX Communication Objects
 * @author Alexander Christian <info(at)root1.de>
 * @depends KnxTelegram
 */

#include <DebugUtil.h>
#include "KnxComObject.h"

/**
 * Calculates telegram data length based on DPT
 * @param dptId
 * @return length in bytes
 */
byte calcLength(KnxDpt dptId) {
    return (pgm_read_byte(&KnxDptFormatToLength[ pgm_read_byte(&KnxDptToFormat[dptId])]) / 8) + 1;
}

/**
 * Contructor
 * @param dptId
 * @param indicator
 */
KnxComObject::KnxComObject(KnxDpt dptId, byte indicator)
: _dptId(dptId), _indicator(indicator), _dataLength(calcLength(dptId)) {
        
    if (_indicator & KNX_COM_OBJ_I_INDICATOR) {
        // Object with "InitRead" indicator
        _validated = false; 
    } else {
        // any other typed object
        _validated = true; 
    }
    
    if (_dataLength <= 2) {
        _longValue = NULL; 
    } else { 
        // long value case
        _longValue = (byte *) malloc(_dataLength - 1);
        for (byte i = 0; i < _dataLength - 1; i++) {
            _longValue[i] = 0;
        }
    }
    
}

/**
 * Destructor
 */
KnxComObject::~KnxComObject() {
    if (_dataLength > 2) {
        free(_longValue);
    }
}

/**
 * TODO document me
 * @return 
 */
bool KnxComObject::isActive() {
    return _active;
}

/**
 * Get the com obj value
 * Ensure 'value' is sizeOf data-length
 * @param value
 */
void KnxComObject::getValue(byte value[]) const {
    if (_dataLength <= 2) {
        value[0] = _value; // short value case, ReadValue(void) fct should rather be used
    } else {
        for (byte i = 0; i < _dataLength - 1; i++) {
            value[i] = _longValue[i]; // long value case
        }
    }
}

/**
 * Update comobj value by raw byte[]
 * @param other
 */
void KnxComObject::updateValue(const byte other[]) {
    if (_dataLength <= 2) {
        _value = other[0]; // short value case, UpdateValue(byte) fct should rather be used
    } else {
        for (byte i = 0; i < _dataLength - 1; i++) {
            _longValue[i] = other[i]; // long value case
        }
    }
    _validated = true; 
}

/**
 * Update comobj value with payload from telegram
 * @param other
 * @return KNX_COM_OBJECT_ERROR or KNX_COM_OBJECT_OK
 */
byte KnxComObject::updateValue(const KnxTelegram& other) {
    
    if (other.getPayloadLength() != getLength()) {
        return KNX_COM_OBJECT_ERROR; // Error : telegram payload length differs from com obj one
    }
    
    switch (_dataLength) {
        case 1:
            _value = other.getFirstPayloadByte();
            break;
        case 2:
            other.getLongPayload(&_value, 1);
            break;
        default:
            other.getLongPayload(_longValue, _dataLength - 1);
    }    
    
    _validated = true; // com object set to valid
    return KNX_COM_OBJECT_OK;
}

/**
 * Copy comobj attributes into a telegram
 * @param dest
 */
void KnxComObject::copyAttributes(KnxTelegram& dest) const {
    dest.changePriority(getPriority());
    dest.setTargetAddress(getAddr());
    dest.setPayloadLength(_dataLength); 
}

/**
 * Copy the comobj value into a telegram 
 * @param dest
 */
void KnxComObject::copyValue(KnxTelegram& dest) const {
    switch (_dataLength) {
        case 1:
            dest.setFirstPayloadByte(_value);
            break;
        case 2:
            dest.setLongPayload(&_value, 1);
            break;
        default:
            dest.setLongPayload(_longValue, _dataLength - 1);
    }    
}