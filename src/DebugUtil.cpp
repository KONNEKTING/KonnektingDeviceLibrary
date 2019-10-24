/*!
 * @file DebugUtil.cpp
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

#include "DebugUtil.h"
#include "wiring_private.h"

#if defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_STM32)
    extern "C" char* sbrk(int incr);
    extern char *__brkval;
#endif

// DebugUtil unique instance creation
DebugUtil DebugUtil::Debug;
DebugUtil& Debug = DebugUtil::Debug;

/*
 * Format Help:
 * http://www.cplusplus.com/reference/cstdio/printf/
 * 
 * %i = signed decimal integer
 * %u = unsigned decimal integer
 * %x = hex
 * %X = upper case hex
 * %s = string
 * %c = character
 * 0x%02x = hex representation like 0xff
 * %% = % symbol
 */
DebugUtil::DebugUtil() {
}

void DebugUtil::setPrintStream(Stream* printstream) {
    _printstream = printstream;
    print(F("DEBUG! free ram: %d bytes \n"), freeRam());
}

int DebugUtil::freeRam() {
#if defined(ESP8266) || defined(ESP32)
    return ESP.getFreeHeap();
#elif defined(ARDUINO_ARCH_AVR)
    extern int __heap_start, *__brkval;
    int v;
    return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
#elif defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_STM32)
    char top;
    return &top - reinterpret_cast<char*>(sbrk(0));
#else
    return -1;
#endif
}

void DebugUtil::print(const char *format, ...) {
    if (_printstream) {
        char buf[128]; // limit to 128chars
        va_list args;
        va_start(args, format);
        vsnprintf(buf, 128, format, args);
        va_end(args);
        _printstream->print(buf);
    }

}

void DebugUtil::print(const __FlashStringHelper *format, ...) {
    if (_printstream) {
        char buf[128]; // limit to 128chars
        va_list args;
        va_start(args, format);

#if defined(__AVR__) || defined(ESP8266) || defined(ARDUINO_ARCH_STM32)
        vsnprintf_P(buf, sizeof (buf), (const char *) format, args); // progmem for AVR and ESP8266
#else
        vsnprintf(buf, sizeof (buf), (const char *) format, args);   // for rest of the world
#endif    
        va_end(args);
        //Serial.print(buf);)    
        _printstream->print(buf);
    }
}

void DebugUtil::println(const char *format, ...) {
    if (_printstream) {

        char buf[128]; // limit to 128chars
        va_list args;
        va_start(args, format);
        vsnprintf(buf, 128, format, args);
        va_end(args);
        //Serial.println(buf);)
        _printstream->println(buf);
    }
}

void DebugUtil::println(const __FlashStringHelper *format, ...) {
    if (_printstream) {

        char buf[128]; // limit to 128chars
        va_list args;
        va_start(args, format);

#if defined(__AVR__) || defined(ESP8266) || defined(ARDUINO_ARCH_STM32)
        vsnprintf_P(buf, sizeof (buf), (const char *) format, args); // progmem for AVR and ESP8266
#else
        vsnprintf(buf, sizeof (buf), (const char *) format, args); // for rest of the world
#endif    

        va_end(args);
        //Serial.println(buf);)    
        _printstream->println(buf);
    }
}




