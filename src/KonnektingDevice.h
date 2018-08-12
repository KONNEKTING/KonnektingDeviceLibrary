/*!
 * @file KonnektingDevice.h
 *
 * This is part of KONNEKTING Device Library for the Arduino platform.  
 *
 *    Copyright (C) 2016 Alexander Christian <info(at)root1.de>. All rights reserved.
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

#ifndef KONNEKTING_h
#define KONNEKTING_h

#define KONNEKTING_DEVICE_LIBRARY_VERSION 10000
#define KONNEKTING_DEVICE_LIBRARY_SNAPSHOT 
#define KONNEKTING_1_0_0_beta4b

#include <Arduino.h>
#include <DebugUtil.h>
#include <KnxDevice.h>
#include <KnxDptConstants.h>

// AVR, ESP8266, ESP32 and STM32 uses EEPROM (SAMD21 not ...)
#if defined(__AVR__) || defined(ESP8266) || defined(ESP32) || defined(ARDUINO_ARCH_STM32)
#include <EEPROM.h>
#ifdef ARDUINO_ARCH_AVR
#include <avr/wdt.h>
#endif
#endif

#define EEPROM_DEVICE_FLAGS          0  ///< EEPROM index for device flags
#define EEPROM_INDIVIDUALADDRESS_HI  1  ///< EEPROM index for IA, high byte
#define EEPROM_INDIVIDUALADDRESS_LO  2  ///< EEPROM index for IA, low byte
#define EEPROM_COMOBJECTTABLE_START 10  ///< EEPROM index start of comobj table

#define PROTOCOLVERSION 0

#define MSGTYPE_ACK                         0 ///< Message Type: ACK 0x00
#define MSGTYPE_READ_DEVICE_INFO            1 ///< Message Type: Read Device Information 0x01
#define MSGTYPE_ANSWER_DEVICE_INFO          2 ///< Message Type: Answer Device Information 0x02
#define MSGTYPE_RESTART                     9 ///< Message Type: Restart 0x09

#define MSGTYPE_WRITE_PROGRAMMING_MODE      10 // 0x0A
#define MSGTYPE_READ_PROGRAMMING_MODE       11 // 0x0B
#define MSGTYPE_ANSWER_PROGRAMMING_MODE     12 // 0x0C

#define MSGTYPE_WRITE_INDIVIDUAL_ADDRESS    20 // 0x14
#define MSGTYPE_READ_INDIVIDUAL_ADDRESS     21 // 0x15
#define MSGTYPE_ANSWER_INDIVIDUAL_ADDRESS   22 // 0x16

#define MSGTYPE_WRITE_PARAMETER             30 // 0x1E
#define MSGTYPE_READ_PARAMETER              31 // 0x1F
#define MSGTYPE_ANSWER_PARAMETER            32 // 0x20

#define MSGTYPE_WRITE_COM_OBJECT            40 // 0x28
#define MSGTYPE_READ_COM_OBJECT             41 // 0x29
#define MSGTYPE_ANSWER_COM_OBJECT           42 // 0x2A

#define PARAM_INT8 1
#define PARAM_UINT8 1
#define PARAM_INT16 2
#define PARAM_UINT16 2
#define PARAM_INT32 4
#define PARAM_UINT32 4
#define PARAM_RAW1 1
#define PARAM_RAW2 2
#define PARAM_RAW3 3
#define PARAM_RAW4 4
#define PARAM_RAW5 5
#define PARAM_RAW6 6
#define PARAM_RAW7 7
#define PARAM_RAW8 8
#define PARAM_RAW9 9
#define PARAM_RAW10 10
#define PARAM_RAW11 11
#define PARAM_STRING11 11


// process intercepted knxEvents-calls with this method
extern void konnektingKnxEvents(byte index);

/**************************************************************************/
/*! 
 *  @brief  Main class provides KONNEKTING Device API
 */

/**************************************************************************/
class KonnektingDevice {
    static byte _paramSizeList[];
    static const int _numberOfParams;

    byte (*_eepromReadFunc)(int);
    void (*_eepromWriteFunc)(int, byte);
    void (*_eepromUpdateFunc)(int, byte);
    void (*_eepromCommitFunc)(void);
    void (*_progIndicatorFunc)(bool);

    // Constructor, Destructor
    KonnektingDevice(); // private constructor (singleton design pattern)

    ~KonnektingDevice() {
    } // private destructor (singleton design pattern)
    KonnektingDevice(KonnektingDevice&); // private copy constructor (singleton design pattern) 

public:
    static KonnektingDevice Konnekting;

    void setMemoryReadFunc(byte(*func)(int));
    void setMemoryWriteFunc(void (*func)(int, byte));
    void setMemoryUpdateFunc(void (*func)(int, byte));
    void setMemoryCommitFunc(void (*func)(void));

    void init(HardwareSerial& serial, 
                void (*progIndicatorFunc)(bool), 
                word manufacturerID, 
                byte deviceID, 
                byte revisionID
                );

    void init(HardwareSerial& serial,
            int progButtonPin,
            int progLedPin,
            word manufacturerID,
            byte deviceID,
            byte revisionID
            );

    // needs to be public too, due to ISR handler mechanism :-(
    bool internalKnxEvents(byte index);

    // must be public to be accessible from KonnektingProgButtonPressed()
    void toggleProgState();

    

    byte getParamSize(int index);
    void getParamValue(int index, byte* value);

    uint8_t getUINT8Param(int index);
    int8_t getINT8Param(int index);

    uint16_t getUINT16Param(int index);
    int16_t getINT16Param(int index);

    uint32_t getUINT32Param(int index);
    int32_t getINT32Param(int index);

    String getSTRING11Param(int index);

    bool isActive();
    bool isFactorySetting();

    bool isProgState();

    bool isReadyForApplication();

    void setProgState(bool state);

    int getFreeEepromOffset();

private:

    bool _rebootRequired = false;
    bool _initialized = false;
#ifdef REBOOT_BUTTON
    byte _progbtnCount = 0;
    long _lastProgbtn = 0;
#endif
    word _individualAddress;

    byte _deviceFlags;
    word _manufacturerID;
    byte _deviceID;
    byte _revisionID;

    int _paramTableStartindex;


    int _progLED;
    int _progButton; // (->interrupt)
    void setProgLed(bool state);

    bool _progState;
    
    KnxComObject createProgComObject();

    void internalInit(HardwareSerial& serial,
            word manufacturerID,
            byte deviceID,
            byte revisionID
            );
    int calcParamSkipBytes(int index);

    bool isMatchingIA(byte hi, byte lo);

    void reboot();

    // prog methods    
    void sendAck(byte errorcode, int indexinformation);
    void handleMsgReadDeviceInfo(byte* msg);
    void handleMsgRestart(byte* msg);
    void handleMsgWriteProgrammingMode(byte* msg);
    void handleMsgReadProgrammingMode(byte* msg);
    void handleMsgWriteIndividualAddress(byte* msg);
    void handleMsgReadIndividualAddress(byte* msg);
    void handleMsgWriteParameter(byte* msg);
    void handleMsgReadParameter(byte* msg);
    void handleMsgWriteComObject(byte* msg);
    void handleMsgReadComObject(byte* msg);

    int memoryRead(int index);
    void memoryWrite(int index, byte date);
    void memoryUpdate(int index, byte date);
    void memoryCommit();

};

// not part of Konnekting class
void KonnektingProgButtonPressed();

// Reference to the KnxDevice unique instance
extern KonnektingDevice& Konnekting;

#endif // KONNEKTING_h

