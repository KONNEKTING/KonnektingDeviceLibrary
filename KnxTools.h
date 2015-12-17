//    This file is part of Arduino Knx Bus Device library.

//    Copyright (C) 2015 Alexander Christian <info@root1.de>

//    The Arduino Knx Bus Device library is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.

//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.

//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.

/*
 * Knx Programming via GroupWrite-telegram
 *
 * @author Alexander Christian <info@root1.de>
 * @since 2015-11-06
 * @license GPLv3
 */

#ifndef KNXTOOLS_h
#define KNXTOOLS_h

#include <Arduino.h> 
#include <KnxDevice.h>
#include <EEPROM.h>



// process intercepted knxEvents-calls with this method
extern void knxToolsEvents(byte index);

class KnxTools {
    static byte _paramLenghtList[];

    // Constructor, Destructor
    KnxTools(); // private constructor (singleton design pattern)

    ~KnxTools() {
    } // private destructor (singleton design pattern)
    KnxTools(const KnxTools&); // private copy constructor (singleton design pattern) 

public:
    static KnxTools Tools;

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

    /**
     * Check whether the Knx Tools is initialized (Tools.init(...)) and therefore active or not
     * @return true, if tools are initialized and active, false if not
     */
    bool isActive();
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
    void sendAck();
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

};

// not part of KnxTools class
void KnxToolsProgButtonPressed();

// Reference to the KnxDevice unique instance
extern KnxTools& Tools;

#endif // KNXTOOLS_h
