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

/*
 * !!!!! IMPORTANT !!!!!
 * if "#define DEBUG" is set, you must run your KONNEKTING Suite with
 * "-Dde.root1.slicknx.konnekting.debug=true" A release-version of your
 * development MUST NOT contain "#define DEBUG" ...
 */

#define DEBUG_PROTOCOL
#define WRITEMEM
// reboot feature via progbutton
//#define REBOOT_BUTTON

#include "KonnektingDevice.h"
#include "Arduino.h"
#include "DebugUtil.h"
#include "KnxComObject.h"
#include "KnxDataPointTypes.h"
#include "KnxDevice.h"

#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif

#define PROGCOMOBJ_INDEX 255

// KonnektingDevice unique instance creation
KonnektingDevice KonnektingDevice::Konnekting;
KonnektingDevice &Konnekting = KonnektingDevice::Konnekting;  // maybe this line is useless??

// ---------------
// init static members, see https://thinkingeek.com/2012/08/08/common-linking-issues-c/
AssociationTable KonnektingDevice::_associationTable;
AddressTable KonnektingDevice::_addressTable;
byte KonnektingDevice::_assocMaxTableEntries = 0;
// ---------------

/**************************************************************************/
/*!
 *  @brief  Intercepting knx events to process internal com objects
 *  @param  index
 *              Comobject index
 *  @return void
 */
/**************************************************************************/
void konnektingKnxEvents(byte index) {
    DEBUG_PRINTLN(F("\n\nkonnektingKnxEvents index=%d"), index);

    // if it's not a internal com object, route back to knxEvents()
    if (!Konnekting.internalKnxEvents(index)) {
        knxEvents(index);
    }
}

/**************************************************************************/
/*!
 *  @brief  Instantiates a new KONNEKTING Device class
 */
/**************************************************************************/
KonnektingDevice::KonnektingDevice() {
    // no debug output here, as debug might not be initialized when constructor is called
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
void KonnektingDevice::internalInit(HardwareSerial &serial, word manufacturerID, byte deviceID, byte revisionID) {
    DEBUG_PRINTLN(F("Initialize KonnektingDevice (build date=%s time=%s)"), F(__DATE__), F(__TIME__));

    _initialized = true;

    _manufacturerID = manufacturerID;
    _deviceID = deviceID;
    _revisionID = revisionID;
#ifdef REBOOT_BUTTON
    _lastProgbtn = 0;
    _progbtnCount = 0;
#endif

    setProgState(false);

    // hardcoded stuff
    DEBUG_PRINTLN(F("Manufacturer: 0x%02x Device: 0x%02x Revision: 0x%02x"), _manufacturerID, _deviceID, _revisionID);

    _individualAddress = P_ADDR(1, 1, 254);

    // force read-only memory
    byte versionHi = memoryRead(0x0000);
    byte versionLo = memoryRead(0x0001);
    word version = __WORD(versionHi, versionLo);

    DEBUG_PRINTLN(F("version: 0x%04X expected: 0x%04X"), version, KONNEKTING_VERSION);

    /*
    DEBUG_PRINTLN(F("clear eeprom ..."));
    for (int i=0;i<2048;i++)  {
        memoryWrite(i, 0xFF);
    }
    DEBUG_PRINTLN(F("clear eeprom *done*"));
    */

    // FIXME when this is called? when doing programming?
    if (version != KONNEKTING_VERSION) {
        DEBUG_PRINTLN(F("##### setting read-only memory of system table for first time?..."));
        memoryWrite(0x0000, HI__(KONNEKTING_VERSION));
        memoryWrite(0x0001, __LO(KONNEKTING_VERSION));
        memoryWrite(0x0002, 0xff);  // device flags
        memoryWrite(0x0003, HI__(KONNEKTING_MEMORYADDRESS_ADDRESSTABLE));
        memoryWrite(0x0004, __LO(KONNEKTING_MEMORYADDRESS_ADDRESSTABLE));
        memoryWrite(0x0005, HI__(KONNEKTING_MEMORYADDRESS_ASSOCIATIONTABLE));
        memoryWrite(0x0006, __LO(KONNEKTING_MEMORYADDRESS_ASSOCIATIONTABLE));
        memoryWrite(0x0007, HI__(KONNEKTING_MEMORYADDRESS_COMMOBJECTTABLE));
        memoryWrite(0x0008, __LO(KONNEKTING_MEMORYADDRESS_COMMOBJECTTABLE));
        memoryWrite(0x0009, HI__(KONNEKTING_MEMORYADDRESS_PARAMETERTABLE));
        memoryWrite(0x000A, __LO(KONNEKTING_MEMORYADDRESS_PARAMETERTABLE));
        DEBUG_PRINTLN(F("##### setting read-only memory of system table *done*"));
    }

    DEBUG_PRINTLN(F("comobjs in sketch: %d"), Knx.getNumberOfComObjects());

    _deviceFlags = memoryRead(EEPROM_DEVICE_FLAGS);
    DEBUG_PRINTLN(F("_deviceFlags: (bin)" BYTETOBINARYPATTERN), BYTETOBINARY(_deviceFlags));

    if (!isFactorySetting()) {
        DEBUG_PRINTLN(F("->MEMORY"));
        /*
         * Read eeprom stuff
         */

        // PA
        byte hiAddr = memoryRead(EEPROM_INDIVIDUALADDRESS_HI);
        byte loAddr = memoryRead(EEPROM_INDIVIDUALADDRESS_LO);
        _individualAddress = __WORD(hiAddr, loAddr);
        DEBUG_PRINTLN(F("ia=0x%04x"), _individualAddress);

        // DEBUG_PRINTLN(F("KONNEKTING_MEMORYADDRESS_GROUPADDRESSTABLE = 0x%04x"), KONNEKTING_MEMORYADDRESS_ADDRESSTABLE);
        // DEBUG_PRINTLN(F("KONNEKTING_MEMORYADDRESS_ASSOCIATIONTABLE  = 0x%04x"), KONNEKTING_MEMORYADDRESS_ASSOCIATIONTABLE);
        // DEBUG_PRINTLN(F("KONNEKTING_MEMORYADDRESS_COMMOBJECTTABLE   = 0x%04x"), KONNEKTING_MEMORYADDRESS_COMMOBJECTTABLE);
        // DEBUG_PRINTLN(F("KONNEKTING_MEMORYADDRESS_PARAMETERTABLE    = 0x%04x"), KONNEKTING_MEMORYADDRESS_PARAMETERTABLE);

        DEBUG_PRINT(F("Reading commobj table..."));
        uint8_t commObjTableEntries = memoryRead(KONNEKTING_MEMORYADDRESS_COMMOBJECTTABLE);
        DEBUG_PRINTLN(F("%i entries"), commObjTableEntries);

        if (commObjTableEntries != Knx.getNumberOfComObjects()) {
            while (true) {
                DEBUG_PRINTLN(F("Knx init ERROR. ComObj size in sketch (%d) does not fit comobj size in memory (%d)."), Knx.getNumberOfComObjects(), commObjTableEntries);
                delay(1000);
            }
        }

        /* *************************************
         * read comobj configs from memory
         * *************************************/
        for (byte i = 0; i < Knx.getNumberOfComObjects(); i++) {
            byte config = memoryRead(KONNEKTING_MEMORYADDRESS_COMMOBJECTTABLE + 1 + i);
            DEBUG_PRINTLN(F("  ComObj #%d config: hex=0x%02x bin=" BYTETOBINARYPATTERN), i, config, BYTETOBINARY(config));
            // set comobj config
            Knx.setComObjectIndicator(i, config & 0x3F);
        }
        DEBUG_PRINTLN(F("Reading commobj table...*done*"));

        /* *************************************
         * read address table from memory
         * *************************************/
        DEBUG_PRINT(F("Reading address table..."));
        _addressTable.size = memoryRead(KONNEKTING_MEMORYADDRESS_ADDRESSTABLE);
        DEBUG_PRINTLN(F("%i entries"), _addressTable.size);

        _addressTable.address = (word *)malloc(_addressTable.size * sizeof(word));

        for (byte i = 0; i < _addressTable.size; i++) {
            byte gaHi = memoryRead(KONNEKTING_MEMORYADDRESS_ADDRESSTABLE + 1 + (i * 2));
            byte gaLo = memoryRead(KONNEKTING_MEMORYADDRESS_ADDRESSTABLE + 1 + (i * 2) + 1);
            word ga = __WORD(gaHi, gaLo);

            DEBUG_PRINTLN(F("  index=%d GA: hex=0x%02x"), i, ga);
            // store copy of addresstable in RAM
            _addressTable.address[i] = ga;
        }
        DEBUG_PRINTLN(F("Reading address table...*done*"));

        /* *************************************
         * read association table from memory
         * *************************************/
        DEBUG_PRINT(F("Reading association table..."));
        _associationTable.size = memoryRead(KONNEKTING_MEMORYADDRESS_ASSOCIATIONTABLE);
        DEBUG_PRINTLN(F("%i entries"), _associationTable.size);

        _associationTable.gaId = (byte *)malloc(_associationTable.size * sizeof(byte));
        _associationTable.coId = (byte *)malloc(_associationTable.size * sizeof(byte));

        int overallMax = 0;
        int currentMax = 0;
        int currentAddrId = -1;

        for (byte i = 0; i < _associationTable.size; i++) {
            byte addressId = memoryRead(KONNEKTING_MEMORYADDRESS_ASSOCIATIONTABLE + 1 + (i * 2));
            byte commObjectId = memoryRead(KONNEKTING_MEMORYADDRESS_ASSOCIATIONTABLE + 1 + (i * 2) + 1);

            if (currentAddrId == addressId) {
                currentMax++;
            } else {  // different GA detected

                // if the last known currentMax is bigger than the overallMax, replace overallMax
                if (currentMax > overallMax) {
                    overallMax = currentMax;
                }

                currentMax = 1;             // found different GA association, so we found 1 assoc for this GA so far
                currentAddrId = addressId;  // remember that we are currently counting for this address
            }

            // store copy of association table in RAM
            _associationTable.gaId[i] = addressId;
            _associationTable.coId[i] = commObjectId;

            // get group address by it's ID from already read address table
            word ga = _addressTable.address[addressId];

            DEBUG_PRINTLN(F("  index=%d ComObj=%d ga=0x%04x"), i, commObjectId, ga);

            Knx.setComObjectAddress(commObjectId, ga);
        }
        _assocMaxTableEntries = overallMax;
        DEBUG_PRINTLN(F("Reading association table...*done* _assocMaxTableEntries=%d"), _assocMaxTableEntries);

        // params are read either on demand or in setup() and not on init() ...

    } else {
        DEBUG_PRINTLN(F("->FACTORY"));
    }

    DEBUG_PRINTLN(F("IA: 0x%04x"), _individualAddress);
    KnxDeviceStatus status;
    status = Knx.begin(serial, _individualAddress);
    DEBUG_PRINTLN(F("KnxDevice startup status: 0x%02x"), status);

    if (status != KNX_DEVICE_OK) {
        DEBUG_PRINTLN(F("Knx init ERROR. Retry after reboot!!"));
        delay(500);
        reboot();
    }

#if defined(ESP8266) || defined(ESP32)
    // ESP has no EEPROM, but flash and needs to init the EEPROM emulator with
    // an initial size. We create 8k EEPROM
    EEPROM.begin(8192);
#endif
    _rebootRequired = false;
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
void KonnektingDevice::init(HardwareSerial &serial,
                            void (*progIndicatorFunc)(bool),
                            word manufacturerID, byte deviceID,
                            byte revisionID) {
    _progIndicatorFunc = progIndicatorFunc;

    internalInit(serial, manufacturerID, deviceID, revisionID);
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
void KonnektingDevice::init(HardwareSerial &serial, int progButtonPin,
                            int progLedPin, word manufacturerID, byte deviceID,
                            byte revisionID) {
    _progLED = progLedPin;
    _progButton = progButtonPin;

    pinMode(_progLED, OUTPUT);
    pinMode(_progButton, INPUT);
    attachInterrupt(digitalPinToInterrupt(_progButton),
                    KonnektingProgButtonPressed, RISING);

    internalInit(serial, manufacturerID, deviceID, revisionID);
}

/**************************************************************************/
/*!
 *  @brief  checks if the device is already initialized and active.
 *  @return True if the device has been initialized (by calling one of the
 * init() functions).
 */
/**************************************************************************/
bool KonnektingDevice::isActive() { return _initialized; }

/**************************************************************************/
/*!
 *  @brief  checks if the device is in factory settings mode, means: has not
 * been programmed so far.
 *  @return True if factory settings are active
 */
/**************************************************************************/
bool KonnektingDevice::isFactorySetting() {
    // see: https://wiki.konnekting.de/index.php/KONNEKTING_Protocol_Specification_0x01#Device_Flags
    bool isFactory = _deviceFlags == 0xff || (_deviceFlags & 0x80) == 0x80;
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

    DEBUG_PRINTLN(F("getParamValue: index=%d paramTableStartIndex=%d "
                    "skipbytes=%d paremLen=%d"),
                  index, KONNEKTING_MEMORYADDRESS_PARAMETERTABLE, skipBytes, paramLen);

    // read byte by byte
    for (int i = 0; i < paramLen; i++) {
        int addr = KONNEKTING_MEMORYADDRESS_PARAMETERTABLE + skipBytes + i;

        value[i] = memoryRead(addr);
        DEBUG_PRINTLN(F(" val[%d]@%d -> 0x%02x"), i, addr, value[i]);
    }
}

/**************************************************************************/
/*!
 *  @brief  Interrupt Service Routines for the prog-button
 *          Used when actually using the built-int prog-button-feature that
 * requires an interrupt enabled pin. Although this function is "public", it's
 * NOT part of the API and should not be called by users.
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
    setProgState(!_progState);  // toggle and set
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
    if (!isProgState() && _rebootRequired) {
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
bool KonnektingDevice::isProgState() { return _progState; }

/**************************************************************************/
/*!
 *  @brief  Check whether Konnekting is ready for application logic.
 *          Means: not busy with programming-mode and not running with factory
 * settings
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
    DEBUG_PRINTLN(F("setProgState=%d"), state);
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
    } else {
        digitalWrite(_progLED, state);
    }
    DEBUG_PRINTLN(F("setProgLed=%d"), state);
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
    return p;
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
    // do reset of arduino zero, inspired by
    // http://forum.arduino.cc/index.php?topic=366836.0
    DEBUG_PRINTLN(F("SAMD SystemReset"));

    // disable watchdog
    WDT->CTRL.reg = 0;

    // Just wait till WDT is free
    while (WDT->STATUS.bit.SYNCBUSY == 1)
        ;

    // see Table 17-5 Timeout Period (valid values 0-11)
    WDT->CONFIG.reg = 0;

    // enable watchdog
    WDT->CTRL.reg = WDT_CTRL_ENABLE;

    // Just wait till WDT is free
    while (WDT->STATUS.bit.SYNCBUSY == 1)
        ;
    // forever loop until WDT fires
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
    asm volatile("  jmp 0");
#endif
}

/**************************************************************************/
/*!
 *  @brief  processes internal comobj for programming purpose.
 *          This method is feed with _all_ comobj events. If it's internal
 * comobj, it's handled and return value is true. If it's a user defined comobj,
 * all processing is skipped and false is returned.
 *  @param  index
 *          index of incoming comobj
 *  @return true, if index was internal comobject and has been handled, false if
 * not
 */
/**************************************************************************/
bool KonnektingDevice::internalKnxEvents(byte index) {
    DEBUG_PRINTLN(F("internalKnxEvents index=%d"), index);
    bool consumed = false;
    switch (index) {
        case 255:  // prog com object index 255 has been updated

            byte buffer[14];
            Knx.read(PROGCOMOBJ_INDEX, buffer);
#ifdef DEBUG_PROTOCOL
            for (int i = 0; i < 14; i++) {
                DEBUG_PRINTLN(
                    F("buffer[%02d]\thex=0x%02x bin=" BYTETOBINARYPATTERN), i,
                    buffer[i], BYTETOBINARY(buffer[i]));
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
                    case MSGTYPE_PROPERTY_PAGE_READ:
                        handleMsgPropertyPageRead(buffer);
                        break;
                    case MSGTYPE_RESTART:
                        handleMsgRestart(buffer);
                        break;
                    case MSGTYPE_PROGRAMMING_MODE_WRITE:
                        handleMsgProgrammingModeWrite(buffer);
                        break;
                    case MSGTYPE_PROGRAMMING_MODE_READ:
                        handleMsgProgrammingModeRead(buffer);
                        break;
                    case MSGTYPE_MEMORY_WRITE:
                        if (_progState) handleMsgMemoryWrite(buffer);
                        break;
                    case MSGTYPE_MEMORY_READ:
                        if (_progState) handleMsgMemoryRead(buffer);
                        break;
                    case MSGTYPE_DATA_WRITE_PREPARE:
                        if (_progState) handleMsgDataWritePrepare(buffer);
                        break;
                    case MSGTYPE_DATA_WRITE:
                        if (_progState) handleMsgDataWrite(buffer);
                        break;
                    case MSGTYPE_DATA_WRITE_FINISH:
                        if (_progState) handleMsgDataWriteFinish(buffer);
                        break;
                    case MSGTYPE_DATA_READ:
                        if (_progState) handleMsgDataRead(buffer);
                        break;
                    case MSGTYPE_DATA_READ_RESPONSE:
                        DEBUG_PRINTLN(F("Will not handle received MSGTYPE_DATA_READ_RESPONSE. Skipping message."));
                        break;
                    case MSGTYPE_DATA_READ_DATA:
                        DEBUG_PRINTLN(F("Will not handle received MSGTYPE_DATA_READ_DATA. Skipping message."));
                        break;
                    case MSGTYPE_DATA_REMOVE:
                        if (_progState) handleMsgDataRemove(buffer);
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
void KonnektingDevice::sendMsgAck(byte ackType, byte errorCode) {
    DEBUG_PRINTLN(F("sendMsgAck ackType=0x%02x errorCode=0x%02x"), ackType, errorCode);
    byte response[14];
    response[0] = PROTOCOLVERSION;
    response[1] = MSGTYPE_ACK;
    response[2] = ackType;
    response[3] = errorCode;
    fillEmpty(response, 4);

    Knx.write(PROGCOMOBJ_INDEX, response);
}

void KonnektingDevice::handleMsgPropertyPageRead(byte msg[]) {
    DEBUG_PRINTLN(F("handleMsgPropertyPageRead"));

    byte addressFlag = msg[2];

    boolean doReply = false;
#ifdef DEBUG_PROTOCOL
    DEBUG_PRINTLN(F(" addressFlag=0x%02X"), addressFlag);
#endif

    if (addressFlag == 0x00 && isProgState()) {
        doReply = true;
#ifdef DEBUG_PROTOCOL
        DEBUG_PRINTLN(F(" reply due to prog button"));
#endif
    } else if (addressFlag = 0xFF && _individualAddress == __WORD(msg[3], msg[4])) {
#ifdef DEBUG_PROTOCOL
        DEBUG_PRINTLN(F(" reply due to matching IA"));
#endif
        doReply = true;
    }

    if (doReply) {
        byte response[14];
#ifdef DEBUG_PROTOCOL
        DEBUG_PRINTLN(F(" reply page #%i"), msg[5]);
#endif
        switch (msg[5]) {
            case 0x00:  // Device Info
                response[0] = PROTOCOLVERSION;
                response[1] = MSGTYPE_PROPERTY_PAGE_RESPONSE;
                response[2] = HI__(_manufacturerID);
                response[3] = __LO(_manufacturerID);
                response[4] = _deviceID;
                response[5] = _revisionID;
                response[6] = _deviceFlags;
                response[7] = SYSTEM_TYPE_DEFAULT;
                fillEmpty(response, 8);
                break;
            default:
                fillEmpty(response, 0);
                break;
        }
        DEBUG_PRINTLN(F("handleMsgPropertyPageRead send response"));
        Knx.write(PROGCOMOBJ_INDEX, response);
    }
    DEBUG_PRINTLN(F("handleMsgPropertyPageRead *done*"));
}

void KonnektingDevice::handleMsgRestart(byte msg[]) {
    DEBUG_PRINTLN(F("handleMsgRestart"));

    if (_individualAddress == __WORD(msg[2], msg[3])) {
#ifdef DEBUG_PROTOCOL
        DEBUG_PRINTLN(F("matching IA"));
#endif
        // trigger restart
        reboot();
    } else {
#ifdef DEBUG_PROTOCOL
        DEBUG_PRINTLN(F("no matching: IA 0x%04X <- 0x%04X"), _individualAddress, __WORD(msg[2], msg[3]));
#endif
    }
}

void KonnektingDevice::handleMsgProgrammingModeWrite(byte msg[]) {
    DEBUG_PRINTLN(F("handleMsgProgrammingModeWrite"));
    word addr = __WORD(msg[2], msg[3]);

    if (_individualAddress == addr) {
#ifdef DEBUG_PROTOCOL
        DEBUG_PRINTLN(F("matching IA"));
#endif
        setProgState(msg[4] == 0x01);
        sendMsgAck(ACK, ERR_CODE_OK);

        if (msg[4] == 0x00) {
#if defined(ESP8266) || defined(ESP32)
            // ESP8266/ESP32 uses own EEPROM implementation which requires commit() call
            DEBUG_PRINTLN(F("ESP8266/ESP32: EEPROM.commit()"));
            EEPROM.commit();
#else
            // commit memory changes
            memoryCommit();
#endif
        }

    } else {
        DEBUG_PRINTLN(F("no matching IA. self=0x%04X got=0x%04X"), _individualAddress, addr);
    }
}

void KonnektingDevice::handleMsgProgrammingModeRead(byte /*msg*/[]) {
    // to suppress compiler warning about unused variable, "msg" has been
    // commented out
    DEBUG_PRINTLN(F("handleMsgProgrammingModeRead"));
    if (_progState) {
        byte response[14];
        response[0] = PROTOCOLVERSION;
        response[1] = MSGTYPE_PROGRAMMING_MODE_RESPONSE;
        response[2] = HI__(_individualAddress);
        response[3] = __LO(_individualAddress);

        fillEmpty(response, 4);
        DEBUG_PRINTLN(F("handleMsgProgrammingModeRead send response"));
        Knx.write(PROGCOMOBJ_INDEX, response);
    }
    DEBUG_PRINTLN(F("handleMsgProgrammingModeRead *done*"));
}

void KonnektingDevice::handleMsgMemoryWrite(byte msg[]) {
    DEBUG_PRINTLN(F("handleMsgMemoryWrite"));

    boolean systemTableChanged = false;

    uint8_t count = msg[2];
    uint16_t startAddr = __WORD(msg[3], msg[4]);
    DEBUG_PRINTLN(F("  count=%d startAddr=0x%04x"), count, startAddr);

    if (startAddr >= 16 && startAddr < 32 && count > 0) {
        systemTableChanged = true;
    }

    for (uint8_t i = 0; i < count; i++) {
        uint16_t addr = startAddr + i;
        byte data = msg[5 + i];

        memoryWrite(addr, data);
    }
    if (isFactorySetting()) {
        // clear factory setting bit to 0
        _deviceFlags &= ~0x80;
        DEBUG_PRINTLN(F(" toggled factory setting in device flags: (bin)" BYTETOBINARYPATTERN), BYTETOBINARY(_deviceFlags));
        memoryWrite(EEPROM_DEVICE_FLAGS, _deviceFlags);
    }
    if (systemTableChanged) {
        DEBUG_PRINTLN(F(" reload system table data due to change"));
        // reload all system table related r/w data
        byte hiAddr = memoryRead(EEPROM_INDIVIDUALADDRESS_HI);
        byte loAddr = memoryRead(EEPROM_INDIVIDUALADDRESS_LO);
        _individualAddress = __WORD(hiAddr, loAddr);
    }
    sendMsgAck(ACK, ERR_CODE_OK);
    DEBUG_PRINTLN(F("handleMsgMemoryWrite *done*"));
}

void KonnektingDevice::handleMsgMemoryRead(byte msg[]) {
    DEBUG_PRINTLN(F("handleMsgMemoryRead"));

    uint8_t count = msg[2];
    uint16_t startAddr = __WORD(msg[3], msg[4]);
    DEBUG_PRINTLN(F("  count=%d startAddr=0x%04x"), count, startAddr);

    byte response[14];
    response[0] = PROTOCOLVERSION;
    response[1] = MSGTYPE_MEMORY_RESPONSE;
    response[2] = count;
    response[3] = HI__(startAddr);
    response[4] = __LO(startAddr);

    // read data from eeprom and put into answer message
    for (uint8_t i = 0; i < count; i++) {
        int addr = startAddr + i;

        response[5 + i] = memoryRead(addr);
    }
    fillEmpty(response, 5 + count);

    Knx.write(PROGCOMOBJ_INDEX, response);
    DEBUG_PRINTLN(F("handleMsgMemoryRead *done*"));
}

void KonnektingDevice::handleMsgDataWritePrepare(byte msg[]) {
    DEBUG_PRINTLN(F("handleMsgDataWritePrepare"));
    if (*_dataWritePrepareFunc != NULL) {
        
        DataWritePrepare dwp;
        dwp.type = msg[2];
        dwp.id = msg[3];
        dwp.size = __DWORD(msg[4], msg[5], msg[6], msg[7]);

        DEBUG_PRINT(F(" using fctptr"));
        bool result = _dataWritePrepareFunc(dwp);
        if (!result) {
            sendMsgAck(NACK, ERR_CODE_DATA_WRITE_PREPARE_FAILED);
        } else {
            sendMsgAck(ACK, ERR_CODE_OK);
        }

    } else {
        DEBUG_PRINTLN(F("handleMsgDataWritePrepare: missing FCTPTR!"));
        sendMsgAck(NACK, ERR_CODE_NOT_SUPPORTED);
    }
}

void KonnektingDevice::handleMsgDataWrite(byte msg[]) {
    DEBUG_PRINTLN(F("handleMsgDataWrite"));
    if (*_dataWriteFunc != NULL) {

        DataWrite dw;
        dw.count = msg[2];

        memcpy(&dw.data[0], &msg[3], dw.count);

        DEBUG_PRINT(F(" using fctptr"));
        bool result = _dataWriteFunc(dw);
        if (!result) {
            sendMsgAck(NACK, ERR_CODE_DATA_WRITE_FAILED);
        } else {
            sendMsgAck(ACK, ERR_CODE_OK);
        }

    } else {
        DEBUG_PRINTLN(F("handleMsgDataWrite: missing FCTPTR!"));
        sendMsgAck(NACK, ERR_CODE_NOT_SUPPORTED);
    }
}

void KonnektingDevice::handleMsgDataWriteFinish(byte msg[]) {
    DEBUG_PRINTLN(F("handleMsgDataWriteFinish"));
    if (*_dataWriteFinishFunc != NULL) {

        unsigned long crc32 = __DWORD(msg[2], msg[3], msg[4], msg[5]);
        
        DEBUG_PRINT(F(" using fctptr"));
        bool result = _dataWriteFinishFunc(crc32);
        if (!result) {
            sendMsgAck(NACK, ERR_CODE_DATA_WRITE_CRC_FAILED);
        } else {
            sendMsgAck(ACK, ERR_CODE_OK);
        }

    } else {
        DEBUG_PRINTLN(F("handleMsgDataWrite: missing FCTPTR!"));
        sendMsgAck(NACK, ERR_CODE_NOT_SUPPORTED);
    }
}

void KonnektingDevice::handleMsgDataRead(byte msg[]) {
    // TODO
}

void KonnektingDevice::sendMsgDataReadResponse(byte msg[]) {
    // TODO
}

void KonnektingDevice::sendMsgDataReadData(byte *msg) {
    // TODO
}

void KonnektingDevice::handleMsgDataRemove(byte msg[]) {
    // TODO
}

byte KonnektingDevice::memoryRead(int index) {
    DEBUG_PRINT(F("memRead: index=0x%04x"), index);
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
    DEBUG_PRINT(F("memWrite: index=0x%04x data=0x%02x"), index, data);
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
 *  @brief  Fills remainig bytes with 0xFF
 *  @param  startIndex
 *          index at which to start filling with 0xFF
 *  @return void
 */
/**************************************************************************/
void KonnektingDevice::fillEmpty(byte msg[], int startIndex) {
    for (int i = startIndex; i < MSG_LENGTH; i++) {
        msg[i] = 0xFF;
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
        DEBUG_PRINTLN(F("Requested UINT8 param for index %d but param has "
                        "different length! Will Return 0."),
                      index);
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
        DEBUG_PRINTLN(
            F("Requested INT8 param for index %d but param has different "
              "length! Will Return 0."),
            index);
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
        DEBUG_PRINTLN(F("Requested UINT16 param for index %d but param has "
                        "different length! Will Return 0."),
                      index);
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
        DEBUG_PRINTLN(F("Requested INT16 param for index %d but param has "
                        "different length! Will Return 0."),
                      index);
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
        DEBUG_PRINTLN(F("Requested UINT32 param for index %d but param has "
                        "different length! Will Return 0."),
                      index);
        return 0;
    }

    byte paramValue[4];
    getParamValue(index, paramValue);

    uint32_t val =
        ((uint32_t)paramValue[0] << 24) + ((uint32_t)paramValue[1] << 16) +
        ((uint32_t)paramValue[2] << 8) + ((uint32_t)paramValue[3] << 0);

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
        DEBUG_PRINTLN(F("Requested INT32 param for index %d but param has "
                        "different length! Will Return 0."),
                      index);
        return 0;
    }

    byte paramValue[4];
    getParamValue(index, paramValue);

    int32_t val =
        ((uint32_t)paramValue[0] << 24) + ((uint32_t)paramValue[1] << 16) +
        ((uint32_t)paramValue[2] << 8) + ((uint32_t)paramValue[3] << 0);

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
        DEBUG_PRINTLN(F("Requested STRING11 param for index %d but param has "
                        "different length! Will Return \"\""),
                      index);
        ret = "";
        return ret;
    }

    byte paramValue[PARAM_STRING11];
    getParamValue(index, paramValue);

    // check if string is 0x00 terminated (means <11 chars)
    for (int i = 0; i < PARAM_STRING11; i++) {
        if (paramValue[i] == 0x00) {
            break;  // stop at null-termination
        } else {
            ret += (char)paramValue[i];  // copy char by char into string
        }
    }

    return ret;
}

/**************************************************************************/
/*!
 *  @brief  Returns the address at which the "user space" in eeprom starts.
 *          The area in front of this address is used by KONNEKTING and writing
 * to this area is not allowed
 *  @return eeprom address at which the "user space" starts
 */
/**************************************************************************/
int KonnektingDevice::getFreeEepromOffset() {
    int offset = KONNEKTING_MEMORYADDRESS_PARAMETERTABLE;
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
void KonnektingDevice::setMemoryReadFunc(byte (*func)(int)) {
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

void KonnektingDevice::setDataWritePrepareFunc(bool (*func)(DataWritePrepare)) {
    _dataWritePrepareFunc = func;
}
void KonnektingDevice::setDataWriteFunc(bool (*func)(DataWrite)) {
    _dataWriteFunc = func;
}
void KonnektingDevice::setDataWriteFinishFunc(bool (*func)(unsigned long)) {
    _dataWriteFinishFunc = func;
}