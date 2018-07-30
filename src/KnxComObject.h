/*
 *    This file is part of KONNEKTING Knx Device Library.
 * 
 *    It is derived from another GPLv3 licensed project:
 *      The Arduino Knx Bus Device library allows to turn Arduino into "self-made" KNX bus device.
 *      Copyright (C) 2014 2015 Franck MARINI (fm@liwan.fr)
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


// File : KnxComObject.h
// Author : Franck Marini
// Modified: Alexander Christian <info(at)root1.de>
// Description : Handling of the KNX Communication Objects
// Module dependencies : KnxTelegram

#ifndef KNXCOMOBJECT_H
#define KNXCOMOBJECT_H

#include "KnxTelegram.h"
#include "KnxDataPointTypes.h"

// !!!!!!!!!!!!!!! FLAG OPTIONS !!!!!!!!!!!!!!!!!

// Definition of comobject indicator values
// See "knx.org" for comobject indicators specification
// See: https://redaktion.knx-user-forum.de/lexikon/flags/
// INDICATOR field : B7  B6  B5  B4  B3  B2  B1  B0
//                   xx  xx   C   R   W   T   U   I  
//                   xx  xx   K   L   S   Ü   A   I  
#define KNX_COM_OBJ_C_INDICATOR	0x20 // Comuunication (C)
#define KNX_COM_OBJ_R_INDICATOR	0x10 // Read (R)
#define KNX_COM_OBJ_W_INDICATOR	0x08 // Write (W)
#define KNX_COM_OBJ_T_INDICATOR	0x04 // Transmit (T)
#define KNX_COM_OBJ_U_INDICATOR	0x02 // Update (U)
#define KNX_COM_OBJ_I_INDICATOR	0x01 // Init Read (I)

// Definition of predefined com obj profiles
// Sensor profile : COM_OBJ_SENSOR
#define COM_OBJ_SENSOR KNX_COM_OBJ_C_R_T_INDICATOR
#define KNX_COM_OBJ_C_R_T_INDICATOR 0x34 // ( Communication | Read | Transmit )

// Logic input profile : COM_OBJ_LOGIC_IN
#define COM_OBJ_LOGIC_IN KNX_COM_OBJ_C_W_U_INDICATOR

#define KNX_COM_OBJ_C_W_U_INDICATOR 0x2A  // ( Communication | Write | Update )
#define KNX_COM_OBJ_C_W_U_T_INDICATOR 0x2E  // ( Communication | Write | Update | Transmit )

// Logic input to be initialized at bus power-up profile : COM_OBJ_LOGIC_IN_INIT
#define COM_OBJ_LOGIC_IN_INIT KNX_COM_OBJ_C_W_U_I_INDICATOR
#define KNX_COM_OBJ_C_W_U_I_INDICATOR 0x2B  // ( Communication | Write | Update | Init)

#define KNX_COM_OBJECT_OK       0
#define KNX_COM_OBJECT_ERROR    255

class KnxComObject {
    
    /**
     * true: CO can be used, false: CO is "offline"
     */
    bool _active;
    
    /**
     *  Group Address value
     */
    word _addr; 

    /**
     * DPT
     */
    byte _dptId; 

    /**
     * C/R/W/T/U/I indicators
     */
    byte _indicator; 

    /** 
     * Com object data length is calculated in the same way as telegram payload length
     * (See "knx.org" telegram specification for more details)
     */
    byte _dataLength;

    /**
     * _validated: used for "InitRead" typed comobjects:
     *  "false" until the object value is updated
     *  Other typed comobjects get "true" value immediately
     */
    bool _validated;

    union {
        // field used in case of short value (1 byte max width, i.e. length <= 2)
        struct {
            byte _value;
            byte _notUSed;
        };
        // field used in case of long value (2 bytes width or more, i.e. length > 2)
        // The data space is allocated dynamically by the constructor
        byte *_longValue;
    };

public:
    // Constructor :
    KnxComObject(KnxDpt dptId, byte indicator);

    // Destructor
    ~KnxComObject();
    
    bool isActive(void);
    void setActive(bool flag);

    // INLINED functions (see definitions later in this file)
    word getAddr(void) const;

    /**
     * Overwrite address value defined at construction
     * The function is used in case of programming
     * @param the GA to set
     */
    void setAddr(word);

    byte getDptId(void) const;

    e_KnxPriority getPriority(void) const;

    byte getIndicator(void) const;

    bool getValidity(void) const;

    void setValidity(void);

    byte getLength(void) const;

    /**
     * Return the com obj value (short value case only)
     * @return 
     */
    byte getValue(void) const;

    /**
     * Update the com obj value (short value case only)
     * @param newVal
     * @return ERROR if the com obj is long value (invalid use case), else return OK
     */
    byte updateValue(byte newVal);

    /**
     * Toggle the binary value (for com objs with "B1" format)
     */
    void toggleValue(void);

    // functions NOT INLINED :

    /**
     * Get the com obj value (short and long value cases)
     * @param dest
     */
    void getValue(byte dest[]) const;

    /**
     * Update the com obj value (short and long value cases)
     * @param ori
     */
    void updateValue(const byte ori[]);

    /**
     * Update the com obj value with a telegram payload content
     * @param ori
     * @return ERROR if the telegram payload length differs from com obj one, else return OK
     */
    byte updateValue(const KnxTelegram& ori);

    /**
     * Copy the com obj attributes (addr, prio, length) into a telegram object
     * @param dest
     */
    void copyAttributes(KnxTelegram& dest) const;

    /**
     * Copy the com obj value into a telegram object
     * @param dest
     */
    void copyValue(KnxTelegram& dest) const;

};


// --------------- Definition of the INLINE functions -----------------

inline word KnxComObject::getAddr(void) const {
    return _addr;
}

inline void KnxComObject::setAddr(word addr) {
    _addr = addr;
}

inline byte KnxComObject::getDptId(void) const {
    return _dptId;
}

inline e_KnxPriority KnxComObject::getPriority(void) const {
    return KNX_PRIORITY_NORMAL_VALUE;
}

inline byte KnxComObject::getIndicator(void) const {
    return _indicator;
}

inline bool KnxComObject::getValidity(void) const {
    return _validated;
}

inline void KnxComObject::setValidity(void) {
    _validated = true;
}

inline byte KnxComObject::getLength(void) const {
    return _dataLength;
}

inline byte KnxComObject::getValue(void) const {
    return _value;
}

inline byte KnxComObject::updateValue(byte newValue) {
    if (_dataLength > 2) return KNX_COM_OBJECT_ERROR;
    _value = newValue;
    _validated = true;
    return KNX_COM_OBJECT_OK;
}

inline void KnxComObject::toggleValue(void) {
    _value = !_value;
}

#endif // KNXCOMOBJECT_H
