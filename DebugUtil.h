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

#ifndef DEBUGUTIL_H
#define DEBUGUTIL_H


#include <Arduino.h>
#define DEBUG_PRINTLN(...)
#define DEBUG_PRINTLN2(...)
#define DEBUG_PRINT(...)
#define DEBUG_PRINT2(...)


//#define debug(...) if (myprint!=NULL) {myprint->print(__VA_ARGS__);}
//#define debugln(...) if (myprint!=NULL) {myprint->println(__VA_ARGS__);}

class DebugUtil {
    
   // Constructor, Destructor
    DebugUtil(); // private constructor (singleton design pattern)

    ~DebugUtil() {
    } // private destructor (singleton design pattern)
    DebugUtil(DebugUtil&); // private copy constructor (singleton design pattern) 
    
private:    
    Print* _printstream;

public:
    static DebugUtil Debug;
    
    void setPrintStream(Stream* printstream);
    
    int freeRam();
    
    void print(char *format, ...);
    void print(const __FlashStringHelper *format, ...);
    void println(char *format, ...);
    void println(const __FlashStringHelper *format, ...);
};

// Reference to the KnxDevice unique instance
extern DebugUtil& Debug;

#endif // DEBUGUTIL_H
