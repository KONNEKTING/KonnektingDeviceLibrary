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


// File : KonnektingDebug.h
// Author: Alexander Christian <info(at)root1.de>
// Description : project debug, inspired by https://github.com/Naguissa/DebugUtils

#ifndef KONNEKTINGDEBUG_H
#define KONNEKTINGDEBUG_H

//#include <Arduino.h>
//
//#ifdef DEBUG
//
//        #define DEBUG_PRINT(A) if (__DEBUG_SERIAL!=NULL) { __DEBUG_SERIAL.print(A);}
//        #define DEBUG_PRINTLN(A) if (__DEBUG_SERIAL!=NULL) { __DEBUG_SERIAL.println(A);}
//        #define DEBUG_PRINT2(A, B) if (__DEBUG_SERIAL!=NULL) { __DEBUG_SERIAL.print(A, B);}
//        #define DEBUG_PRINTLN2(A, B) if (__DEBUG_SERIAL!=NULL) { __DEBUG_SERIAL.println(A, B);}
//#else
        #define DEBUG_PRINT(A)
        #define DEBUG_PRINTLN(A)
        #define DEBUG_PRINT2(A, B)
        #define DEBUG_PRINTLN2(A, B)
//#endif
//static Serial_ __DEBUG_SERIAL;
//
//
//// DEBUG PROTOCOL HANDLING
//#define DEBUG_PROTOCOL
//
//// remove/disable just for debugging purpose to disable memory write
//#define WRITEMEM

#endif // KONNEKTINGDEBUG_H
