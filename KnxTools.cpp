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

#include "KnxTools.h"
#include <avr/wdt.h>

/*
 * !!!!! IMPORTANT !!!!!
 * if "#define DEBUG" is set, you must run your KONNEKTING Suite with "-Dde.root1.slicknx.konnekting.debug=true" 
 * A release-version of your development MUST NOT contain "#define DEBUG" ...
 */
#define DEBUG

#define WRITEMEM

#if defined(DEBUG)
#include <SoftwareSerial.h>
SoftwareSerial debugSerial(10, 11); // RX, TX
char consolebuffer[80];
#define CONSOLEDEBUG(...)  debugSerial.print(__VA_ARGS__);
#define CONSOLEDEBUGLN(...)  debugSerial.println(__VA_ARGS__);
#else
#define CONSOLEDEBUG(...) 
#define CONSOLEDEBUGLN(...)
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

// KnxTools unique instance creation
KnxTools KnxTools::Tools;
KnxTools& Tools = KnxTools::Tools;

/**
 * Intercepting knx events to process internal com objects
 * @param index
 */
void knxToolsEvents(byte index) {

    CONSOLEDEBUG("knxToolsEvents index=");
    CONSOLEDEBUG(index);

    // if it's not a internal com object, route back to knxEvents()
    if (!Tools.internalComObject(index)) {
        knxEvents(index);
    }
}

/**
 * Constructor
 */
KnxTools::KnxTools() {
#ifdef DEBUG   
    debugSerial.begin(9600);
#endif    
    CONSOLEDEBUGLN("Setup KnxTools");
}

/**
 * Starts KNX Tools, as well as KNX Device
 * 
 * @param serial serial port reference, f.i. "Serial" or "Serial1"
 * @param progButtonPin pin which drives LED when in programming mode, default should be D3
 * @param progLedPin pin which toggles programming mode, needs an interrupt enabled pin!, default should be D8
 * @param manufacturerID
 * @param deviceID
 * @param revisionID
 * 
 */
void KnxTools::init(HardwareSerial& serial, int progButtonPin, int progLedPin, word manufacturerID, byte deviceID, byte revisionID) {
//    Serial.println("Hello Computer2");
    _initialized = true;

    _manufacturerID = manufacturerID;
    _deviceID = deviceID;
    _revisionID = revisionID;

    _progLED = progLedPin; // default pin D8
    _progButton = progButtonPin; // default pin D3 (->interrupt!)

    pinMode(_progLED, OUTPUT);
    pinMode(_progButton, INPUT);
    //digitalWrite(_progButton, HIGH); //PULLUP

    digitalWrite(_progLED, LOW);

    attachInterrupt(digitalPinToInterrupt(_progButton), KnxToolsProgButtonPressed, RISING);

    // hardcoded stuff
    CONSOLEDEBUG("Manufacturer: ");
    CONSOLEDEBUG(_manufacturerID, HEX);
    CONSOLEDEBUGLN("hex");

    CONSOLEDEBUG("Device: ");
    CONSOLEDEBUG(_deviceID, HEX);
    CONSOLEDEBUGLN("hex");

    CONSOLEDEBUG("Revision: ");
    CONSOLEDEBUG(_revisionID, HEX);
    CONSOLEDEBUGLN("hex");

    CONSOLEDEBUG("numberOfCommObjects: ");
    CONSOLEDEBUGLN(Knx.getNumberOfComObjects());

    // calc index of parameter table in eeprom --> depends on number of com objects
    _paramTableStartindex = EEPROM_COMOBJECTTABLE_START + (Knx.getNumberOfComObjects() * 2);

    _deviceFlags = EEPROM.read(EEPROM_DEVICE_FLAGS);
    
    CONSOLEDEBUG("_deviceFlags: ");
    CONSOLEDEBUG(_deviceFlags, BIN);
    CONSOLEDEBUGLN("bin");

    _individualAddress = P_ADDR(1, 1, 254);
    if (!isFactorySetting()) {
        CONSOLEDEBUGLN("Using EEPROM");
        /*
         * Read eeprom stuff
         */

        // PA
        byte hiAddr = EEPROM.read(EEPROM_INDIVIDUALADDRESS_HI);
        byte loAddr = EEPROM.read(EEPROM_INDIVIDUALADDRESS_LO);
        _individualAddress = (hiAddr << 8) + (loAddr << 0);

        // ComObjects
        // at most 255 com objects
        for (byte i = 0; i < Knx.getNumberOfComObjects(); i++) {
            byte hi = EEPROM.read(i + EEPROM_COMOBJECTTABLE_START);
            byte lo = EEPROM.read(i + EEPROM_COMOBJECTTABLE_START + 1);
            word comObjAddr = (hi << 8) + (lo << 0);
            Knx.setComObjectAddress(i, comObjAddr);
            CONSOLEDEBUG("ComObj #");
            CONSOLEDEBUG(i);
            CONSOLEDEBUG(" HI: 0x");
            CONSOLEDEBUG(hi,HEX);
            CONSOLEDEBUG(" LO: 0x");
            CONSOLEDEBUG(lo,HEX );
            CONSOLEDEBUG(" GA: 0x");
            CONSOLEDEBUG(comObjAddr, HEX);
            CONSOLEDEBUGLN("");
        }

    } else {
        CONSOLEDEBUGLN("Using FACTORY");
    }
    CONSOLEDEBUG("PA: ");
    CONSOLEDEBUGLN(_individualAddress, HEX);
    Knx.begin(serial, _individualAddress);
}

bool KnxTools::isActive() {
    return _initialized;
}

bool KnxTools::isFactorySetting() {
    return _deviceFlags == 0xff;
}

// bytes to skip when reading/writing in param-table

int KnxTools::calcParamSkipBytes(byte index) {
    // calc bytes to skip
    int skipBytes = 0;
    if (index > 0) {
        for (byte i = 0; i < index - 1; i++) {
            skipBytes += getParamSize(i);
        }
    }
    return skipBytes;
}

byte KnxTools::getParamSize(byte index) {
    return _paramLenghtList[index];
}

void KnxTools::getParamValue(int index, byte value[]) {

    if (index > _paramsNb){
        return;
    }
    
    int skipBytes = calcParamSkipBytes(index);
    int paramLen = getParamSize(index);

    // read byte by byte
    for (byte i = 0; i < paramLen; i++) {
        value[i] = EEPROM.read(_paramTableStartindex + skipBytes + i);
    }
}

// local helper method got the prog-button-interrupt

void KnxToolsProgButtonPressed() {
    CONSOLEDEBUGLN("PROGBUTTON toggle");
    Tools.toggleProgState();
}

/*
 * toggle the actually ProgState
 */
void KnxTools::toggleProgState() {
    _progState = !_progState; // toggle
    setProgState(_progState); // set
}

/*
 * Sets thep prog state to given boolean value
 * @param state new prog state
 */
void KnxTools::setProgState(bool state) {
    if (state == true) {
        _progState = true;
        digitalWrite(_progLED, HIGH);
        CONSOLEDEBUGLN("PROGBUTTON 1");
    } else if (state == false) {
        _progState = false;
        digitalWrite(_progLED, LOW);
        CONSOLEDEBUGLN("PROGBUTTON 0");
    }
}

KnxComObject KnxTools::createProgComObject() {
    CONSOLEDEBUGLN("createProgComObject");
    return /* Index 0 */ KnxComObject(G_ADDR(15,7,255), KNX_DPT_60000_000 /* KNX PROGRAM */, KNX_COM_OBJ_C_W_U_T_INDICATOR); /* NEEDS TO BE THERE FOR PROGRAMMING PURPOSE */
}

/**
 * Reboot device via WatchDogTimer within 1s
 */
void KnxTools::reboot() {
//    wdt_enable( WDTO_500MS ); 
    wdt_enable( WDTO_1S ); 
    while(1) {}
}

bool KnxTools::internalComObject(byte index) {

    CONSOLEDEBUGLN("-----------------------------");
    CONSOLEDEBUG("internalComObject index=");
    CONSOLEDEBUGLN(index);
    bool consumed = false;
    switch (index) {
        case 0: // object index 0 has been updated

            
            CONSOLEDEBUGLN("About to read 14 bytes");
            byte buffer[14];
            Knx.read(0, buffer);
            CONSOLEDEBUGLN("done reading 14 bytes");

            for (int i = 0; i < 14; i++) {
                CONSOLEDEBUG("buffer[");
                CONSOLEDEBUG(i);
                CONSOLEDEBUG("]      hex=");
                CONSOLEDEBUG(buffer[i], HEX);
                CONSOLEDEBUG("\tbin=");
                CONSOLEDEBUGLN(buffer[i], BIN);
            }

            byte protocolversion = buffer[0];
            byte msgType = buffer[1];

            CONSOLEDEBUG("protocolversion=");
            CONSOLEDEBUGLN(protocolversion,HEX);
            
            CONSOLEDEBUG("msgType=");
            CONSOLEDEBUGLN(msgType,HEX);

            if (protocolversion != PROTOCOLVERSION) {
                CONSOLEDEBUG("Unsupported protocol version. Using ");
                CONSOLEDEBUG(PROTOCOLVERSION);
                CONSOLEDEBUG(" Got: ");
                CONSOLEDEBUG(protocolversion);
                CONSOLEDEBUGLN("!");
            } else {

                switch (msgType) {
                    case MSGTYPE_READ_DEVICE_INFO:
                        handleMsgReadDeviceInfo(buffer);
                        break;
                    case MSGTYPE_RESTART:
                        handleMsgRestart(buffer);
                        break;
                    case MSGTYPE_WRITE_PROGRAMMING_MODE:
                        handleMsgWriteProgrammingMode(buffer);
                        break;
                    case MSGTYPE_READ_PROGRAMMING_MODE:
                        handleMsgReadProgrammingMode(buffer);
                        break;
                    case MSGTYPE_WRITE_INDIVIDUAL_ADDRESS:
                        if (_progState) handleMsgWriteIndividualAddress(buffer);
                        break;
                    case MSGTYPE_READ_INDIVIDUAL_ADDRESS:
                        if (_progState) handleMsgReadIndividualAddress(buffer);
                        break;
                    case MSGTYPE_WRITE_PARAMETER:
                        if (_progState) handleMsgWriteParameter(buffer);
                        break;
                    case MSGTYPE_READ_PARAMETER:
                        handleMsgReadParameter(buffer);
                        break;
                    case MSGTYPE_WRITE_COM_OBJECT:
                        if (_progState) handleMsgWriteComObject(buffer);
                        break;
                    case MSGTYPE_READ_COM_OBJECT:
                        handleMsgReadComObject(buffer);
                        break;
                    default:
                        CONSOLEDEBUG("Unsupported msgtype:");
                        CONSOLEDEBUG(msgType);
                        CONSOLEDEBUGLN("!");
                        break;
                }

            }
            consumed = true;
            break;

    }
    return consumed;

}

void KnxTools::sendAck(byte errorcode, byte indexinformation){
    CONSOLEDEBUG("sendAck errorcode=");
    CONSOLEDEBUG(errorcode, HEX);
    CONSOLEDEBUG("indexinformation=");
    CONSOLEDEBUGLN(indexinformation, HEX);
    byte response[14];
    response[0] = PROTOCOLVERSION;
    response[1] = MSGTYPE_ACK;
    response[2] = errorcode;
    response[3] = indexinformation;
    for (byte i=4;i<14;i++){
        response[i] = 0;
    }
    Knx.write(0, response);    
}


void KnxTools::handleMsgReadDeviceInfo(byte msg[]) {
    CONSOLEDEBUGLN("handleMsgReadDeviceInfo");
    byte response[14];
    response[0] = PROTOCOLVERSION;
    response[1] = MSGTYPE_ANSWER_DEVICE_INFO;
    response[2] = (_manufacturerID >> 8) & 0xff;
    response[3] = (_manufacturerID >> 0) & 0xff;
    response[4] = _deviceID;
    response[5] = _revisionID;
    response[6] = _deviceFlags;
    response[7] = (_individualAddress >> 8) & 0xff;
    response[8] = (_individualAddress >> 0) & 0xff;
    response[9] = 0;
    response[10] = 0;
    response[11] = 0;
    response[12] = 0;
    response[13] = 0;
    Knx.write(0, response);
}

void KnxTools::handleMsgRestart(byte msg[]) {
    CONSOLEDEBUGLN("handleMsgRestart");
    
    byte hi = (_individualAddress >> 8) & 0xff;
    byte lo = (_individualAddress >> 0) & 0xff;
    
    if (hi==msg[2] && lo==msg[3]) {
        CONSOLEDEBUGLN("matching IA");    
        // trigger restart
        reboot();
    } else {
        CONSOLEDEBUGLN("no matching IA");
    }
    
}

void KnxTools::handleMsgWriteProgrammingMode(byte msg[]) {
    CONSOLEDEBUGLN("handleMsgWriteProgrammingMode");
    //word addr = (msg[2] << 8) + (msg[3] << 0);
    
    byte ownHI = (_individualAddress >> 8) & 0xff;
    byte ownLO = (_individualAddress >> 0) & 0xff;
    if (msg[2] == ownHI && msg[3] == ownLO) {
        CONSOLEDEBUGLN("match");
        setProgState(msg[4] == 0x01); 
    } else {
        CONSOLEDEBUGLN("no match");
    }
    sendAck(0x00, 0x00);
}

void KnxTools::handleMsgReadProgrammingMode(byte msg[]) {
    CONSOLEDEBUGLN("handleMsgReadProgrammingMode");
    if (_progState) {
        byte response[14];
        response[0] = PROTOCOLVERSION;
        response[1] = MSGTYPE_ANSWER_PROGRAMMING_MODE;
        response[2] = (_individualAddress >> 8) & 0xff;
        response[3] = (_individualAddress >> 0) & 0xff;
        response[4] = 0;
        response[5] = 0;
        response[6] = 0;
        response[7] = 0;
        response[8] = 0;
        response[9] = 0;
        response[10] = 0;
        response[11] = 0;
        response[12] = 0;
        response[13] = 0;
        Knx.write(0, response);
    }
}

void KnxTools::handleMsgWriteIndividualAddress(byte msg[]) {
    CONSOLEDEBUGLN("handleMsgWriteIndividualAddress");
#if defined(WRITEMEM)    
    EEPROM.update(EEPROM_INDIVIDUALADDRESS_HI, msg[2]);
    delay(10); // required?

    EEPROM.update(EEPROM_INDIVIDUALADDRESS_LO, msg[3]);
    delay(10); // required?

    _deviceFlags |= 0x01; // add bit for "not using factory setting"
    EEPROM.update(EEPROM_DEVICE_FLAGS, _deviceFlags);
    delay(10); // required?
#endif    
    _individualAddress = (msg[2] << 8) + (msg[3] << 0);
    sendAck(0x00, 0x00);
}

void KnxTools::handleMsgReadIndividualAddress(byte msg[]) {
    CONSOLEDEBUGLN("handleMsgReadIndividualAddress");
    byte response[14];
    response[0] = PROTOCOLVERSION;
    response[1] = MSGTYPE_ANSWER_INDIVIDUAL_ADDRESS;
    response[2] = (_individualAddress >> 8) & 0xff;
    response[3] = (_individualAddress >> 0) & 0xff;
    response[4] = 0;
    response[5] = 0;
    response[6] = 0;
    response[7] = 0;
    response[8] = 0;
    response[9] = 0;
    response[10] = 0;
    response[11] = 0;
    response[12] = 0;
    response[13] = 0;
    Knx.write(0, response);
}

void KnxTools::handleMsgWriteParameter(byte msg[]) {
    CONSOLEDEBUGLN("handleMsgWriteParameter");

    
    byte index = msg[0];
    // FIXME check param index --> NACK
    
    if (index > _paramsNb) {
        sendAck(KNX_DEVICE_INVALID_INDEX, index);
    }
    
    int skipBytes = calcParamSkipBytes(index);
    int paramLen = getParamSize(index);
#if defined(WRITEMEM)    
    // write byte by byte
    for (byte i = 0; i < paramLen; i++) {
        EEPROM.update(_paramTableStartindex + skipBytes + i, msg[3 + i]);
        delay(10); // really required?
    }
#endif
    sendAck(0x00, 0x00);
}

void KnxTools::handleMsgReadParameter(byte msg[]) {
    CONSOLEDEBUGLN("handleMsgReadParameter");
    byte index = msg[0];

    byte paramSize = getParamSize(index);

    byte paramValue[paramSize];
    getParamValue(index, paramValue);

    byte response[14];
    response[0] = PROTOCOLVERSION;
    response[1] = MSGTYPE_ANSWER_PARAMETER;
    response[2] = index;

    // fill in param value
    for (byte i = 0; i < paramSize; i++) {
        response[3 + i] = paramValue[i];
    }

    // fill rest with 0x00
    for (byte i = 0; i < 11 /* max param length */ - paramSize; i++) {
        response[3 + paramSize + i] = 0;
    }

    Knx.write(0, response);

}

void KnxTools::handleMsgWriteComObject(byte msg[]) {
    CONSOLEDEBUGLN("handleMsgWriteComObject");
    byte tupels = msg[2];

    for (byte tupelNumber = 0; tupelNumber < tupels; tupelNumber++) {
        
        byte tupelOffset = 3 + ((tupelNumber - 1)*3);
        
        byte comObjId = msg[tupelOffset + 0];
        byte gaHi = msg[tupelOffset + 1];
        byte gaLo = msg[tupelOffset + 2];
        
        word ga = (gaHi << 8) + (gaLo << 0);
        e_KnxDeviceStatus result = Knx.setComObjectAddress(comObjId, ga);
        if (result != KNX_DEVICE_ERROR) {
            sendAck(result, comObjId);
        } else {
#if defined(WRITEMEM)            
        // write to eeprom?!
#endif
        }
    }
    sendAck(0x00, 0x00);
}

void KnxTools::handleMsgReadComObject(byte msg[]) {
    CONSOLEDEBUGLN("handleMsgReadComObject");
    byte numberOfComObjects = msg[2];

    byte response[14];
    response[0] = PROTOCOLVERSION;
    response[1] = MSGTYPE_ANSWER_COM_OBJECT;
    response[2] = numberOfComObjects;
    
    for (byte i=0; i<numberOfComObjects; i++) {
        
        byte comObjId = msg[3+i];
        word ga = Knx.getComObjectAddress(comObjId);
        
        byte tupelOffset = 3 + ((i - 1)*3);
        response[tupelOffset+0] = comObjId;
        response[tupelOffset+1] = (ga >> 8) & 0xff; // GA Hi
        response[tupelOffset+2] = (ga >> 0) & 0xff; // GA Lo
        
    }

    // fill rest with 0x00
    for (byte i = 0; i < 11 - (numberOfComObjects*3); i++) {
        response[3 + (numberOfComObjects*3) + i] = 0;
    }

    Knx.write(0, response);

}

