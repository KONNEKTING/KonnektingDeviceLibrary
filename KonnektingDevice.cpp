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
#define DEBUG
#include "DebugUtil.h"
#include "KnxDevice.h"
#include "KnxComObject.h"
#include "KnxDPT.h"
#include "KonnektingDevice.h"

#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif


// KonnektingDevice unique instance creation
KonnektingDevice KonnektingDevice::Konnekting;
KonnektingDevice& Konnekting = KonnektingDevice::Konnekting;

/**
 * Intercepting knx events to process internal com objects
 * @param index
 */
void konnektingKnxEvents(byte index) {

    DEBUG_PRINT(F("\n\nkonnektingKnxEvents index="));
    DEBUG_PRINTLN(index);

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
    DEBUG_PRINT("Setup ESP8266 ... ");

    // disable WIFI per default
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    delay(100);

    // enable 1k EEPROM on ESP8266 platform
    EEPROM.begin(1024);

    DEBUG_PRINTLN("*DONE*");
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

//    _paramSizeList = paramSizeList;
//    _numberOfParams = sizeof (_paramSizeList); 
    
    DEBUG_PRINTLN(F("Initialize KonnektingDevice"));
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

    attachInterrupt(digitalPinToInterrupt(_progButton), KonnektingProgButtonPressed, RISING);

    // hardcoded stuff
    DEBUG_PRINT(F("Manufacturer: "));
    DEBUG_PRINT2(_manufacturerID, HEX);
    DEBUG_PRINTLN(F("hex"));

    DEBUG_PRINT("Device: ");
    DEBUG_PRINT2(_deviceID, HEX);
    DEBUG_PRINTLN(F("hex"));

    DEBUG_PRINT(F("Revision: "));
    DEBUG_PRINT2(_revisionID, HEX);
    DEBUG_PRINTLN(F("hex"));

    DEBUG_PRINT(F("numberOfCommObjects: "));
    DEBUG_PRINTLN(Knx.getNumberOfComObjects());

    // calc index of parameter table in eeprom --> depends on number of com objects
    _paramTableStartindex = EEPROM_COMOBJECTTABLE_START + (Knx.getNumberOfComObjects() * 3);

    _deviceFlags = EEPROM.read(EEPROM_DEVICE_FLAGS);

    DEBUG_PRINT(F("_deviceFlags: "));
    DEBUG_PRINT2(_deviceFlags, BIN);
    DEBUG_PRINTLN(F("bin"));

    _individualAddress = P_ADDR(1, 1, 254);
    if (!isFactorySetting()) {
        DEBUG_PRINTLN(F("->EEPROM"));
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

            DEBUG_PRINT(F("ComObj index="));
            DEBUG_PRINT((i + 1));
            DEBUG_PRINT(F(" Suite-ID="));
            DEBUG_PRINT(i);
            DEBUG_PRINT(F(" HI: 0x"));
            DEBUG_PRINT2(hi, HEX);
            DEBUG_PRINT(F(" LO: 0x"));
            DEBUG_PRINT2(lo, HEX);
            DEBUG_PRINT(F(" GA: 0x"));
            DEBUG_PRINT2(comObjAddr, HEX);
            DEBUG_PRINT(F(" Settings: 0x"));
            DEBUG_PRINT2(settings, HEX);
            DEBUG_PRINT(F(" Active: "));
            DEBUG_PRINTLN2(active, DEC);
        }

    } else {
        DEBUG_PRINTLN(F("->FACTORY"));
    }
    DEBUG_PRINT(F("IA: 0x"));
    DEBUG_PRINTLN2(_individualAddress, HEX);
    e_KnxDeviceStatus status;
    status = Knx.begin(serial, _individualAddress);
    DEBUG_PRINT(F("KnxDevice startup status: 0x"));
    DEBUG_PRINTLN2(status, HEX);

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
    return _deviceFlags == 0xff;
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

    DEBUG_PRINT(F("getParamValue: index="));
    DEBUG_PRINT(index);
    DEBUG_PRINT(F(" _paramTableStartindex="));
    DEBUG_PRINT(_paramTableStartindex);
    DEBUG_PRINT(F(" skipBytes="));
    DEBUG_PRINT(skipBytes);
    DEBUG_PRINT(F(" paramLen="));
    DEBUG_PRINTLN(paramLen);

    // read byte by byte
    for (byte i = 0; i < paramLen; i++) {

        int addr = _paramTableStartindex + skipBytes + i;

        value[i] = EEPROM.read(addr);
        DEBUG_PRINT(F(" val["));
        DEBUG_PRINT(i);
        DEBUG_PRINT(F("]@"));
        DEBUG_PRINT(addr);
        DEBUG_PRINT(F(" -> 0x"));
        DEBUG_PRINT2(value[i], HEX);
        DEBUG_PRINTLN(F(""));
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
    return _progState ? true : false;
}

/**
 * Check whether Konnekting is ready for application logic. 
 * (Means: no busy with programming-mode and not running with factory settings)
 * @return true if it's safe to run application logic
 */
bool KonnektingDevice::isReadyForApplication() {
    return !isProgState() && !isFactorySetting();
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
    KnxComObject p = KnxComObject(KNX_DPT_60000_000 /* KNX PROGRAM */, KNX_COM_OBJ_C_W_U_T_INDICATOR); /* NEEDS TO BE THERE FOR PROGRAMMING PURPOSE */
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
    DEBUG_PRINTLN("ESP8266 restart");
    ESP.restart();
#endif

#ifdef __AVR_ATmega328P__
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

    DEBUG_PRINT(F("internalComObject index="));
    DEBUG_PRINTLN(index);
    bool consumed = false;
    switch (index) {
        case 0: // object index 0 has been updated

            byte buffer[14];
            Knx.read(0, buffer);
#ifdef DEBUG_PROTOCOL
            for (int i = 0; i < 14; i++) {
                DEBUG_PRINT(F("buffer["));
                DEBUG_PRINT(i);
                DEBUG_PRINT(F("]\thex=0x"));
                DEBUG_PRINT2(buffer[i], HEX);
                DEBUG_PRINT(F("  \tbin="));
                DEBUG_PRINTLN2(buffer[i], BIN);
            }
#endif

            byte protocolversion = buffer[0];
            byte msgType = buffer[1];

            DEBUG_PRINT(F("protocolversion=0x"));
            DEBUG_PRINTLN2(protocolversion, HEX);

            DEBUG_PRINT(F("msgType=0x"));
            DEBUG_PRINTLN2(msgType, HEX);

            if (protocolversion != PROTOCOLVERSION) {
                DEBUG_PRINT(F("Unsupported protocol version. Using "));
                DEBUG_PRINT(PROTOCOLVERSION);
                DEBUG_PRINT(F(" Got: "));
                DEBUG_PRINT(protocolversion);
                DEBUG_PRINTLN(F("!"));
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
                        DEBUG_PRINT(F("Unsupported msgtype: 0x"));
                        DEBUG_PRINT2(msgType, HEX);
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
    DEBUG_PRINT(F("sendAck errorcode=0x"));
    DEBUG_PRINT2(errorcode, HEX);
    DEBUG_PRINT(F(" indexinformation=0x"));
    DEBUG_PRINTLN2(indexinformation, HEX);
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
        Knx.write(0, response);
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
        if (msg[4] == 0x00) {
            DEBUG_PRINTLN("ESP8266: EEPROM.commit()");
            EEPROM.commit();
        }
#endif                

    } else {
        DEBUG_PRINTLN(F("no matching IA"));
    }
}

void KonnektingDevice::handleMsgReadProgrammingMode(byte msg[]) {
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
        Knx.write(0, response);
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
    DEBUG_PRINT(F("DeviceFlags after =0x"));
    DEBUG_PRINTLN2(_deviceFlags, HEX);
#endif

    memoryUpdate(EEPROM_DEVICE_FLAGS, _deviceFlags);
#endif    
    _individualAddress = (msg[2] << 8) + (msg[3] << 0);
    sendAck(0x00, 0x00);
}

void KonnektingDevice::handleMsgReadIndividualAddress(byte msg[]) {
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
    Knx.write(0, response);
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
    DEBUG_PRINT(F("id="));
    DEBUG_PRINT(index);
    DEBUG_PRINTLN(F(""))
#endif

#if defined(WRITEMEM)    
            // write byte by byte
    for (byte i = 0; i < paramLen; i++) {
#ifdef DEBUG_PROTOCOL
        DEBUG_PRINT(F(" data["));
        DEBUG_PRINT(i);
        DEBUG_PRINT(F("]=0x"));
        DEBUG_PRINTLN2(msg[3 + i], HEX);
#endif
        //EEPROM.update(_paramTableStartindex + skipBytes + i, msg[3 + i]);
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

    Knx.write(0, response);

}

void KonnektingDevice::handleMsgWriteComObject(byte msg[]) {
    DEBUG_PRINTLN(F("handleMsgWriteComObject"));

    byte comObjId = msg[2];
    byte gaHi = msg[3];
    byte gaLo = msg[4];
    byte settings = msg[5];
    word ga = (gaHi << 8) + (gaLo << 0);

#ifdef DEBUG_PROTOCOL
    DEBUG_PRINT(F("CO id="));
    DEBUG_PRINT(comObjId);
    DEBUG_PRINT(F(" hi=0x"));
    DEBUG_PRINT2(gaHi, HEX);
    DEBUG_PRINT(F(" lo=0x"));
    DEBUG_PRINT2(gaLo, HEX);
    DEBUG_PRINT(F(" ga=0x"));
    DEBUG_PRINT2(ga, HEX);
    DEBUG_PRINT(F(" settings=0x"));
    DEBUG_PRINTLN2(settings, HEX);

    bool foundWrongUndefined = false;
    for (int i = 6; i < 13; i++) {
        if (msg[i] != 0x00) foundWrongUndefined = true;
    }
    if (foundWrongUndefined) {
        DEBUG_PRINTLN("!!!!!!!!!!! WARNING: Suite is sending wrong data. Ensure Suite version matches the Device Lib !!!!!");
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

    Knx.write(0, response);
}

void KonnektingDevice::memoryUpdate(int index, byte data) {


    DEBUG_PRINT(F("memUpdate: index="));
    DEBUG_PRINT(index);
    DEBUG_PRINT(F(" data=0x"));
    DEBUG_PRINTLN2(data, HEX);

#ifdef ESP8266    
    DEBUG_PRINTLN(F("ESP8266: EEPROM.update"));
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

uint8_t KonnektingDevice::getUINT8Param(byte index) {
    if (getParamSize(index) != PARAM_UINT8) {
        DEBUG_PRINT(F("Requested UINT8 param for index "));
        DEBUG_PRINT(index);
        DEBUG_PRINTLN(F(" but param has different length! Will Return 0."));
        return 0;
    }

    byte paramValue[1];
    getParamValue(index, paramValue);

    return paramValue[0];
}

int8_t KonnektingDevice::getINT8Param(byte index) {
    if (getParamSize(index) != PARAM_INT8) {
        DEBUG_PRINT(F("Requested INT8 param for index "));
        DEBUG_PRINT(index);
        DEBUG_PRINTLN(F(" but param has different length! Will Return 0."));
        return 0;
    }

    byte paramValue[1];
    getParamValue(index, paramValue);

    return paramValue[0];
}

uint16_t KonnektingDevice::getUINT16Param(byte index) {
    if (getParamSize(index) != PARAM_UINT16) {
        DEBUG_PRINT(F("Requested UINT16 param for index "));
        DEBUG_PRINT(index);
        DEBUG_PRINTLN(F(" but param has different length! Will Return 0."));
        return 0;
    }

    byte paramValue[2];
    getParamValue(index, paramValue);

    uint16_t val = (paramValue[0] << 8) + (paramValue[1] << 0);

    return val;
}

int16_t KonnektingDevice::getINT16Param(byte index) {
    if (getParamSize(index) != PARAM_INT16) {
        DEBUG_PRINT(F("Requested INT16 param for index "));
        DEBUG_PRINT(index);
        DEBUG_PRINTLN(F(" but param has different length! Will Return 0."));
        return 0;
    }

    byte paramValue[2];
    getParamValue(index, paramValue);

    //    DEBUG_PRINT(F(" int16: [1]=0x"));
    //    DEBUG_PRINT2(paramValue[0], HEX);
    //    DEBUG_PRINT(F(" [0]=0x"));
    //    DEBUG_PRINTLN2(paramValue[1], HEX);

    int16_t val = (paramValue[0] << 8) + (paramValue[1] << 0);

    return val;
}

uint32_t KonnektingDevice::getUINT32Param(byte index) {
    if (getParamSize(index) != PARAM_UINT32) {
        DEBUG_PRINT(F("Requested UINT32 param for index "));
        DEBUG_PRINT(index);
        DEBUG_PRINTLN(F(" but param has different length! Will Return 0."));
        return 0;
    }

    byte paramValue[4];
    getParamValue(index, paramValue);

    uint32_t val = ((uint32_t) paramValue[0] << 24) + ((uint32_t) paramValue[1] << 16) + ((uint32_t) paramValue[2] << 8) + ((uint32_t) paramValue[3] << 0);

    return val;
}

int32_t KonnektingDevice::getINT32Param(byte index) {
    if (getParamSize(index) != PARAM_INT32) {
        DEBUG_PRINT(F("Requested UINT32 param for index "));
        DEBUG_PRINT(index);
        DEBUG_PRINTLN(F(" but param has different length! Will Return 0."));
        return 0;
    }

    byte paramValue[4];
    getParamValue(index, paramValue);

    int32_t val = ((uint32_t) paramValue[0] << 24) + ((uint32_t) paramValue[1] << 16) + ((uint32_t) paramValue[2] << 8) + ((uint32_t) paramValue[3] << 0);

    return val;
}

int KonnektingDevice::getFreeEepromOffset() {

    int offset = _paramTableStartindex;
    for (int i = 0; i < _numberOfParams; i++) {
        offset += _paramSizeList[i];
    }
    return offset;
}