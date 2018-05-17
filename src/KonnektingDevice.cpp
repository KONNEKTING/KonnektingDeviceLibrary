/*!
 * @file KonnektingDevice.cpp
 *
 * @mainpage KONNEKTING Device Library
 *
 * @section intro_sec Introduction
 *
 * This is the documentation for KONNEKTINGs Arduno Device Library.
 *
 * See https://wiki.konnekting.de for more information
 *
 * @section dependencies Dependencies
 *
 * This library depends on <a href="https://foo/bar">
 * Foo Bar</a> being present on your system. Please make sure you have
 * installed the latest version before using this library.
 *
 * @section author Author
 *
 * Written by Alexander Christian.
 *
 * @section license License
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
 *
 */


/*
 * !!!!! IMPORTANT !!!!!
 * if "#define DEBUG" is set, you must run your KONNEKTING Suite with "-Dde.root1.slicknx.konnekting.debug=true" 
 * A release-version of your development MUST NOT contain "#define DEBUG" ...
 */

#define DEBUG_PROTOCOL
#define WRITEMEM
// reboot feature via progbutton
#define REBOOT_BUTTON

#include "Arduino.h"
#include "DebugUtil.h"
#include "KnxDevice.h"
#include "KnxComObject.h"
#include "KnxDataPointTypes.h"
#include "KonnektingDevice.h"

#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif

#define PROGCOMOBJ_INDEX 255



// KonnektingDevice unique instance creation
KonnektingDevice KonnektingDevice::Konnekting;
KonnektingDevice& Konnekting = KonnektingDevice::Konnekting;

/**
 * Intercepting knx events to process internal com objects
 * @param index
 */
/**************************************************************************/
/*!
    @brief  Intercepting knx events to process internal com objects
    @param    index
                Comobject index
    @return void
 */

/**************************************************************************/
void konnektingKnxEvents(byte index) {

    DEBUG_PRINTLN(F("\n\nkonnektingKnxEvents index=%d"), index);

    // if it's not a internal com object, route back to knxEvents()
    if (!Konnekting.internalKnxEvents(index)) {
        knxEvents(index);
    }
}

/***************************************************************************
 CONSTRUCTOR
 ***************************************************************************/

/**************************************************************************/
/*!
    @brief  Instantiates a new KONNEKTING Device class
 */

/**************************************************************************/
KonnektingDevice::KonnektingDevice() {
    DEBUG_PRINTLN(F("\n\n\n\nSetup KonnektingDevice"));
}

/**************************************************************************/
/*!
 *  @brief  Internal init function, called by outer init
 *  @param  serial 
 *          serial port reference, f.i. "Serial" or "Serial1"
 *  @param  manufacturerID
 *          The ID of manufacturer
 *  @param  deviceID
 *          The ID of the device
 *  @param  revisionID
 *          The device's revision
 *  @return void
 */
/**************************************************************************/
void KonnektingDevice::internalInit(HardwareSerial& serial,
        word manufacturerID,
        byte deviceID,
        byte revisionID){

    DEBUG_PRINTLN(F("Initialize KonnektingDevice"));

    DEBUG_PRINTLN("15/7/255 = 0x%04x", G_ADDR(15, 7, 255));

    _initialized = true;

    _manufacturerID = manufacturerID;
    _deviceID = deviceID;
    _revisionID = revisionID;

    _lastProgbtn = 0;
    _progbtnCount = 0;

    setProgState(false);


    // hardcoded stuff
    DEBUG_PRINTLN(F("Manufacturer: 0x%02x Device: 0x%02x Revision: 0x%02x"), _manufacturerID, _deviceID, _revisionID);

    DEBUG_PRINTLN(F("numberOfCommObjects: %d"), Knx.getNumberOfComObjects());

    // calc  of parameter table in eeprom --> depends on number of com objects
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

    
#if defined(ESP8266) || defined(ESP32)
    // ESP has no EEPROM, but flash and needs to init the EEPROM emulator with an initial size. We create 8k EEPROM
    EEPROM.begin(8192);
#endif
}

/**************************************************************************/
/*!
 *  @brief  Starts KNX KonnektingDevice, as well as KNX Device
 *  @param  serial 
 *          serial port reference, f.i. "Serial" or "Serial1"
 *  @param  progIndicatorFunc
 *          function pointer to the function to toggle programming mode
 *  @param  manufacturerID
 *          The ID of manufacturer
 *  @param  deviceID
 *          The ID of the device
 *  @param  revisionID
 *          The device's revision
 *  @return void
 */
/**************************************************************************/

void KonnektingDevice::init(HardwareSerial& serial,
        void (*progIndicatorFunc)(bool),
        word manufacturerID,
        byte deviceID,
        byte revisionID
        ) {
    _progIndicatorFunc = progIndicatorFunc;

    internalInit(serial,manufacturerID,deviceID,revisionID);
}

/**************************************************************************/
/*!
 *  @brief  Starts KNX KonnektingDevice, as well as KNX Device
 *  @param  serial 
 *          serial port reference, f.i. "Serial" or "Serial1"
 *  @param  progButtonPin 
 *          pin which toggles programming mode, needs an interrupt capable pin!
 *  @param  progLedPin 
 *          pin which drives LED when in programming mode
 *  @param  manufacturerID
 *          The ID of manufacturer
 *  @param  deviceID
 *          The ID of the device
 *  @param  revisionID
 *          The device's revision
 *  @return void
 */
/**************************************************************************/
void KonnektingDevice::init(HardwareSerial& serial,
        int progButtonPin,
        int progLedPin,
        word manufacturerID,
        byte deviceID,
        byte revisionID
        ) {

    _progLED = progLedPin;
    _progButton = progButtonPin;

    pinMode(_progLED, OUTPUT);
    pinMode(_progButton, INPUT);
    attachInterrupt(digitalPinToInterrupt(_progButton), KonnektingProgButtonPressed, RISING);

    internalInit(serial,manufacturerID,deviceID,revisionID);
}

/**************************************************************************/
/*!
 *  @brief  checks if the device is already initialized and active.
 *  @return True if the device has been initialized (by calling one of the init() functions).
 */
/**************************************************************************/
bool KonnektingDevice::isActive() {
    return _initialized;
}

/**************************************************************************/
/*!
 *  @brief  checks if the device is in factory settings mode, means: has not been programmed so far.
 *  @return True if factory settings are active
 */
/**************************************************************************/
bool KonnektingDevice::isFactorySetting() {
    bool isFactory = (_deviceFlags == 0xff);
    //    DEBUG_PRINTLN(F("isFactorySetting: %d"), isFactory);
    return isFactory;
}

/**************************************************************************/
/*!
 *  @brief  Bytes to skip when reading/writing in param-table
 *  @param  index
 *          parameter-id to calc skipbytes for
 *  @return bytes to skip
 */

/**************************************************************************/
int KonnektingDevice::calcParamSkipBytes(int index) {
    // calc bytes to skip
    int skipBytes = 0;
    if (index > 0) {
        for (int i = 0; i < index; i++) {
            skipBytes += getParamSize(i);
        }
    }
    return skipBytes;
}

/**************************************************************************/
/*!
 *  @brief  Gets the size in byte of a param identified by its index
 *  @param  index
 *          parameter-id to get the size of
 *  @return size in bytes
 */

/**************************************************************************/
byte KonnektingDevice::getParamSize(int index) {
    return _paramSizeList[index];
}

/**************************************************************************/
/*!
 *  @brief  Gets the size in byte of a param identified by its index
 *  @param  index
 *          parameter-id of the parameter to get the value for
 *  @param[out] value
 *          the value of the parameter
 *  @return void
 */

/**************************************************************************/
void KonnektingDevice::getParamValue(int index, byte value[]) {

    if (index > _numberOfParams - 1) {
        return;
    }

    int skipBytes = calcParamSkipBytes(index);
    int paramLen = getParamSize(index);

    DEBUG_PRINTLN(F("getParamValue: index=%d _paramTableStartindex=%d skipbytes=%d paremLen=%d"), index, _paramTableStartindex, skipBytes, paramLen);

    // read byte by byte
    for (int i = 0; i < paramLen; i++) {

        int addr = _paramTableStartindex + skipBytes + i;

        value[i] = memoryRead(addr);
        DEBUG_PRINTLN(F(" val[%d]@%d -> 0x%02x"), i, addr, value[i]);
    }
}

/**************************************************************************/
/*!
 *  @brief  Interrupt Service Routines for the prog-button
 *          Used when actually using the built-int prog-button-feature that requires an interrupt enabled pin.
 *          Although this function is "public", it's NOT part of the API and should not be called by users.
 *  @return void
 */

/**************************************************************************/
void KonnektingProgButtonPressed() {
    DEBUG_PRINTLN(F("PrgBtn toggle"));
    Konnekting.toggleProgState();
}

/**************************************************************************/
/*!
 *  @brief  Toggle the "programming mode" state. 
 *          This is typically called by prog-button-implementation.
 *  @return void
 */
/**************************************************************************/
void KonnektingDevice::toggleProgState() {

#ifdef REBOOT_BUTTON
    if (millis() - _lastProgbtn < 300) {
        _progbtnCount++;

        if (_progbtnCount == 3) {
            DEBUG_PRINTLN(F("Forced-Reboot-Request detected"));
            reboot();
        }
    } else {
        _progbtnCount = 1;
    }
    _lastProgbtn = millis();
#endif

    setProgState(!_progState); // toggle and set
    if (_rebootRequired) {
        DEBUG_PRINTLN(F("found rebootRequired flag, triggering reboot"));
        reboot();
    }
}

/**************************************************************************/
/*!
 *  @brief  Gets programming mode state
 *  @return true, if programming is active, false if not
 */

/**************************************************************************/
bool KonnektingDevice::isProgState() {
    return _progState;
}

/**************************************************************************/
/*!
 *  @brief  Check whether Konnekting is ready for application logic. 
 *          Means: not busy with programming-mode and not running with factory settings
 *  @return true if it's safe to run application logic
 */

/**************************************************************************/
bool KonnektingDevice::isReadyForApplication() {
    bool isReady = (!isProgState() && !isFactorySetting());
    return isReady;
}

/**************************************************************************/
/*!
 *  @brief  Sets the prog state to given boolean value
 *  @param  state 
 *          new prog state
 *  @return void
 */

/**************************************************************************/
void KonnektingDevice::setProgState(bool state) {
    _progState = state;
    setProgLed(state);
    DEBUG_PRINTLN(F("PrgState %d"),state);
    if (*_progIndicatorFunc == NULL) {
        digitalWrite(_progLED, state);
    }
}

/**************************************************************************/
/*!
 *  @brief  Sets the prog LED to given boolean value
 *  @param  state 
 *          new prog state
 *  @return void
 */

/**************************************************************************/
void KonnektingDevice::setProgLed(bool state) {
    if (*_progIndicatorFunc != NULL) {
        _progIndicatorFunc(state);
    }else{
        digitalWrite(_progLED, state);
    }
    DEBUG_PRINTLN(F("PrgLed %d"),state);
}

/**************************************************************************/
/*!
 *  @brief  Check if given 2byte ID is matching the current set IA
 *  @param  hi
 *          high byte of IA
 *  @param  lo
 *          low byte of IA
 *  @return true if match, false if not
 */

/**************************************************************************/
bool KonnektingDevice::isMatchingIA(byte hi, byte lo) {
    byte iaHi = (_individualAddress >> 8) & 0xff;
    byte iaLo = (_individualAddress >> 0) & 0xff;

    return (hi == iaHi && lo == iaLo);
}

/**************************************************************************/
/*!
 *  @brief  Creates internal programming ComObj
 *  @return KnxComObject
 */
/**************************************************************************/
KnxComObject KonnektingDevice::createProgComObject() {
    DEBUG_PRINTLN(F("createProgComObject"));
    KnxComObject p = KnxComObject(KNX_DPT_60000_60000 /* KNX PROGRAM */, KNX_COM_OBJ_C_W_U_T_INDICATOR); /* NEEDS TO BE THERE FOR PROGRAMMING PURPOSE */
    p.setAddr(G_ADDR(15, 7, 255));
    p.setActive(true);
    return /* Index 0 */ p;
}

/**************************************************************************/
/*!
 *  @brief  Reboot the device
 *          This is typically done after finishing the programming process
 *          to get the device back in a well-known state with new settings
 *  @return void
 */
/**************************************************************************/
void KonnektingDevice::reboot() {
    Knx.end();

#if defined(ESP8266) || defined(ESP32) 
    DEBUG_PRINTLN(F("ESP restart"));
    ESP.restart();
#elif ARDUINO_ARCH_SAMD
    // do reset of arduino zero, inspired by http://forum.arduino.cc/index.php?topic=366836.0
    DEBUG_PRINTLN(F("SAMD SystemReset"));
    WDT->CTRL.reg = 0; // disable watchdog
    while (WDT->STATUS.bit.SYNCBUSY == 1); //Just wait till WDT is free
    WDT->CONFIG.reg = 0; // see Table 17-5 Timeout Period (valid values 0-11)
    WDT->CTRL.reg = WDT_CTRL_ENABLE; //enable watchdog
    while (WDT->STATUS.bit.SYNCBUSY == 1); //Just wait till WDT is free
    while (1) {
    }
#elif ARDUINO_ARCH_STM32
    DEBUG_PRINTLN(F("STM32 SystemReset"));
    delay(100);
    NVIC_SystemReset();
#elif __AVR_ATmega32U4__
    DEBUG_PRINTLN(F("WDT reset NOW"));
    wdt_enable(WDTO_500MS);
    while (1) {
    }
#else     
    // to overcome WDT infinite reboot-loop issue
    // see: https://github.com/arduino/Arduino/issues/4492
    DEBUG_PRINTLN(F("software reset NOW"));
    delay(500);
    asm volatile ("  jmp 0");
#endif    

}

/**************************************************************************/
/*!
 *  @brief  processes internal comobj for programming purpose.
 *          This method is feed with _all_ comobj events. If it's internal comobj, it's handled and return value is true.
 *          If it's a user defined comobj, all processing is skipped and false is returned.
 *  @param  index
 *          index of incoming comobj
 *  @return true, if index was internal comobject and has been handled, false if not
 */
/**************************************************************************/
bool KonnektingDevice::internalKnxEvents(byte index) {

    DEBUG_PRINTLN(F("internalKnxEvents index=%d"), index);
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

/**************************************************************************/
/*!
 *  @brief  Sending ACK message back to the bus
 *  @param  errorcode
 *          error code, if any
 *  @param  indexinformation
 *          indexinformation, if error is related to an index 
 *  @return void
 */
/**************************************************************************/
void KonnektingDevice::sendAck(byte errorcode, int indexinformation) {
    DEBUG_PRINTLN(F("sendAck errorcode=0x%02x indexInformation=0x%04x"), errorcode, indexinformation);
    byte response[14];
    response[0] = PROTOCOLVERSION;
    response[1] = MSGTYPE_ACK;
    response[2] = (errorcode == 0x00 ? 0x00 : 0xFF);
    response[3] = errorcode;
    response[4] = (indexinformation >> 8) & 0xff;
    response[5] = (indexinformation >> 0) & 0xff;
    for (byte i = 6; i < 14; i++) {
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

#if defined(ESP8266) || defined(ESP32)
        // ESP8266/ESP32 uses own EEPROM implementation which requires commit() call
        if (msg[4] == 0x00) {
            DEBUG_PRINTLN(F("ESP8266/ESP32: EEPROM.commit()"));
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

    int index = msg[2];

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
    int index = msg[0];

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
    DEBUG_PRINT(F("memRead: index=0x%02x"), index);
    byte d = 0xFF;

    if (*_eepromReadFunc != NULL) {
        DEBUG_PRINT(F(" using fctptr"));
        d = _eepromReadFunc(index);
    } else {
#ifdef ARDUINO_ARCH_SAMD
        DEBUG_PRINTLN(F("memRead: EEPROM NOT SUPPORTED. USE FCTPTR!"));
#else
        d = EEPROM.read(index);
#endif
    }
    DEBUG_PRINTLN(F(" data=0x%02x"), d);
    return d;
}

void KonnektingDevice::memoryWrite(int index, byte data) {

    DEBUG_PRINT(F("memWrite: index=0x%02x data=0x%02x"), index, data);
    if (*_eepromWriteFunc != NULL) {
        DEBUG_PRINTLN(F(" using fctptr"));
        _eepromWriteFunc(index, data);
    } else {
        DEBUG_PRINTLN(F(""));
#ifdef ARDUINO_ARCH_SAMD
        DEBUG_PRINTLN(F("memoryWrite: EEPROM NOT SUPPORTED. USE FCTPTR!"));
#else
        EEPROM.write(index, data);
#endif 
    }
    // EEPROM has been changed, reboot will be required
    _rebootRequired = true;
}

void KonnektingDevice::memoryUpdate(int index, byte data) {

    DEBUG_PRINT(F("memUpdate: index=0x%02x data=0x%02x"), index, data);

    if (*_eepromUpdateFunc != NULL) {
        DEBUG_PRINTLN(F(" using fctptr"));
        _eepromUpdateFunc(index, data);
    } else {
        DEBUG_PRINTLN(F(""));
#if defined(ESP8266) || defined(ESP32)
        DEBUG_PRINTLN(F("ESP8266/ESP32: EEPROM.update"));
        byte d = EEPROM.read(index);
        if (d != data) {
            EEPROM.write(index, data);
        }
#elif ARDUINO_ARCH_SAMD   
        DEBUG_PRINTLN(F("memoryUpdate: EEPROM NOT SUPPORTED. USE FCTPTR!"));
#else
        EEPROM.update(index, data);
#endif
    }
    // EEPROM has been changed, reboot will be required
    _rebootRequired = true;

}

void KonnektingDevice::memoryCommit() {
    if (*_eepromCommitFunc != NULL) {
        DEBUG_PRINTLN(F("memCommit: using fctptr"));
        _eepromCommitFunc();
    }
}

/**************************************************************************/
/*!
 *  @brief  Gets the uint8 value of given parameter
 *  @param  index
 *          index of parameter
 *  @return uint8 value of parameter
 */
/**************************************************************************/
uint8_t KonnektingDevice::getUINT8Param(int index) {
    if (getParamSize(index) != PARAM_UINT8) {
        DEBUG_PRINTLN(F("Requested UINT8 param for index %d but param has different length! Will Return 0."), index);
        return 0;
    }

    byte paramValue[1];
    getParamValue(index, paramValue);

    return paramValue[0];
}

/**************************************************************************/
/*!
 *  @brief  Gets the int8 value of given parameter
 *  @param  index
 *          index of parameter
 *  @return int8 value of parameter
 */
/**************************************************************************/
int8_t KonnektingDevice::getINT8Param(int index) {
    if (getParamSize(index) != PARAM_INT8) {
        DEBUG_PRINTLN(F("Requested INT8 param for index %d but param has different length! Will Return 0."), index);
        return 0;
    }

    byte paramValue[1];
    getParamValue(index, paramValue);

    return paramValue[0];
}

/**************************************************************************/
/*!
 *  @brief  Gets the uint16 value of given parameter
 *  @param  index
 *          index of parameter
 *  @return uint16 value of parameter
 */
/**************************************************************************/
uint16_t KonnektingDevice::getUINT16Param(int index) {
    if (getParamSize(index) != PARAM_UINT16) {
        DEBUG_PRINTLN(F("Requested UINT16 param for index %d but param has different length! Will Return 0."), index);
        return 0;
    }

    byte paramValue[2];
    getParamValue(index, paramValue);

    uint16_t val = (paramValue[0] << 8) + (paramValue[1] << 0);

    return val;
}

/**************************************************************************/
/*!
 *  @brief  Gets the int16 value of given parameter
 *  @param  index
 *          index of parameter
 *  @return int16 value of parameter
 */
/**************************************************************************/
int16_t KonnektingDevice::getINT16Param(int index) {
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

/**************************************************************************/
/*!
 *  @brief  Gets the uint32 value of given parameter
 *  @param  index
 *          index of parameter
 *  @return uint32 value of parameter
 */
/**************************************************************************/
uint32_t KonnektingDevice::getUINT32Param(int index) {
    if (getParamSize(index) != PARAM_UINT32) {
        DEBUG_PRINTLN(F("Requested UINT32 param for index %d but param has different length! Will Return 0."), index);
        return 0;
    }

    byte paramValue[4];
    getParamValue(index, paramValue);

    uint32_t val = ((uint32_t) paramValue[0] << 24) + ((uint32_t) paramValue[1] << 16) + ((uint32_t) paramValue[2] << 8) + ((uint32_t) paramValue[3] << 0);

    return val;
}

/**************************************************************************/
/*!
 *  @brief  Gets the int32 value of given parameter
 *  @param  index
 *          index of parameter
 *  @return int32 value of parameter
 */
/**************************************************************************/
int32_t KonnektingDevice::getINT32Param(int index) {
    if (getParamSize(index) != PARAM_INT32) {
        DEBUG_PRINTLN(F("Requested INT32 param for index %d but param has different length! Will Return 0."), index);
        return 0;
    }

    byte paramValue[4];
    getParamValue(index, paramValue);

    int32_t val = ((uint32_t) paramValue[0] << 24) + ((uint32_t) paramValue[1] << 16) + ((uint32_t) paramValue[2] << 8) + ((uint32_t) paramValue[3] << 0);

    return val;
}

/**************************************************************************/
/*!
 *  @brief  Gets the string value of given parameter
 *  @param  index
 *          index of parameter
 *  @return string value of parameter
 */
/**************************************************************************/
String KonnektingDevice::getSTRING11Param(int index) {
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

/**************************************************************************/
/*!
 *  @brief  Returns the address at which the "user space" in eeprom starts.
 *          The area in front of this address is used by KONNEKTING and writing to this area is not allowed
 *  @return eeprom address at which the "user space" starts
 */
/**************************************************************************/
int KonnektingDevice::getFreeEepromOffset() {

    int offset = _paramTableStartindex;
    for (int i = 0; i < _numberOfParams; i++) {
        offset += _paramSizeList[i];
    }
    return offset;
}

/**************************************************************************/
/*!
 *  @brief  Sets the function to call when doing 'read' on memory.
 *  @param  func
 *          function pointer to memory read function
 *  @return void
 */
/**************************************************************************/
void KonnektingDevice::setMemoryReadFunc(byte(*func)(int)) {
    _eepromReadFunc = func;
}

/**************************************************************************/
/*!
 *  @brief  Sets the function to call when doing 'write' on memory.
 *  @param  func
 *          function pointer to memory write function
 *  @return void
 */
/**************************************************************************/
void KonnektingDevice::setMemoryWriteFunc(void (*func)(int, byte)) {
    _eepromWriteFunc = func;
}

/**************************************************************************/
/*!
 *  @brief  Sets the function to call when doing 'update' on memory.
 *  @param  func
 *          function pointer to memory update function
 *  @return void
 */
/**************************************************************************/
void KonnektingDevice::setMemoryUpdateFunc(void (*func)(int, byte)) {
    _eepromUpdateFunc = func;
}

/**************************************************************************/
/*!
 *  @brief  Sets the function to call when memory changes are done ('commit').
 *  @param  func
 *          function pointer to memory commit function
 *  @return void
 */
/**************************************************************************/
void KonnektingDevice::setMemoryCommitFunc(void (*func)(void)) {
    _eepromCommitFunc = func;
}
