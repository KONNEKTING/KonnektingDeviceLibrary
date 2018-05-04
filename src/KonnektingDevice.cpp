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


/*
 * !!!!! IMPORTANT !!!!!
 * if "#define DEBUG" is set, you must run your KONNEKTING Suite with "-Dde.root1.slicknx.konnekting.debug=true" 
 * A release-version of your development MUST NOT contain "#define DEBUG" ...
 */

#define DEBUG_PROTOCOL
#define WRITEMEM

#include "Arduino.h"
#include "DebugUtil.h"
#include "KnxDevice.h"
#include "KnxComObject.h"
#include "KnxDPT.h"
#include "KonnektingDevice.h"

#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif

#ifdef __SAMD21G18A__
#include "SAMD21G18_Reset.h"
#endif

#define PROGCOMOBJ_INDEX 255


// KonnektingDevice unique instance creation
KonnektingDevice KonnektingDevice::Konnekting;
KonnektingDevice& Konnekting = KonnektingDevice::Konnekting;

/**
 * Intercepting knx events to process internal com objects
 * @param index
 */
void konnektingKnxEvents(byte index) {

    DEBUG_PRINTLN(F("\n\nkonnektingKnxEvents index=%d"), index);

    // if it's not a internal com object, route back to knxEvents()
    if (!Konnekting.internalComObject(index)) {
        knxEvents(index);
    }
}

/**
 * Constructor
 */
KonnektingDevice::KonnektingDevice() {

    DEBUG_PRINTLN(F("\n\n\n\nSetup KonnektingDevice"));

#ifdef ESP8266
    //DEBUG_PRINT(F("Setup ESP8266 ... "));

    // disable WIFI per default
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    delay(100);

    // enable 1k EEPROM on ESP8266 platform
    EEPROM.begin(1024);

    DEBUG_PRINTLN(F("*DONE*"));
#endif    

}

/**
 * Starts KNX KonnektingDevice, as well as KNX Device
 * 
 * @param serial serial port reference, f.i. "Serial" or "Serial1"
 * @param progButtonPin pin which drives LED when in programming mode
 * @param progLedPin pin which toggles programming mode, needs an interrupt capable pin!
 * @param manufacturerID
 * @param deviceID
 * @param revisionID
 * 
 */
void KonnektingDevice::init(HardwareSerial& serial,
        int progButtonPin,
        int progLedPin,
        word manufacturerID,
        byte deviceID,
        byte revisionID
        ) {

    DEBUG_PRINTLN(F("Initialize KonnektingDevice"));

    DEBUG_PRINTLN("15/7/255 = 0x%04x", G_ADDR(15, 7, 255));

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

#ifdef __SAMD21G18A__
    // sollte eigtl. auch mit "digitalPinToInterrupt" funktionieren, tut es mit dem zero aber irgendwie nicht?!
    attachInterrupt(_progButton, KonnektingProgButtonPressed, RISING);
#else    
    attachInterrupt(digitalPinToInterrupt(_progButton), KonnektingProgButtonPressed, RISING);
#endif    

    // hardcoded stuff
    DEBUG_PRINTLN(F("Manufacturer: 0x%02x Device: 0x%02x Revision: 0x%02x"), _manufacturerID, _deviceID, _revisionID);

    DEBUG_PRINTLN(F("numberOfCommObjects: %d"), Knx.getNumberOfComObjects());

    // calc index of parameter table in eeprom --> depends on number of com objects
    _paramTableStartindex = EEPROM_COMOBJECTTABLE_START + (Knx.getNumberOfComObjects() * 3);

    _deviceFlags = memoryRead(EEPROM_DEVICE_FLAGS);

    DEBUG_PRINTLN(F("_deviceFlags: " BYTETOBINARYPATTERN), BYTETOBINARY(_deviceFlags));

    _individualAddress = P_ADDR(1, 1, 254);
    if (!isFactorySetting()) {
        DEBUG_PRINTLN(F("->EEPROM"));
        /*
         * Read eeprom stuff
         */

        // PA
        byte hiAddr = memoryRead(EEPROM_INDIVIDUALADDRESS_HI);
        byte loAddr = memoryRead(EEPROM_INDIVIDUALADDRESS_LO);
        _individualAddress = (hiAddr << 8) + (loAddr << 0);

        // ComObjects
        // at most 254 com objects, 255 is progcomobj
        for (byte i = 0; i < Knx.getNumberOfComObjects(); i++) {
            byte hi = memoryRead(EEPROM_COMOBJECTTABLE_START + (i * 3));
            byte lo = memoryRead(EEPROM_COMOBJECTTABLE_START + (i * 3) + 1);
            byte settings = memoryRead(EEPROM_COMOBJECTTABLE_START + (i * 3) + 2);
            word comObjAddr = (hi << 8) + (lo << 0);

            bool active = ((settings & 0x80) == 0x80);
            Knx.setComObjectAddress(i, comObjAddr, active);

            DEBUG_PRINTLN(F("ComObj index=%d HI=0x%02x LO=0x%02x GA=0x%04x setting=0x%02x active=%d"), i, hi, lo, comObjAddr, settings, active);
        }

    } else {
        DEBUG_PRINTLN(F("->FACTORY"));
    }
    DEBUG_PRINTLN(F("IA: 0x%04x"), _individualAddress);
    e_KnxDeviceStatus status;
    status = Knx.begin(serial, _individualAddress);
    DEBUG_PRINTLN(F("KnxDevice startup status: 0x%02x"), status);

    if (status != KNX_DEVICE_OK) {
        DEBUG_PRINTLN(F("Knx init ERROR. Retry after reboot!!"));
        delay(500);
        reboot();
    }
}

bool KonnektingDevice::isActive() {
    return _initialized;
}

bool KonnektingDevice::isFactorySetting() {
    bool isFactory = (_deviceFlags == 0xff);
    //    DEBUG_PRINTLN(F("isFactorySetting: %d"), isFactory);
    return isFactory;
}

/*
 * Bytes to skip when reading/writing in param-table
 * @param parameter-id to calc skipbytes for
 * @return bytes to skip
 */
int KonnektingDevice::calcParamSkipBytes(byte index) {
    // calc bytes to skip
    int skipBytes = 0;
    if (index > 0) {
        for (byte i = 0; i < index; i++) {
            skipBytes += getParamSize(i);
        }
    }
    return skipBytes;
}

byte KonnektingDevice::getParamSize(byte index) {
    return _paramSizeList[index];
}

void KonnektingDevice::getParamValue(int index, byte value[]) {

    if (index > _numberOfParams - 1) {
        return;
    }

    int skipBytes = calcParamSkipBytes(index);
    int paramLen = getParamSize(index);

    DEBUG_PRINTLN(F("getParamValue: index=%d _paramTableStartindex=%d skipbytes=%d paremLen=%d"), index, _paramTableStartindex, skipBytes, paramLen);

    // read byte by byte
    for (byte i = 0; i < paramLen; i++) {

        int addr = _paramTableStartindex + skipBytes + i;

        value[i] = memoryRead(addr);
        DEBUG_PRINTLN(F(" val[%d]@%d -> 0x%02x"), i, addr, value[i]);
    }
}

/**
 * prog-button-interrupt ISR
 */
void KonnektingProgButtonPressed() {
    DEBUG_PRINTLN(F("PrgBtn toggle"));
    Konnekting.toggleProgState();
}

/**
 * User-toggle the actually ProgState
 */
void KonnektingDevice::toggleProgState() {
    _progState = !_progState; // toggle
    setProgState(_progState); // set
    if (_rebootRequired) {
        DEBUG_PRINTLN(F("EEPROM changed, triggering reboot"));
        reboot();
    }
}

/**
 * Gets programming state
 * @return true, if programming is active, false if not
 */
bool KonnektingDevice::isProgState() {
    return _progState;
}

/**
 * Check whether Konnekting is ready for application logic. 
 * (Means: not busy with programming-mode and not running with factory settings)
 * @return true if it's safe to run application logic
 */
bool KonnektingDevice::isReadyForApplication() {
    bool isReady = (!isProgState() && !isFactorySetting());
    return isReady;
}

/**
 * Sets thep prog state to given boolean value
 * @param state new prog state
 */
void KonnektingDevice::setProgState(bool state) {
    if (state == true) {
        _progState = true;
        digitalWrite(_progLED, HIGH);
        DEBUG_PRINTLN(F("PrgBtn 1"));
    } else if (state == false) {
        _progState = false;
        digitalWrite(_progLED, LOW);
        DEBUG_PRINTLN(F("PrgBtn 0"));
    }
}

/**
 * Check if given 2byte ID is matching the current set IA
 * @return true if match, false if not
 */
bool KonnektingDevice::isMatchingIA(byte hi, byte lo) {
    byte iaHi = (_individualAddress >> 8) & 0xff;
    byte iaLo = (_individualAddress >> 0) & 0xff;

    return (hi == iaHi && lo == iaLo);
}

/**
 * Creates inter prog-com-object
 * @return prog com object
 */
KnxComObject KonnektingDevice::createProgComObject() {
    DEBUG_PRINTLN(F("createProgComObject"));
    KnxComObject p = KnxComObject(KNX_DPT_60000_60000 /* KNX PROGRAM */, KNX_COM_OBJ_C_W_U_T_INDICATOR); /* NEEDS TO BE THERE FOR PROGRAMMING PURPOSE */
    p.SetAddr(G_ADDR(15, 7, 255));
    p.setActive(true);
    return /* Index 0 */ p;
}

/**
 * Reboot device 
 */
void KonnektingDevice::reboot() {
    Knx.end();

#ifdef ESP8266 
    DEBUG_PRINTLN(F("ESP8266 restart"));
    ESP.restart();
#elif __SAMD21G18A__
    // do reset of arduino zero
    setupWDT(0); // minimum period
    while (1) {
    }
#elif __AVR_ATmega328P__
    // to overcome WDT infinite reboot-loop issue
    // see: https://github.com/arduino/Arduino/issues/4492
    DEBUG_PRINTLN(F("software reset NOW"));
    delay(500);
    asm volatile ("  jmp 0");
#else     
    DEBUG_PRINTLN(F("WDT reset NOW"));
    wdt_enable(WDTO_500MS);
    while (1) {
    }
#endif    

}

/**
 * Check if given index is an internal comobject
 * @param index
 * @return true, if index was internal comobject and has been handled, false if not
 */
bool KonnektingDevice::internalComObject(byte index) {

    DEBUG_PRINTLN(F("internalComObject index=%d"), index);
    bool consumed = false;
    switch (index) {
        case 255: // prog com object index 255 has been updated

            byte buffer[14];
            Knx.read(PROGCOMOBJ_INDEX, buffer);
#ifdef DEBUG_PROTOCOL
            for (int i = 0; i < 14; i++) {
                DEBUG_PRINTLN(F("buffer[%d]\thex=0x%02x bin=" BYTETOBINARYPATTERN), i, buffer[i], BYTETOBINARY(buffer[i]));
            }
#endif

            byte protocolversion = buffer[0];
            byte msgType = buffer[1];

            DEBUG_PRINTLN(F("protocolversion=0x%02x"), protocolversion);

            DEBUG_PRINTLN(F("msgType=0x%02x"), msgType);

            if (protocolversion != PROTOCOLVERSION) {
                DEBUG_PRINTLN(F("Unsupported protocol version. Using: %d Got: %d !"), PROTOCOLVERSION, protocolversion);
            } else {

                switch (msgType) {
                    case MSGTYPE_ACK:
                        DEBUG_PRINTLN(F("Will not handle received ACK. Skipping message."));
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
                        DEBUG_PRINTLN(F("Unsupported msgtype: 0x%02x"), msgType);
                        DEBUG_PRINTLN(F(" !!! Skipping message."));
                        break;
                }

            }
            consumed = true;
            break;

    }
    return consumed;

}

void KonnektingDevice::sendAck(byte errorcode, byte indexinformation) {
    DEBUG_PRINTLN(F("sendAck errorcode=0x%02x indexInformation=0x%02x"), errorcode, indexinformation);
    byte response[14];
    response[0] = PROTOCOLVERSION;
    response[1] = MSGTYPE_ACK;
    response[2] = (errorcode == 0x00 ? 0x00 : 0xFF);
    response[3] = errorcode;
    response[4] = indexinformation;
    for (byte i = 5; i < 14; i++) {
        response[i] = 0x00;
    }
    Knx.write(PROGCOMOBJ_INDEX, response);
}

void KonnektingDevice::handleMsgReadDeviceInfo(byte msg[]) {
    DEBUG_PRINTLN(F("handleMsgReadDeviceInfo"));

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
        Knx.write(PROGCOMOBJ_INDEX, response);
    } else {
#ifdef DEBUG_PROTOCOL
        DEBUG_PRINTLN(F("no matching IA"));
#endif        
    }
}

void KonnektingDevice::handleMsgRestart(byte msg[]) {
    DEBUG_PRINTLN(F("handleMsgRestart"));

    if (isMatchingIA(msg[2], msg[3])) {
#ifdef DEBUG_PROTOCOL
        DEBUG_PRINTLN(F("matching IA"));
#endif
        // trigger restart
        reboot();
    } else {
#ifdef DEBUG_PROTOCOL
        DEBUG_PRINTLN(F("no matching IA"));
#endif
    }

}

void KonnektingDevice::handleMsgWriteProgrammingMode(byte msg[]) {
    DEBUG_PRINTLN(F("handleMsgWriteProgrammingMode"));
    //word addr = (msg[2] << 8) + (msg[3] << 0);


    if (isMatchingIA(msg[2], msg[3])) {
#ifdef DEBUG_PROTOCOL
        DEBUG_PRINTLN(F("matching IA"));
#endif
        setProgState(msg[4] == 0x01);
        sendAck(0x00, 0x00);
        
#ifdef ESP8266
        // ESP8266 uses own EEPROM implementation which requires commit() call
        if (msg[4] == 0x00) {
            DEBUG_PRINTLN(F("ESP8266: EEPROM.commit()"));
            EEPROM.commit();
        }
#else
        // commit memory changes
        memoryCommit();
#endif                

    } else {
        DEBUG_PRINTLN(F("no matching IA"));
    }
}

void KonnektingDevice::handleMsgReadProgrammingMode(byte /*msg*/[]) {
    // to suppress compiler warning about unused variable, "msg" has been commented out
    DEBUG_PRINTLN(F("handleMsgReadProgrammingMode"));
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
        Knx.write(PROGCOMOBJ_INDEX, response);
    }
}

void KonnektingDevice::handleMsgWriteIndividualAddress(byte msg[]) {
    DEBUG_PRINTLN(F("handleMsgWriteIndividualAddress"));
#if defined(WRITEMEM)    
    memoryUpdate(EEPROM_INDIVIDUALADDRESS_HI, msg[2]);
    memoryUpdate(EEPROM_INDIVIDUALADDRESS_LO, msg[3]);

    // see: http://stackoverflow.com/questions/3920307/how-can-i-remove-a-flag-in-c
    _deviceFlags &= ~0x80; // remove factory setting bit (left most bit))
#ifdef DEBUG_PROTOCOL
    DEBUG_PRINTLN(F("DeviceFlags after =0x%02x"), _deviceFlags);
#endif

    memoryUpdate(EEPROM_DEVICE_FLAGS, _deviceFlags);
#endif    
    _individualAddress = (msg[2] << 8) + (msg[3] << 0);
    sendAck(0x00, 0x00);
}

void KonnektingDevice::handleMsgReadIndividualAddress(byte /*msg*/[]) {
    // to suppress compiler warning about unused variable, "msg" has been commented out
    DEBUG_PRINTLN(F("handleMsgReadIndividualAddress"));
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
    Knx.write(PROGCOMOBJ_INDEX, response);
}

void KonnektingDevice::handleMsgWriteParameter(byte msg[]) {
    DEBUG_PRINTLN(F("handleMsgWriteParameter"));

    byte index = msg[2];

    if (index > _numberOfParams - 1) {
        sendAck(KNX_DEVICE_INVALID_INDEX, index);
        return;
    }

    int skipBytes = calcParamSkipBytes(index);
    int paramLen = getParamSize(index);

#ifdef DEBUG_PROTOCOL
    DEBUG_PRINTLN(F("id=%d"), index);
#endif

#if defined(WRITEMEM)    
    // write byte by byte
    for (byte i = 0; i < paramLen; i++) {
#ifdef DEBUG_PROTOCOL
        DEBUG_PRINTLN(F(" data[%d]=0x%02x"), i, msg[3 + i]);
#endif
        memoryUpdate(_paramTableStartindex + skipBytes + i, msg[3 + i]);
    }
#endif
    sendAck(0x00, 0x00);
}

void KonnektingDevice::handleMsgReadParameter(byte msg[]) {
    DEBUG_PRINTLN(F("handleMsgReadParameter"));
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

    Knx.write(PROGCOMOBJ_INDEX, response);

}

void KonnektingDevice::handleMsgWriteComObject(byte msg[]) {
    DEBUG_PRINTLN(F("handleMsgWriteComObject"));

    byte comObjId = msg[2];
    byte gaHi = msg[3];
    byte gaLo = msg[4];
    byte settings = msg[5];
    word ga = (gaHi << 8) + (gaLo << 0);

#ifdef DEBUG_PROTOCOL
    DEBUG_PRINTLN(F("CO id=%d hi=0x%02x lo=0x%02x GA=0x%04x settings=0x%02x"), comObjId, gaHi, gaLo, ga, settings);

    bool foundWrongUndefined = false;
    for (int i = 6; i < 13; i++) {
        if (msg[i] != 0x00) foundWrongUndefined = true;
    }
    if (foundWrongUndefined) {
        DEBUG_PRINTLN(F("!!!!!!!!!!! WARNING: Suite is sending wrong data. Ensure Suite version matches the Device Lib !!!!!"));
    }
#endif        

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

void KonnektingDevice::handleMsgReadComObject(byte msg[]) {
#ifdef DEBUG_PROTOCOL
    DEBUG_PRINTLN(F("handleMsgReadComObject"));
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

    Knx.write(PROGCOMOBJ_INDEX, response);
}

int KonnektingDevice::memoryRead(int index) {
    DEBUG_PRINTLN(F("memRead: index=0x%02x"), index);
    byte d = 0xFF;

    if (*eepromReadFunc != NULL) {
        DEBUG_PRINTLN(F("memRead: using fctptr"));
        d = eepromReadFunc(index);
    } else {
#ifdef __SAMD21G18A__
        DEBUG_PRINTLN(F("memRead: EEPROM NOT SUPPORTED. USE FCTPTR!"));
#else
        d = EEPROM.read(index);
#endif
    }
    DEBUG_PRINTLN(F("memRead: data=0x%02x"), d);
    return d;
}

void KonnektingDevice::memoryWrite(int index, byte data) {

    DEBUG_PRINTLN(F("memWrite: index=0x%02x data=0x%02x"), index, data);
    if (*eepromWriteFunc != NULL) {
        DEBUG_PRINTLN(F("memWrite: using fctptr"));
        eepromWriteFunc(index, data);
    } else {
#ifdef __SAMD21G18A__
        DEBUG_PRINTLN(F("memoryWrite: EEPROM NOT SUPPORTED. USE FCTPTR!"));
#elif ESP8266    
        DEBUG_PRINTLN(F("ESP8266: EEPROM.write"));
        EEPROM.write(index, data);
#else
        EEPROM.write(index, data);
        //    delay(10); // really required?
#endif 
    }

    // EEPROM has been changed, reboot will be required
    _rebootRequired = true;
}

void KonnektingDevice::memoryUpdate(int index, byte data) {

    DEBUG_PRINTLN(F("memUpdate: index=0x%02x data=0x%02x"), index, data);

    if (*eepromUpdateFunc != NULL) {
        DEBUG_PRINTLN(F("memUpdate: using fctptr"));
        eepromUpdateFunc(index, data);
    } else {
#ifdef __SAMD21G18A__   
        DEBUG_PRINTLN(F("memoryUpdate: EEPROM NOT SUPPORTED. USE FCTPTR!"));
#elif ESP8266    
        DEBUG_PRINTLN(F("ESP8266: EEPROM.update"));
        byte d = EEPROM.read(index);
        if (d != data) {
            EEPROM.write(index, data);
        }
#else
        EEPROM.update(index, data);
        //    delay(10); // really required?
#endif
    }
    // EEPROM has been changed, reboot will be required
    _rebootRequired = true;

}

void KonnektingDevice::memoryCommit() {
    if (*eepromCommitFunc != NULL) {
        DEBUG_PRINTLN(F("memCommit: using fctptr"));
        eepromCommitFunc();
    }
}

uint8_t KonnektingDevice::getUINT8Param(byte index) {
    if (getParamSize(index) != PARAM_UINT8) {
        DEBUG_PRINTLN(F("Requested UINT8 param for index %d but param has different length! Will Return 0."), index);
        return 0;
    }

    byte paramValue[1];
    getParamValue(index, paramValue);

    return paramValue[0];
}

int8_t KonnektingDevice::getINT8Param(byte index) {
    if (getParamSize(index) != PARAM_INT8) {
        DEBUG_PRINTLN(F("Requested INT8 param for index %d but param has different length! Will Return 0."), index);
        return 0;
    }

    byte paramValue[1];
    getParamValue(index, paramValue);

    return paramValue[0];
}

uint16_t KonnektingDevice::getUINT16Param(byte index) {
    if (getParamSize(index) != PARAM_UINT16) {
        DEBUG_PRINTLN(F("Requested UINT16 param for index %d but param has different length! Will Return 0."), index);
        return 0;
    }

    byte paramValue[2];
    getParamValue(index, paramValue);

    uint16_t val = (paramValue[0] << 8) + (paramValue[1] << 0);

    return val;
}

int16_t KonnektingDevice::getINT16Param(byte index) {
    if (getParamSize(index) != PARAM_INT16) {
        DEBUG_PRINTLN(F("Requested INT16 param for index %d but param has different length! Will Return 0."), index);
        return 0;
    }

    byte paramValue[2];
    getParamValue(index, paramValue);

    //    //DEBUG_PRINT((F(" int16: [1]=0x"));
    //    //DEBUG_PRINT2(paramValue[0], HEX);
    //    //DEBUG_PRINT((F(" [0]=0x"));
    //    DEBUG_PRINTLN2(paramValue[1], HEX);

    int16_t val = (paramValue[0] << 8) + (paramValue[1] << 0);

    return val;
}

uint32_t KonnektingDevice::getUINT32Param(byte index) {
    if (getParamSize(index) != PARAM_UINT32) {
        DEBUG_PRINTLN(F("Requested UINT32 param for index %d but param has different length! Will Return 0."), index);
        return 0;
    }

    byte paramValue[4];
    getParamValue(index, paramValue);

    uint32_t val = ((uint32_t) paramValue[0] << 24) + ((uint32_t) paramValue[1] << 16) + ((uint32_t) paramValue[2] << 8) + ((uint32_t) paramValue[3] << 0);

    return val;
}

int32_t KonnektingDevice::getINT32Param(byte index) {
    if (getParamSize(index) != PARAM_INT32) {
        DEBUG_PRINTLN(F("Requested INT32 param for index %d but param has different length! Will Return 0."), index);
        return 0;
    }

    byte paramValue[4];
    getParamValue(index, paramValue);

    int32_t val = ((uint32_t) paramValue[0] << 24) + ((uint32_t) paramValue[1] << 16) + ((uint32_t) paramValue[2] << 8) + ((uint32_t) paramValue[3] << 0);

    return val;
}

String KonnektingDevice::getSTRING11Param(byte index) {
    String ret;
    if (getParamSize(index) != PARAM_STRING11) {
        DEBUG_PRINTLN(F("Requested STRING11 param for index %d but param has different length! Will Return \"\""), index);
        ret = "";
        return ret;
    }

    byte paramValue[PARAM_STRING11];
    getParamValue(index, paramValue);

    // check if string is 0x00 terminated (means <11 chars)
    for (int i = 0; i < PARAM_STRING11; i++) {
        if (paramValue[i] == 0x00) {
            break; // stop at null-termination
        } else {
            ret += (char) paramValue[i]; // copy char by char into string      
        }
    }

    return ret;
}

int KonnektingDevice::getFreeEepromOffset() {

    int offset = _paramTableStartindex;
    for (int i = 0; i < _numberOfParams; i++) {
        offset += _paramSizeList[i];
    }
    return offset;
}

void KonnektingDevice::setMemoryReadFunc(int (*func)(int)) {
    eepromReadFunc = func;
}

void KonnektingDevice::setMemoryWriteFunc(void (*func)(int, int)) {
    eepromWriteFunc = func;
}

void KonnektingDevice::setMemoryUpdateFunc(void (*func)(int, int)) {
    eepromUpdateFunc = func;
}

void KonnektingDevice::setMemoryCommitFunc(void (*func)(void)) {
    eepromCommitFunc = func;
}

