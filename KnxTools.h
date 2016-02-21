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

#ifndef KNXTOOLS_h
#define KNXTOOLS_h

#include <Arduino.h> 
#include <KnxDevice.h>
#include <EEPROM.h>

#ifndef ESP8266 
#include <avr/wdt.h>
#endif

#ifdef ESP8266
#define LED_BUILTIN 16
#endif

#define PARAM_INT8 1
#define PARAM_UINT8 1
#define PARAM_INT16 2
#define PARAM_UINT16 2
#define PARAM_INT32 4
#define PARAM_UINT32 4


// process intercepted knxEvents-calls with this method
extern void knxToolsEvents(byte index);

class KnxTools {
    static byte _paramSizeList[];
    static const byte _numberOfParams;                // Nb of attached Parameters

    // Constructor, Destructor
    KnxTools(); // private constructor (singleton design pattern)

    ~KnxTools() {
    } // private destructor (singleton design pattern)
    KnxTools(const KnxTools&); // private copy constructor (singleton design pattern) 

public:
    static KnxTools Tools;
                                                    // The value shall be provided by the end-user

    void init(HardwareSerial& serial, int progButtonPin, int progLedPin, word manufacturerID, byte deviceID, byte revisionID);

    /**
     * needs to be called in "void knxToolsEvents(byte index)" to check if ComObject is
     * an internal ComObject which is not needed to be handled by developer
     * f.i. ComObject 0 --> for programming purpose
     * @param index index of KnxComObject
     * @return if provided comobject index is an internal comobject (true) or not (false)
     */
    bool internalComObject(byte index);

    // must be public to be accessible from KnxToolsProgButtonPressed())
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

    /**
     * Check whether the Knx Tools is initialized (Tools.init(...)) and therefore active or not
     * @return true, if tools are initialized and active, false if not
     */
    bool isActive();
    
    /**
     * Gets programming state
     * @return true, if programming is active, false if not
     */
    bool getProgState();
    //
    //
private:

    bool _initialized = false;

    word _individualAddress;

    byte _deviceFlags;
    word _manufacturerID;
    byte _deviceID;
    byte _revisionID;

    int _paramTableStartindex;


    int _progLED; // default pin D8
    int _progButton; // default pin D3 (->interrupt)

    bool _progState;

    int calcParamSkipBytes(byte index);

    void setProgState(bool state);
    bool isFactorySetting();

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
    
    void memoryUpdate(int index, byte date);

};

// not part of KnxTools class
void KnxToolsProgButtonPressed();

// Reference to the KnxDevice unique instance
extern KnxTools& Tools;

#endif // KNXTOOLS_h
