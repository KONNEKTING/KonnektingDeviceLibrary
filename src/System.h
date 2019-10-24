/*
 *    This file is part of KONNEKTING Device Library.
 *
 *    The KONNEKTING Device Library is free software: you can redistribute it and/or modify
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

// per known device, set the device's best matching system type
#if defined(ESP8266) || defined(ESP32)

    #warning Using KONNEKTING for ESP8266/ESP32
    #define KONNEKTING_SYSTEM_TYPE_DEFAULT

#elif defined ARDUINO_ARCH_STM32    

    #warning Using KONNEKTING for STM32
    #define KONNEKTING_SYSTEM_TYPE_DEFAULT

#elif defined ARDUINO_ARCH_SAMD

    #warning Using KONNEKTING for SAMD
    #define KONNEKTING_SYSTEM_TYPE_DEFAULT

#elif defined __AVR_ATmega32U4__    

#warning Using KONNEKTING for ATmega32U4
    #define KONNEKTING_SYSTEM_TYPE_SIMPLE
#else

#warning No system, specified, using SIMPLE for now...
    #define KONNEKTING_SYSTEM_TYPE_SIMPLE

#endif

// per system type, set required stuff
#if defined KONNEKTING_SYSTEM_TYPE_SIMPLE

    #warning Using KONNEKTING System Type Simple
    #define KONNEKTING_NUMBER_OF_ADDRESSES 128
    #define KONNEKTING_NUMBER_OF_ASSOCIATIONS 128
    #define KONNEKTING_NUMBER_OF_COMOBJECTS 128
    
    #define KONNEKTING_NUMBER_OF_PARAMETERS 128

    // start at end of system table
    #define KONNEKTING_MEMORYADDRESS_ADDRESSTABLE 32 
    // start at end of GA-Table
    #define KONNEKTING_MEMORYADDRESS_ASSOCIATIONTABLE KONNEKTING_MEMORYADDRESS_ADDRESSTABLE + 1 + (KONNEKTING_NUMBER_OF_ASSOCIATIONS*2) 
    // start at end of Assoc-Table
    #define KONNEKTING_MEMORYADDRESS_COMMOBJECTTABLE KONNEKTING_MEMORYADDRESS_ASSOCIATIONTABLE + 1 + (KONNEKTING_NUMBER_OF_COMOBJECTS*2) 
    // start at end of CommObj-Table
    #define KONNEKTING_MEMORYADDRESS_PARAMETERTABLE KONNEKTING_MEMORYADDRESS_COMMOBJECTTABLE + 1 + KONNEKTING_NUMBER_OF_PARAMETERS 

#elif defined KONNEKTING_SYSTEM_TYPE_DEFAULT

    #warning Using KONNEKTING System Type Default
    #define KONNEKTING_NUMBER_OF_ADDRESSES 255
    #define KONNEKTING_NUMBER_OF_ASSOCIATIONS 255
    #define KONNEKTING_NUMBER_OF_COMOBJECTS 255 // one is for prog com obj

    #define KONNEKTING_NUMBER_OF_PARAMETERS 256

    // start at end of system table --> 0x0020
    #define KONNEKTING_MEMORYADDRESS_ADDRESSTABLE 32 
    // start at end of GA-Table --> 0x0221
    #define KONNEKTING_MEMORYADDRESS_ASSOCIATIONTABLE KONNEKTING_MEMORYADDRESS_ADDRESSTABLE + 1 + (KONNEKTING_NUMBER_OF_ASSOCIATIONS*2) 
    // start at end of Assoc-Table --> 0x0420
    #define KONNEKTING_MEMORYADDRESS_COMMOBJECTTABLE KONNEKTING_MEMORYADDRESS_ASSOCIATIONTABLE + 1 + (KONNEKTING_NUMBER_OF_COMOBJECTS*2) 
    // start at end of CommObj-Table --> 0x0521
    #define KONNEKTING_MEMORYADDRESS_PARAMETERTABLE KONNEKTING_MEMORYADDRESS_COMMOBJECTTABLE + 1 + KONNEKTING_NUMBER_OF_PARAMETERS 

#else

    #error No KONNEKTING System Type set. Cannot continue.

#endif