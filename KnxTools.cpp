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
#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif

/*
 * !!!!! IMPORTANT !!!!!
 * if "#define DEBUG" is set, you must run your KONNEKTING Suite with "-Dde.root1.slicknx.konnekting.debug=true" 
 * A release-version of your development MUST NOT contain "#define DEBUG" ...
 */
#define DEBUG

// DEBUG PROTOCOL HANDLING
#define DEBUG_PROTOCOL

// comment just for debugging purpose to disable memory write
#define WRITEMEM 1

#ifdef DEBUG
#include <SoftwareSerial.h>
SoftwareSerial knxToolsDebugSerial(10, 11); // RX, TX
#define CONSOLEDEBUG(...)  knxToolsDebugSerial.print(__VA_ARGS__);
#define CONSOLEDEBUGLN(...)  knxToolsDebugSerial.println(__VA_ARGS__);
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

    CONSOLEDEBUG(F("\n\nknxToolsEvents index="));
    CONSOLEDEBUG(index);
    CONSOLEDEBUGLN(F(" "));

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
    knxToolsDebugSerial.begin(9600);
#endif    
    CONSOLEDEBUGLN(F("\n\n\n\nSetup KnxTools"));

#ifdef ESP8266
    CONSOLEDEBUG("Setup ESP8266 ... ");

    // disable WIFI
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    delay(100);

    // enable 1k EEPROM on ESP8266 platform
    EEPROM.begin(1024);

    CONSOLEDEBUGLN("*DONE*");
#endif    

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
    CONSOLEDEBUG(F("Manufacturer: "));
    CONSOLEDEBUG(_manufacturerID, HEX);
    CONSOLEDEBUGLN(F("hex"));

    CONSOLEDEBUG("Device: ");
    CONSOLEDEBUG(_deviceID, HEX);
    CONSOLEDEBUGLN(F("hex"));

    CONSOLEDEBUG(F("Revision: "));
    CONSOLEDEBUG(_revisionID, HEX);
    CONSOLEDEBUGLN(F("hex"));

    CONSOLEDEBUG(F("numberOfCommObjects: "));
    CONSOLEDEBUGLN(Knx.getNumberOfComObjects());

    // calc index of parameter table in eeprom --> depends on number of com objects
    _paramTableStartindex = EEPROM_COMOBJECTTABLE_START + (Knx.getNumberOfComObjects() * 3);

    _deviceFlags = EEPROM.read(EEPROM_DEVICE_FLAGS);

    CONSOLEDEBUG(F("_deviceFlags: "));
    CONSOLEDEBUG(_deviceFlags, BIN);
    CONSOLEDEBUGLN(F("bin"));

    _individualAddress = P_ADDR(1, 1, 254);
    if (!isFactorySetting()) {
        CONSOLEDEBUGLN(F("Using EEPROM"));
        /*
         * Read eeprom stuff
         */

        // PA
        byte hiAddr = EEPROM.read(EEPROM_INDIVIDUALADDRESS_HI);
        byte loAddr = EEPROM.read(EEPROM_INDIVIDUALADDRESS_LO);
        _individualAddress = (hiAddr << 8) + (loAddr << 0);

        // ComObjects
        // at most 255 com objects
        for (byte i = 0; i < Knx.getNumberOfComObjects() - 1; i++) {
            byte hi = EEPROM.read(EEPROM_COMOBJECTTABLE_START + (i * 3));
            byte lo = EEPROM.read(EEPROM_COMOBJECTTABLE_START + (i * 3) + 1);
            byte settings = EEPROM.read(EEPROM_COMOBJECTTABLE_START + (i * 3) + 2);
            word comObjAddr = (hi << 8) + (lo << 0);

            bool active = ((settings & 0x80) == 0x80);
            Knx.setComObjectAddress((i + 1), comObjAddr, active);

            CONSOLEDEBUG(F("ComObj index="));
            CONSOLEDEBUG((i + 1));
            CONSOLEDEBUG(F(" Suite-ID="));
            CONSOLEDEBUG(i);
            CONSOLEDEBUG(F(" HI: 0x"));
            CONSOLEDEBUG(hi, HEX);
            CONSOLEDEBUG(F(" LO: 0x"));
            CONSOLEDEBUG(lo, HEX);
            CONSOLEDEBUG(F(" GA: 0x"));
            CONSOLEDEBUG(comObjAddr, HEX);
            CONSOLEDEBUG(F(" Settings: 0x"));
            CONSOLEDEBUG(settings, HEX);
            CONSOLEDEBUG(F(" Active: "));
            CONSOLEDEBUG(active, DEC);
            CONSOLEDEBUGLN(F(""));
        }

    } else {
        CONSOLEDEBUGLN(F("Using FACTORY"));
    }
    CONSOLEDEBUG(F("IA: 0x"));
    CONSOLEDEBUGLN(_individualAddress, HEX);
    e_KnxDeviceStatus status;
    status = Knx.begin(serial, _individualAddress);
    CONSOLEDEBUG(F("KnxDevice startup status: 0x"));
    CONSOLEDEBUG(status, HEX);
    CONSOLEDEBUGLN(F(""));

    if (status != KNX_DEVICE_OK) {
        CONSOLEDEBUGLN(F("Knx init ERROR. retry after reboot!!"));
        delay(500);
        reboot();
    }
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
        for (byte i = 0; i < index; i++) {
            skipBytes += getParamSize(i);
        }
    }
    return skipBytes;
}

byte KnxTools::getParamSize(byte index) {
    return _paramSizeList[index];
}

void KnxTools::getParamValue(int index, byte value[]) {

    if (index > _numberOfParams - 1) {
        return;
    }

    int skipBytes = calcParamSkipBytes(index);
    int paramLen = getParamSize(index);

    CONSOLEDEBUG(F("getParamValue: index="));
    CONSOLEDEBUG(index);
    CONSOLEDEBUG(F(" _paramTableStartindex="));
    CONSOLEDEBUG(_paramTableStartindex);
    CONSOLEDEBUG(F(" skipBytes="));
    CONSOLEDEBUG(skipBytes);
    CONSOLEDEBUG(F(" paramLen="));
    CONSOLEDEBUG(paramLen);
    CONSOLEDEBUGLN(F(""));

    // read byte by byte
    for (byte i = 0; i < paramLen; i++) {

        int addr = _paramTableStartindex + skipBytes + i;

        value[i] = EEPROM.read(addr);
        CONSOLEDEBUG(F(" val["));
        CONSOLEDEBUG(i);
        CONSOLEDEBUG(F("]@"));
        CONSOLEDEBUG(addr);
        CONSOLEDEBUG(F(" --> 0x"));
        CONSOLEDEBUG(value[i], HEX);
        CONSOLEDEBUGLN(F(""));
    }
}

// local helper method got the prog-button-interrupt

void KnxToolsProgButtonPressed() {
    CONSOLEDEBUGLN(F("PROGBUTTON toggle"));
    Tools.toggleProgState();
}

/*
 * User-toggle the actually ProgState
 */
void KnxTools::toggleProgState() {
    _progState = !_progState; // toggle
    setProgState(_progState); // set
    if (_rebootRequired) {
        CONSOLEDEBUGLN(F("EEPROM changed, triggering reboot"));
        reboot();
    }
}

/**
 * Gets programming state
 * @return true, if programming is active, false if not
 */
bool KnxTools::getProgState() {
    return _progState ? true : false;
}

/*
 * Sets thep prog state to given boolean value
 * @param state new prog state
 */
void KnxTools::setProgState(bool state) {
    if (state == true) {
        _progState = true;
        digitalWrite(_progLED, HIGH);
        CONSOLEDEBUGLN(F("PROGBUTTON 1"));
    } else if (state == false) {
        _progState = false;
        digitalWrite(_progLED, LOW);
        CONSOLEDEBUGLN(F("PROGBUTTON 0"));
    }
}

bool KnxTools::isMatchingIA(byte hi, byte lo) {
    byte iaHi = (_individualAddress >> 8) & 0xff;
    byte iaLo = (_individualAddress >> 0) & 0xff;

    return (hi == iaHi && lo == iaLo);
}

KnxComObject KnxTools::createProgComObject() {
    CONSOLEDEBUGLN(F("createProgComObject"));
    KnxComObject p = KnxComObject(KNX_DPT_60000_000 /* KNX PROGRAM */, KNX_COM_OBJ_C_W_U_T_INDICATOR); /* NEEDS TO BE THERE FOR PROGRAMMING PURPOSE */
    p.SetAddr(G_ADDR(15, 7, 255));
    p.setActive(true);
    return /* Index 0 */ p;
}

/**
 * Reboot device via WatchDogTimer within 1s
 */
void KnxTools::reboot() {
    Knx.end();

#ifdef ESP8266 
    CONSOLEDEBUGLN("ESP8266 restart");
    ESP.restart();
#endif

#ifdef __AVR_ATmega328P__
    // to overcome WDT infinite reboot-loop issue
    // see: https://github.com/arduino/Arduino/issues/4492
    CONSOLEDEBUGLN(F("software reset NOW"));
    delay(500);
    asm volatile ("  jmp 0");
#else     
    CONSOLEDEBUGLN(F("WDT reset NOW"));
    wdt_enable(WDTO_500MS);
    while (1) {
    }
#endif    

}

bool KnxTools::internalComObject(byte index) {

    CONSOLEDEBUG(F("internalComObject index="));
    CONSOLEDEBUGLN(index);
    bool consumed = false;
    switch (index) {
        case 0: // object index 0 has been updated


            //            CONSOLEDEBUGLN("About to read 14 bytes");
            byte buffer[14];
            Knx.read(0, buffer);
            //            CONSOLEDEBUGLN("done reading 14 bytes");
#ifdef DEBUG_PROTOCOL
            for (int i = 0; i < 14; i++) {
                CONSOLEDEBUG(F("buffer["));
                CONSOLEDEBUG(i);
                CONSOLEDEBUG(F("]\thex=0x"));
                CONSOLEDEBUG(buffer[i], HEX);
                CONSOLEDEBUG(F("  \tbin="));
                CONSOLEDEBUGLN(buffer[i], BIN);
            }
#endif

            byte protocolversion = buffer[0];
            byte msgType = buffer[1];

            CONSOLEDEBUG(F("protocolversion=0x"));
            CONSOLEDEBUGLN(protocolversion, HEX);

            CONSOLEDEBUG(F("msgType=0x"));
            CONSOLEDEBUGLN(msgType, HEX);

            if (protocolversion != PROTOCOLVERSION) {
                CONSOLEDEBUG(F("Unsupported protocol version. Using "));
                CONSOLEDEBUG(PROTOCOLVERSION);
                CONSOLEDEBUG(F(" Got: "));
                CONSOLEDEBUG(protocolversion);
                CONSOLEDEBUGLN(F("!"));
            } else {

                switch (msgType) {
                    case MSGTYPE_ACK:
                        CONSOLEDEBUGLN(F("Will not handle received ACK. Skipping message."));
                        break;
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
                        CONSOLEDEBUG(F("Unsupported msgtype: 0x"));
                        CONSOLEDEBUG(msgType, HEX);
                        CONSOLEDEBUGLN(F(" !!! Skipping message."));
                        break;
                }

            }
            consumed = true;
            break;

    }
    return consumed;

}

void KnxTools::sendAck(byte errorcode, byte indexinformation) {
    CONSOLEDEBUG(F("sendAck errorcode=0x"));
    CONSOLEDEBUG(errorcode, HEX);
    CONSOLEDEBUG(F(" indexinformation=0x"));
    CONSOLEDEBUGLN(indexinformation, HEX);
    byte response[14];
    response[0] = PROTOCOLVERSION;
    response[1] = MSGTYPE_ACK;
    response[2] = (errorcode == 0x00 ? 0x00 : 0xFF);
    response[3] = errorcode;
    response[4] = indexinformation;
    for (byte i = 5; i < 14; i++) {
        response[i] = 0x00;
    }
    Knx.write(0, response);
}

void KnxTools::handleMsgReadDeviceInfo(byte msg[]) {
    CONSOLEDEBUGLN(F("handleMsgReadDeviceInfo"));

    if (isMatchingIA(msg[2], msg[3])) {
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
        response[9] = 0x00;
        response[10] = 0x00;
        response[11] = 0x00;
        response[12] = 0x00;
        response[13] = 0x00;
        Knx.write(0, response);
    } else {
#ifdef DEBUG_PROTOCOL
        CONSOLEDEBUGLN(F("no matching IA"));
#endif        
    }
}

void KnxTools::handleMsgRestart(byte msg[]) {
    CONSOLEDEBUGLN(F("handleMsgRestart"));

    if (isMatchingIA(msg[2], msg[3])) {
#ifdef DEBUG_PROTOCOL
        CONSOLEDEBUGLN(F("matching IA"));
#endif
        // trigger restart
        reboot();
    } else {
#ifdef DEBUG_PROTOCOL
        CONSOLEDEBUGLN(F("no matching IA"));
#endif
    }

}

void KnxTools::handleMsgWriteProgrammingMode(byte msg[]) {
    CONSOLEDEBUGLN(F("handleMsgWriteProgrammingMode"));
    //word addr = (msg[2] << 8) + (msg[3] << 0);


    if (isMatchingIA(msg[2], msg[3])) {
#ifdef DEBUG_PROTOCOL
        CONSOLEDEBUGLN(F("matching IA"));
#endif
        setProgState(msg[4] == 0x01);
        sendAck(0x00, 0x00);
#ifdef ESP8266
        if (msg[4] == 0x00) {
            CONSOLEDEBUGLN("ESP8266: EEPROM.commit()");
            EEPROM.commit();
        }
#endif                

    } else {
        CONSOLEDEBUGLN(F("no matching IA"));
    }
}

void KnxTools::handleMsgReadProgrammingMode(byte msg[]) {
    CONSOLEDEBUGLN(F("handleMsgReadProgrammingMode"));
    if (_progState) {
        byte response[14];
        response[0] = PROTOCOLVERSION;
        response[1] = MSGTYPE_ANSWER_PROGRAMMING_MODE;
        response[2] = (_individualAddress >> 8) & 0xff;
        response[3] = (_individualAddress >> 0) & 0xff;
        response[4] = 0x00;
        response[5] = 0x00;
        response[6] = 0x00;
        response[7] = 0x00;
        response[8] = 0x00;
        response[9] = 0x00;
        response[10] = 0x00;
        response[11] = 0x00;
        response[12] = 0x00;
        response[13] = 0x00;
        Knx.write(0, response);
    }
}

void KnxTools::handleMsgWriteIndividualAddress(byte msg[]) {
    CONSOLEDEBUGLN(F("handleMsgWriteIndividualAddress"));
#if defined(WRITEMEM)    
    memoryUpdate(EEPROM_INDIVIDUALADDRESS_HI, msg[2]);
    memoryUpdate(EEPROM_INDIVIDUALADDRESS_LO, msg[3]);

    // see: http://stackoverflow.com/questions/3920307/how-can-i-remove-a-flag-in-c
    _deviceFlags &= ~0x80; // remove factory setting bit (left most bit))
#ifdef DEBUG_PROTOCOL
    CONSOLEDEBUG(F("DeviceFlags after =0x"));
    CONSOLEDEBUG(_deviceFlags, HEX);
    CONSOLEDEBUGLN(F(""));
#endif

    memoryUpdate(EEPROM_DEVICE_FLAGS, _deviceFlags);
#endif    
    _individualAddress = (msg[2] << 8) + (msg[3] << 0);
    sendAck(0x00, 0x00);
}

void KnxTools::handleMsgReadIndividualAddress(byte msg[]) {
    CONSOLEDEBUGLN(F("handleMsgReadIndividualAddress"));
    byte response[14];
    response[0] = PROTOCOLVERSION;
    response[1] = MSGTYPE_ANSWER_INDIVIDUAL_ADDRESS;
    response[2] = (_individualAddress >> 8) & 0xff;
    response[3] = (_individualAddress >> 0) & 0xff;
    response[4] = 0x00;
    response[5] = 0x00;
    response[6] = 0x00;
    response[7] = 0x00;
    response[8] = 0x00;
    response[9] = 0x00;
    response[10] = 0x00;
    response[11] = 0x00;
    response[12] = 0x00;
    response[13] = 0x00;
    Knx.write(0, response);
}

void KnxTools::handleMsgWriteParameter(byte msg[]) {
    CONSOLEDEBUGLN(F("handleMsgWriteParameter"));

    byte index = msg[2];

    if (index > _numberOfParams - 1) {
        sendAck(KNX_DEVICE_INVALID_INDEX, index);
        return;
    }

    int skipBytes = calcParamSkipBytes(index);
    int paramLen = getParamSize(index);

#ifdef DEBUG_PROTOCOL
    CONSOLEDEBUG(F("id="));
    CONSOLEDEBUG(index);
    CONSOLEDEBUGLN(F(""))
#endif

#if defined(WRITEMEM)    
            // write byte by byte
    for (byte i = 0; i < paramLen; i++) {
#ifdef DEBUG_PROTOCOL
        CONSOLEDEBUG(F(" data["));
        CONSOLEDEBUG(i);
        CONSOLEDEBUG(F("]=0x"));
        CONSOLEDEBUG(msg[3 + i], HEX);
        CONSOLEDEBUGLN(F(""));
#endif
        //EEPROM.update(_paramTableStartindex + skipBytes + i, msg[3 + i]);
        memoryUpdate(_paramTableStartindex + skipBytes + i, msg[3 + i]);
    }
#endif
    sendAck(0x00, 0x00);
}

void KnxTools::handleMsgReadParameter(byte msg[]) {
    CONSOLEDEBUGLN(F("handleMsgReadParameter"));
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
    CONSOLEDEBUGLN(F("handleMsgWriteComObject"));

    byte comObjId = msg[2];
    byte gaHi = msg[3];
    byte gaLo = msg[4];
    byte settings = msg[5];
    word ga = (gaHi << 8) + (gaLo << 0);

#ifdef DEBUG_PROTOCOL
    CONSOLEDEBUG(F("CO id="));
    CONSOLEDEBUG(comObjId);
    CONSOLEDEBUG(F(" hi=0x"));
    CONSOLEDEBUG(gaHi, HEX);
    CONSOLEDEBUG(F(" lo=0x"));
    CONSOLEDEBUG(gaLo, HEX);
    CONSOLEDEBUG(F(" ga=0x"));
    CONSOLEDEBUG(ga, HEX);
    CONSOLEDEBUG(F(" settings=0x"));
    CONSOLEDEBUG(settings, HEX);
    CONSOLEDEBUGLN(F(""));

    bool foundWrongUndefined = false;
    for (int i = 6; i < 13; i++) {
        if (msg[i] != 0x00) foundWrongUndefined = true;
    }
    if (foundWrongUndefined) {
        CONSOLEDEBUGLN("!!!!!!!!!!! WARNING: Suite is sending wrong data. Ensure Suite version matches the Device Lib !!!!!");
    }
#endif        

    //    e_KnxDeviceStatus result = Knx.setComObjectAddress(comObjId, ga);
    //    if (result != KNX_DEVICE_OK) {
    //        CONSOLEDEBUG(F("ERROR="));
    //        CONSOLEDEBUGLN(result);
    //        // fehler!
    //        sendAck(result, comObjId);
    //        return;
    //    } else {

    if (comObjId >= Knx.getNumberOfComObjects()) {
        sendAck(KNX_DEVICE_INVALID_INDEX, comObjId);
        return;
    }

#if defined(WRITEMEM)            
    // write to eeprom?!
    memoryUpdate(EEPROM_COMOBJECTTABLE_START + (comObjId * 3) + 0, gaHi);
    memoryUpdate(EEPROM_COMOBJECTTABLE_START + (comObjId * 3) + 1, gaLo);
    memoryUpdate(EEPROM_COMOBJECTTABLE_START + (comObjId * 3) + 2, settings);
#endif
    //    }

    sendAck(0x00, 0x00);
}

void KnxTools::handleMsgReadComObject(byte msg[]) {
#ifdef DEBUG_PROTOCOL
    CONSOLEDEBUGLN(F("handleMsgReadComObject"));
#endif

    byte comObjId = msg[2];


    word ga = Knx.getComObjectAddress(comObjId);

    byte response[14];
    response[0] = PROTOCOLVERSION;
    response[1] = MSGTYPE_ANSWER_COM_OBJECT;
    response[2] = comObjId;
    response[3] = (ga >> 8) & 0xff; // GA Hi
    response[4] = (ga >> 0) & 0xff; // GA Lo
    response[5] = 0x00; // Settings

    // fill rest with 0x00
    for (byte i = 6; i < 13; i++) {
        response[i] = 0;
    }

    Knx.write(0, response);
}

void KnxTools::memoryUpdate(int index, byte data) {


    CONSOLEDEBUG(F("memUpdate: index="));
    CONSOLEDEBUG(index);
    CONSOLEDEBUG(F(" data=0x"));
    CONSOLEDEBUG(data, HEX);
    CONSOLEDEBUGLN(F(""));

#ifdef ESP8266    
    CONSOLEDEBUGLN(F("ESP8266: EEPROM.update"));
    byte d = EEPROM.read(index);
    if (d != data) {
        EEPROM.write(index, data);
    }
#else
    EEPROM.update(index, data);
    //    delay(10); // really required?
#endif   

    // EEPROM has been changed, reboot will be required
    _rebootRequired = true;

}

/*
 *  #define PARAM_INT8 1
    #define PARAM_UINT8 1
    #define PARAM_INT16 2
    #define PARAM_UINT16 2
    #define PARAM_INT32 4
    #define PARAM_UINT32 4
 */

uint8_t KnxTools::getUINT8Param(byte index) {
    if (getParamSize(index) != PARAM_UINT8) {
        CONSOLEDEBUG(F("Requested UINT8 param for index "));
        CONSOLEDEBUG(index);
        CONSOLEDEBUG(F(" but param has different length! Will Return 0."));
        return 0;
    }

    byte paramValue[1];
    getParamValue(index, paramValue);

    return paramValue[0];
}

int8_t KnxTools::getINT8Param(byte index) {
    if (getParamSize(index) != PARAM_INT8) {
        CONSOLEDEBUG(F("Requested INT8 param for index "));
        CONSOLEDEBUG(index);
        CONSOLEDEBUG(F(" but param has different length! Will Return 0."));
        return 0;
    }

    byte paramValue[1];
    getParamValue(index, paramValue);

    return paramValue[0];
}

uint16_t KnxTools::getUINT16Param(byte index) {
    if (getParamSize(index) != PARAM_UINT16) {
        CONSOLEDEBUG(F("Requested UINT16 param for index "));
        CONSOLEDEBUG(index);
        CONSOLEDEBUG(F(" but param has different length! Will Return 0."));
        return 0;
    }

    byte paramValue[2];
    getParamValue(index, paramValue);

    uint16_t val = (paramValue[0] << 8) + (paramValue[1] << 0);

    return val;
}

int16_t KnxTools::getINT16Param(byte index) {
    if (getParamSize(index) != PARAM_INT16) {
        CONSOLEDEBUG(F("Requested INT16 param for index "));
        CONSOLEDEBUG(index);
        CONSOLEDEBUG(F(" but param has different length! Will Return 0."));
        return 0;
    }

    byte paramValue[2];
    getParamValue(index, paramValue);

    CONSOLEDEBUG(F(" int16: [1]=0x"));
    CONSOLEDEBUG(paramValue[0], HEX);
    CONSOLEDEBUG(F(" [0]=0x"));
    CONSOLEDEBUG(paramValue[1], HEX);
    CONSOLEDEBUGLN(F(""));

    int16_t val = (paramValue[0] << 8) + (paramValue[1] << 0);

    return val;
}

uint32_t KnxTools::getUINT32Param(byte index) {
    if (getParamSize(index) != PARAM_UINT32) {
        CONSOLEDEBUG(F("Requested UINT32 param for index "));
        CONSOLEDEBUG(index);
        CONSOLEDEBUG(F(" but param has different length! Will Return 0."));
        return 0;
    }

    byte paramValue[4];
    getParamValue(index, paramValue);

    uint32_t val = ((uint32_t) paramValue[0] << 24) + ((uint32_t) paramValue[1] << 16) + ((uint32_t) paramValue[2] << 8) + ((uint32_t) paramValue[3] << 0);

    return val;
}

int32_t KnxTools::getINT32Param(byte index) {
    if (getParamSize(index) != PARAM_INT32) {
        CONSOLEDEBUG(F("Requested UINT32 param for index "));
        CONSOLEDEBUG(index);
        CONSOLEDEBUG(F(" but param has different length! Will Return 0."));
        return 0;
    }

    byte paramValue[4];
    getParamValue(index, paramValue);

    int32_t val = ((uint32_t) paramValue[0] << 24) + ((uint32_t) paramValue[1] << 16) + ((uint32_t) paramValue[2] << 8) + ((uint32_t) paramValue[3] << 0);

    return val;
}

int KnxTools::getFreeEepromOffset() {

    int offset = _paramTableStartindex;
    for (int i = 0; i < _numberOfParams; i++) {
        offset += _paramSizeList[i];
    }
    return offset;
}