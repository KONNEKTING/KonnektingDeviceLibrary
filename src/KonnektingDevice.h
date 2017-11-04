/*
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

/*
 * Knx Programming via GroupWrite-telegram
 * 
 * @author Alexander Christian <info(at)root1.de>
 * @since 2015-11-06
 * @license GPLv3
 */

#ifndef KONNEKTING_h
#define KONNEKTING_h

#include <Arduino.h> 
#include <DebugUtil.h>
#include <KnxDevice.h>
#include <KnxDptConstants.h>

// AVR and ESP8266 use EEPROM (SAMD21 not ...)
#if defined(__AVR__) || defined(ESP8266) || defined(STM32)
#include <EEPROM.h>
#ifndef ESP8266
#include <avr/wdt.h>
#endif
#endif

#define EEPROM_DEVICE_FLAGS          0
#define EEPROM_INDIVIDUALADDRESS_HI  1
#define EEPROM_INDIVIDUALADDRESS_LO  2
#define EEPROM_COMOBJECTTABLE_START 10

#define PROTOCOLVERSION 0

#define MSGTYPE_ACK                         0 // 0x00
#define MSGTYPE_READ_DEVICE_INFO            1 // 0x01
#define MSGTYPE_ANSWER_DEVICE_INFO          2 // 0x02
#define MSGTYPE_RESTART                     9 // 0x09

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

class KonnektingDevice {
    static byte _paramSizeList[];
    static const byte _numberOfParams;                // Nb of attached Parameters
    
    byte (*eepromReadFunc)(int);
    void (*eepromWriteFunc)(int, byte);
    void (*eepromUpdateFunc)(int, byte);
    void (*eepromCommitFunc)(void);
    void (*setProgLedFunc)(bool);
    
    // Constructor, Destructor
    KonnektingDevice(); // private constructor (singleton design pattern)

    ~KonnektingDevice() {
    } // private destructor (singleton design pattern)
    KonnektingDevice(KonnektingDevice&); // private copy constructor (singleton design pattern) 

public:
    static KonnektingDevice Konnekting;
    
    void setMemoryReadFunc(byte (*func)(int));
    void setMemoryWriteFunc(void (*func)(int, byte));
    void setMemoryUpdateFunc(void (*func)(int, byte));
    void setMemoryCommitFunc(void (*func)(void));

    void init(HardwareSerial& serial, 
                void (*func)(bool), 
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

    /**
     * needs to be called in "void konnektingKnxEvents(byte index)" to check if ComObject is
     * an internal ComObject which is not needed to be handled by developer
     * f.i. ComObject 0 --> for programming purpose
     * @param index index of KnxComObject
     * @return if provided comobject index is an internal comobject (true) or not (false)
     */
    bool internalComObject(byte index);
    
    // must be public to be accessible from KonnektingProgButtonPressed())
    void toggleProgState();

    KnxComObject createProgComObject();

    byte getParamSize(byte index);
    void getParamValue(int index, byte* value);
    
    uint8_t getUINT8Param(byte index);
    int8_t getINT8Param(byte index);
    
    uint16_t getUINT16Param(byte index);
    int16_t getINT16Param(byte index);
    
    uint32_t getUINT32Param(byte index);
    int32_t getINT32Param(byte index);
    
    String getSTRING11Param(byte index);

    /**
     * Check whether the Knx KonnektingDevice is initialized (Konnekting.init(...)) and therefore active or not
     * @return true, if tools are initialized and active, false if not
     */
    bool isActive();
    bool isFactorySetting();
    
    /**
     * Gets programming state
     * @return true, if programming is active, false if not
     */
    bool isProgState();
    
    /**
     * Check whether Konnekting is ready for application logic. 
     * (Means: no busy with programming-mode and not running with factory settings)
     * @return true if it's safe to run application logic
     */
    bool isReadyForApplication();

    int getFreeEepromOffset();
    
private:

    bool _rebootRequired = false;
    bool _initialized = false;
   
    int _progbtnCount = 0;
    long _lastProgbtn = 0;

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

    int calcParamSkipBytes(byte index);

    void setProgState(bool state);
    
    
    bool isMatchingIA(byte hi, byte lo);

    void reboot();

    // prog methods    
    void sendAck(byte errorcode, byte indexinformation);
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

