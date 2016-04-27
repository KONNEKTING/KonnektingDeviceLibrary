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

    #ifdef DEBUG
            #include <Arduino.h>
            #ifndef DEBUGCONFIG_DEVICE
                    #ifdef DEBUGCONFIG_SOFWARESERIAL
                            #ifndef DEBUGCONFIG_RX_PIN
                                    #define DEBUGCONFIG_RX_PIN 11
                            #endif
                            #ifndef DEBUGCONFIG_TX_PIN
                                    #define DEBUGCONFIG_TX_PIN 10
                            #endif
                            #ifndef SoftwareSerial_h
                                    #error "You need to include SoftwareSerial in your project in order to use it: #include <SoftwareSerial.h>"
                            #endif
                            #include <SoftwareSerial.h>
                            SoftwareSerial DebugSoftwareSerial(DEBUGCONFIG_RX_PIN, DEBUGCONFIG_TX_PIN); // RX, TX
                            #define DEBUGCONFIG_DEVICE DebugSoftwareSerial
                    #else
                            #define DEBUGCONFIG_DEVICE Serial
                    #endif
            #endif
            #ifndef DEBUGCONFIG_BAUDS
                    #define DEBUGCONFIG_BAUDS 9600
            #endif
            void DebugInitFunction() {
                    DEBUGCONFIG_DEVICE.begin(DEBUGCONFIG_BAUDS);
            }
            #define DebugInit() DebugInitFunction()
            #define DEBUG_PRINT(A) DEBUGCONFIG_DEVICE.print(A)
            #define DEBUG_PRINTLN(A) DEBUGCONFIG_DEVICE.println(A)
            #define DEBUG_PRINT2(A, B) DEBUGCONFIG_DEVICE.print(A, B)
            #define DEBUG_PRINTLN2(A, B) DEBUGCONFIG_DEVICE.println(A, B)
    #else
            #define DebugInit()
            #define DEBUG_PRINT(A)
            #define DEBUG_PRINTLN(A)
            #define DEBUG_PRINT2(A, B)
            #define DEBUG_PRINTLN2(A, B)
    #endif


#endif // KONNEKTINGDEBUG_H
