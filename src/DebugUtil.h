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


// File : KonnektingDebug.h
// Author: Alexander Christian <info(at)root1.de>
// Description : project debug, inspired by https://github.com/Naguissa/DebugUtils

#ifndef DEBUGUTIL_H
#define DEBUGUTIL_H


#include <Arduino.h>
#ifdef __SAMD21G18A__
#include "stdarg.h"
#endif

#define DEBUG

#ifdef DEBUG
    #define DEBUG_PRINT(...) Debug.print(__VA_ARGS__);
    #define DEBUG_PRINTLN(...) Debug.println(__VA_ARGS__);
#else
    #define DEBUG_PRINT(...)
    #define DEBUG_PRINTLN(...)
#endif

#define BYTETOBINARYPATTERN "%d%d%d%d%d%d%d%d"
#define BYTETOBINARY(byte)  \
  ((byte & 0x80) ? 1 : 0), \
  ((byte & 0x40) ? 1 : 0), \
  ((byte & 0x20) ? 1 : 0), \
  ((byte & 0x10) ? 1 : 0), \
  ((byte & 0x08) ? 1 : 0), \
  ((byte & 0x04) ? 1 : 0), \
  ((byte & 0x02) ? 1 : 0), \
  ((byte & 0x01) ? 1 : 0) 


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
    
    void print(const char *format, ...);
    void print(const __FlashStringHelper *format, ...);
    void println(const char *format, ...);
    void println(const __FlashStringHelper *format, ...);
};

// Reference to the KnxDevice unique instance
extern DebugUtil& Debug;

#endif // DEBUGUTIL_H
