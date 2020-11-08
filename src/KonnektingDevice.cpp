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
    DEBUG_PRINTLN(F("konnektingKnxEvents index=%d"), index);

    // if it's not a internal com object, route back to knxEvents()
    if (!Konnekting.internalKnxEvents(index)) {
        knxEvents(index);
    }
    DEBUG_PRINTLN(F("\n\n"));
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
    DEBUG_PRINTLN(F("Manufacturer: 0x%04X Device: 0x%02X Revision: 0x%02X SystemType=0x%02X"), _manufacturerID, _deviceID, _revisionID, KONNEKTING_SYSTEM_TYPE);

    _individualAddress = P_ADDR(1, 1, 254); // default IA is none is set

    // force read-only memory
    byte versionHi = memoryRead(SYSTEMTABLE_VERSION + 0);
    byte versionLo = memoryRead(SYSTEMTABLE_VERSION + 1);
    word version = __WORD(versionHi, versionLo);

    DEBUG_PRINTLN(F("version: 0x%04X expected: 0x%04X"), version, KONNEKTING_VERSION);

    /*
    DEBUG_PRINTLN(F("clear eeprom ..."));
    for (int i=0;i<2048;i++)  {
        memoryWrite(i, 0xFF);
    }
    DEBUG_PRINTLN(F("clear eeprom *done*"));
    */

    // if version from memory does not match this sketch, force to update/set read-only part of system table
    if (version != KONNEKTING_VERSION) {
        DEBUG_PRINTLN(F("##### setting read-only memory of system table due to version mismatch..."));
        memoryWrite(SYSTEMTABLE_VERSION + 0,                  HI__(KONNEKTING_VERSION));
        memoryWrite(SYSTEMTABLE_VERSION + 1,                  __LO(KONNEKTING_VERSION));
        memoryWrite(SYSTEMTABLE_DEVICE_FLAGS,                 0xFF); // device flags
        memoryWrite(SYSTEMTABLE_ADDRESSTABLE_ADDRESS + 0,     HI__(KONNEKTING_MEMORYADDRESS_ADDRESSTABLE));
        memoryWrite(SYSTEMTABLE_ADDRESSTABLE_ADDRESS + 1,     __LO(KONNEKTING_MEMORYADDRESS_ADDRESSTABLE));
        memoryWrite(SYSTEMTABLE_ASSOCIATIONTABLE_ADDRESS + 0, HI__(KONNEKTING_MEMORYADDRESS_ASSOCIATIONTABLE));
        memoryWrite(SYSTEMTABLE_ASSOCIATIONTABLE_ADDRESS + 1, __LO(KONNEKTING_MEMORYADDRESS_ASSOCIATIONTABLE));
        memoryWrite(SYSTEMTABLE_COMMOBJECTTABLE_ADDRESS + 0,  HI__(KONNEKTING_MEMORYADDRESS_COMMOBJECTTABLE));
        memoryWrite(SYSTEMTABLE_COMMOBJECTTABLE_ADDRESS + 1,  __LO(KONNEKTING_MEMORYADDRESS_COMMOBJECTTABLE));
        memoryWrite(SYSTEMTABLE_PARAMETERTABLE_ADDRESS + 0,   HI__(KONNEKTING_MEMORYADDRESS_PARAMETERTABLE));
        memoryWrite(SYSTEMTABLE_PARAMETERTABLE_ADDRESS + 1,   __LO(KONNEKTING_MEMORYADDRESS_PARAMETERTABLE));
        DEBUG_PRINTLN(F("##### setting read-only memory of system table *done*"));
    }

    // verify CRC of tables
    bool deviceFlagDirty = false;
    if (!checkTableCRC(CHECKSUM_ID_SYSTEM_TABLE)) {
        DEBUG_PRINTLN(F("!!! SystemTable CRC bad."));
        _deviceFlags |= DEVICEFLAG_IA_BIT; // set bit with "|=", see https://stackoverflow.com/questions/47981/how-do-you-set-clear-and-toggle-a-single-bit
        deviceFlagDirty = true;
    }
    if (!checkTableCRC(CHECKSUM_ID_ADDRESS_TABLE)) {
        DEBUG_PRINTLN(F("!!! AddressTable CRC bad."));
        _deviceFlags |= DEVICEFLAG_CO_BIT;
        deviceFlagDirty = true;
    }
    if (!checkTableCRC(CHECKSUM_ID_ASSOCIATION_TABLE)) {
        DEBUG_PRINTLN(F("!!! AssocTable CRC bad."));
        _deviceFlags |= DEVICEFLAG_CO_BIT;
        deviceFlagDirty = true;
    }
    if (!checkTableCRC(CHECKSUM_ID_COMMOBJECT_TABLE)) {
        DEBUG_PRINTLN(F("!!! CommObjTable CRC bad."));
        _deviceFlags |= DEVICEFLAG_CO_BIT;
        deviceFlagDirty = true;
    }
    if (!checkTableCRC(CHECKSUM_ID_PARAMETER_TABLE)) {
        DEBUG_PRINTLN(F("!!! ParamTable CRC bad."));
        _deviceFlags |= DEVICEFLAG_PARAM_BIT;
        deviceFlagDirty = true;
    }
    if (deviceFlagDirty) {
        DEBUG_PRINTLN(F("update device flag due to CRC issues"));
        memoryWrite(SYSTEMTABLE_DEVICE_FLAGS, _deviceFlags); 
    }

    DEBUG_PRINTLN(F("comobjs in sketch: %d"), Knx.getNumberOfComObjects());

    _deviceFlags = memoryRead(SYSTEMTABLE_DEVICE_FLAGS);
    DEBUG_PRINTLN(F("_deviceFlags: (bin)" BYTETOBINARYPATTERN), BYTETOBINARY(_deviceFlags));

    if (!isFactorySetting()) {
        DEBUG_PRINTLN(F("->MEMORY"));
        /*
         * Read eeprom stuff
         */

        if (isIndividualAddressSet()) {
            // PA
            byte hiAddr = memoryRead(SYSTEMTABLE_INDIVIDUALADDRESS + 0);
            byte loAddr = memoryRead(SYSTEMTABLE_INDIVIDUALADDRESS + 1);
            _individualAddress = __WORD(hiAddr, loAddr);
        }
        DEBUG_PRINTLN(F("ia=0x%04x"), _individualAddress);

        // DEBUG_PRINTLN(F("KONNEKTING_MEMORYADDRESS_GROUPADDRESSTABLE = 0x%04x"), KONNEKTING_MEMORYADDRESS_ADDRESSTABLE);
        // DEBUG_PRINTLN(F("KONNEKTING_MEMORYADDRESS_ASSOCIATIONTABLE  = 0x%04x"), KONNEKTING_MEMORYADDRESS_ASSOCIATIONTABLE);
        // DEBUG_PRINTLN(F("KONNEKTING_MEMORYADDRESS_COMMOBJECTTABLE   = 0x%04x"), KONNEKTING_MEMORYADDRESS_COMMOBJECTTABLE);
        // DEBUG_PRINTLN(F("KONNEKTING_MEMORYADDRESS_PARAMETERTABLE    = 0x%04x"), KONNEKTING_MEMORYADDRESS_PARAMETERTABLE);

        if (isComObjSet()) {
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
            int currentAddrId = 0;
            bool is1stAddrId = true; // when we start, we need a flag to detect the start to set the first id for addressId comparison 

            for (byte i = 0; i < _associationTable.size; i++) {
                byte addressId = memoryRead(KONNEKTING_MEMORYADDRESS_ASSOCIATIONTABLE + 1 + (i * 2));
                byte commObjectId = memoryRead(KONNEKTING_MEMORYADDRESS_ASSOCIATIONTABLE + 1 + (i * 2) + 1);

                if (is1stAddrId) {
                    currentAddrId = addressId;
                    is1stAddrId = false;
                }

                if (currentAddrId == addressId) {
                    currentMax++; // increase counter for current GA association count

                } else {  // different GA detected
                    currentMax = 1; // found different GA association, so we found 1 assoc for this GA so far
                    currentAddrId = addressId; // remember that we are currently counting for this address

                }
                //DEBUG_PRINTLN(F("  currentMax=%d"), currentMax);

                // if the last known currentMax is bigger than the overallMax, replace overallMax
                if (currentMax > overallMax) {
                    overallMax = currentMax;
                    //DEBUG_PRINTLN(F("  new overallMax=%d"), overallMax);
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
        }
        
        // params are read either on demand or in setup() and not on init() ...

    } else {
        DEBUG_PRINTLN(F("->FACTORY"));

        // ensure the tables are empty and contain reliable content
        _addressTable.size = 0; // no single GA has been set
        _addressTable.address = (word *)malloc(0); // and there is no single cell in table reserved for entries
        _associationTable.size = 0; // no single GA has been assigned
        _associationTable.gaId = (byte *)malloc(0); // and there is no single cell in table reserved for entries
        _associationTable.coId = (byte *)malloc(0); // ...
        
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
    bool isFactory = _deviceFlags == 0xFF || (_deviceFlags & DEVICEFLAG_FACTORY_BIT) == DEVICEFLAG_FACTORY_BIT;
    return isFactory;
}

bool KonnektingDevice::isIndividualAddressSet() {
    // see: https://wiki.konnekting.de/index.php/KONNEKTING_Protocol_Specification_0x01#Device_Flags
    bool isSet = (_deviceFlags & DEVICEFLAG_IA_BIT) == 0;
    return isSet;
}

bool KonnektingDevice::isComObjSet() {
    // see: https://wiki.konnekting.de/index.php/KONNEKTING_Protocol_Specification_0x01#Device_Flags
    bool isSet = (_deviceFlags & DEVICEFLAG_CO_BIT) == 0;
    return isSet;
}

bool KonnektingDevice::isParamsSet() {
    // see: https://wiki.konnekting.de/index.php/KONNEKTING_Protocol_Specification_0x01#Device_Flags
    bool isSet = (_deviceFlags & DEVICEFLAG_PARAM_BIT) == 0;
    return isSet;
}

bool KonnektingDevice::isDataSet() {
    // see: https://wiki.konnekting.de/index.php/KONNEKTING_Protocol_Specification_0x01#Device_Flags
    bool isSet = (_deviceFlags & DEVICEFLAG_DATA_BIT) == 0;
    return isSet;
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

    bool consumed = false;
    switch (index) {
        case 255:  // prog com object index 255 has been updated

            byte buffer[14];
            Knx.read(PROGCOMOBJ_INDEX, buffer);
#ifdef DEBUG_PROTOCOL
            // for (int i = 0; i < 14; i++) {
            //     DEBUG_PRINTLN(F("buffer[%02d]\thex=0x%02x bin=" BYTETOBINARYPATTERN), i, buffer[i], BYTETOBINARY(buffer[i]));
            // }
#endif

            byte protocolversion = buffer[0];
            byte msgType = buffer[1];

            // DEBUG_PRINTLN(F("protocolversion=0x%02x"), protocolversion);

            // DEBUG_PRINTLN(F("msgType=0x%02x"), msgType);

            if (protocolversion != PROTOCOLVERSION) {
                DEBUG_PRINTLN(F("Unsupported protocol version. Using: %d Got: %d !"), PROTOCOLVERSION, protocolversion);
            } else {
                switch (msgType) {
                    case MSGTYPE_ACK:
                        handleMsgAck(buffer);
                        break;
                    case MSGTYPE_CHECKSUM_SET:
                        handleMsgChecksumSet(buffer);
                        break;
                    case MSGTYPE_PROPERTY_PAGE_READ:
                        handleMsgPropertyPageRead(buffer);
                        break;
                    case MSGTYPE_UNLOAD:
                        handleMsgUnload(buffer);
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
 *  @param  ackType
 *          type of acknowledge, see https://wiki.konnekting.de/index.php/KONNEKTING_Protocol_Specification_0x01#Acknowledge_Type
 *  @param  errorCode
 *          code that determinates the error, see https://wiki.konnekting.de/index.php/KONNEKTING_Protocol_Specification_0x01#Error_Code
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

bool KonnektingDevice::waitForAck(byte ackCountBefore, unsigned long timeout) {
    unsigned long start = micros();
    timeout *= 1000;  // convert ms to µs
    DEBUG_PRINTLN(F("Waiting for ACK before=%i curr=%i micros=%ld"), ackCountBefore, _ackCounter, micros());
    while ((micros() < (start + timeout)) && _ackCounter == ackCountBefore) {
        if (micros() % 100 * 1000 /* every 100ms */ == 1) DEBUG_PRINTLN(F("\tstart=%ld timeout=%ld micros=%ld before=%i ackCount=%i"), start, timeout, micros(), ackCountBefore, _ackCounter);
        Knx.task();
    }
    bool result = _ackCounter > ackCountBefore;
    DEBUG_PRINTLN(F("Waiting for ACK *done* result=%i curr=%i micros=%ld"), result, _ackCounter, micros());
    return result;  // no ACK received --> timeout
}

void KonnektingDevice::handleMsgAck(byte msg[]) {
    byte type = msg[2];
    byte errCode = msg[3];
    DEBUG_PRINTLN(F("handleMsgAck type=0x%02x errCode=0x%02x"), type, errCode);
    if (type == 0x00) {
        _ackCounter++;
    }
}

bool KonnektingDevice::checkTableCRC(byte crcId){

    DEBUG_PRINTLN(F("checkTableCRC id=0x%02X"), crcId);

    // memory index at which we start reading bytes to calculate crc value for comparison
    int crcCheckStartIndex = -1;
    // number of bytes we need to read from memory
    int crcCheckLength = -1;

    // memory index at whoch we find the current CRC32 value (4 bytes)
    int crcIndex = -1;
    switch(crcId) {
        case CHECKSUM_ID_SYSTEM_TABLE:
            crcIndex = SYSTEMTABLE_CRC_SYSTEMTABLE;
            // only r/w part of system table
            crcCheckStartIndex = 48; 
            crcCheckLength = 16; 
            break;
        case CHECKSUM_ID_ADDRESS_TABLE:
            crcIndex = SYSTEMTABLE_CRC_ADDRESSTABLE;
            crcCheckStartIndex = KONNEKTING_MEMORYADDRESS_ADDRESSTABLE;
            crcCheckLength = 1 + (KONNEKTING_NUMBER_OF_ADDRESSES * 2); // 2 bytes per address plus 1 byte for size information
            break;
        case CHECKSUM_ID_ASSOCIATION_TABLE:
            crcIndex = SYSTEMTABLE_CRC_ASSOCIATIONTABLE;
            crcCheckStartIndex = KONNEKTING_MEMORYADDRESS_ASSOCIATIONTABLE;
            crcCheckLength = 1 + (KONNEKTING_NUMBER_OF_ASSOCIATIONS * 2); // 2 bytes per association plus 1 byte for size information
            break;
        case CHECKSUM_ID_COMMOBJECT_TABLE:
            crcIndex = SYSTEMTABLE_CRC_COMMOBJECTTABLE;
            crcCheckStartIndex = KONNEKTING_MEMORYADDRESS_COMMOBJECTTABLE;
            crcCheckLength = 1 + KONNEKTING_NUMBER_OF_COMOBJECTS; // 1 bytes per association plus 1 byte for size information
            break;
        case CHECKSUM_ID_PARAMETER_TABLE:
            crcIndex = SYSTEMTABLE_CRC_PARAMETERTABLE;
            crcCheckStartIndex = KONNEKTING_MEMORYADDRESS_PARAMETERTABLE;
            int paramSizeTotal = 0;
            for(int i=0;i<_numberOfParams;i++) {
                paramSizeTotal += _paramSizeList[i];
            }
            crcCheckLength = paramSizeTotal; // 1 bytes per association plus 1 byte for size information
            break;
    }

    // read CRC32 from system table
    byte crc0 = memoryRead(crcIndex + 0);
    byte crc1 = memoryRead(crcIndex + 1);
    byte crc2 = memoryRead(crcIndex + 2);
    byte crc3 = memoryRead(crcIndex + 3);
    unsigned long crcValue = __DWORD(crc0, crc1, crc2, crc3);

    DEBUG_PRINTLN(F(" crc=0x%08X startIndex=0x%04x length=%d"), crcValue, crcCheckStartIndex, crcCheckLength);
    CRC32 crcMemory;
    crcMemory.reset();
    for (int i=crcCheckStartIndex; i<crcCheckStartIndex+crcCheckLength; i++) {
        uint8_t b = memoryRead(i);
        crcMemory.update(b);
    }
    unsigned long crcMemoryValue = crcMemory.finalize();

    if (crcMemoryValue != crcValue) {
        DEBUG_PRINTLN(F("crc check failed. expected=0x%08X is=0x%08X"), crcValue, crcMemoryValue);
        return false;
    } else {
        DEBUG_PRINTLN(F("crc checksum SUCCESS"));
        return true;
    }
}


void KonnektingDevice::handleMsgChecksumSet(byte msg[]) {
    byte crcId = msg[2];
    unsigned long crcValue = __DWORD(msg[3], msg[4], msg[5], msg[6]);
    DEBUG_PRINTLN(F("handleMsgChecksumSet crcId=0x%02x crc32=%lu/0x%08X"), crcId, crcValue, crcValue);


    // for startindex spec, see https://wiki.konnekting.de/index.php/KONNEKTING_Protocol_Specification_0x01#System_Table
    int crcIndex = -1;
    switch(crcId) {
        case CHECKSUM_ID_SYSTEM_TABLE:        
        crcIndex = SYSTEMTABLE_CRC_SYSTEMTABLE;
        break;
        case CHECKSUM_ID_ADDRESS_TABLE:
        crcIndex = SYSTEMTABLE_CRC_ADDRESSTABLE;
        break;
        case CHECKSUM_ID_ASSOCIATION_TABLE:
        crcIndex = SYSTEMTABLE_CRC_ASSOCIATIONTABLE;
        break;
        case CHECKSUM_ID_COMMOBJECT_TABLE:
        crcIndex = SYSTEMTABLE_CRC_COMMOBJECTTABLE;
        break;
        case CHECKSUM_ID_PARAMETER_TABLE:
        crcIndex = SYSTEMTABLE_CRC_PARAMETERTABLE;
        break;
    }

    // store CRC in system table
    memoryWrite(crcIndex + 0, msg[3]);
    memoryWrite(crcIndex + 1, msg[4]);
    memoryWrite(crcIndex + 2, msg[5]);
    memoryWrite(crcIndex + 3, msg[6]);

    if (!checkTableCRC(crcId)) {
        sendMsgAck(NACK, ERR_CODE_TABLE_CRC_FAILED);    
    } else
    sendMsgAck(ACK, ERR_CODE_OK);
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
                response[6] = KONNEKTING_SYSTEM_TYPE; // current set system type, see System.h for reference
                fillEmpty(response, 7);
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

void KonnektingDevice::handleMsgUnload(byte msg[]) {
    DEBUG_PRINTLN(F("handleMsgUnload"));
    byte newDeviceFlags = _deviceFlags;
    if (msg[2] == 0xFF) {
        DEBUG_PRINTLN(F(" factory reset"));
        // set all bits back to 1
        newDeviceFlags = 0xFF;

        // clearing all up to userspace
        for (int i=0;i<getMemoryUserSpaceStart(); i++) {
            memoryWrite(i, 0xFF);
        }

        // clear all spi flash data
        if (*_dataRemoveFunc != NULL) {            
            for (int typeId = 0x01; typeId<256; typeId++) {
                DEBUG_PRINT(F("\n  removing data for typeId=%i\n\t"), typeId);
                for (int dataId = 0; dataId<256; dataId+=1) {
                    bool res = _dataRemoveFunc(typeId, dataId);
                    DEBUG_PRINT(F("%s%s"), (res?".":"x"), (dataId%64==63?"\n\t":""));
                } 
            }
            DEBUG_PRINTLN(F(""));
        } else {
            DEBUG_PRINTLN(F(" nothing to remove, no fctptr available"));
        }

    } else {

        // set bits back to 1
        if (msg[3] == 0xFF) {
            DEBUG_PRINTLN(F(" IA"));
            newDeviceFlags |= DEVICEFLAG_IA_BIT;
            memoryWrite(SYSTEMTABLE_INDIVIDUALADDRESS + 0, 0xFF);
            memoryWrite(SYSTEMTABLE_INDIVIDUALADDRESS + 1, 0xFF);
        }
        if (msg[4] == 0xFF) {
            DEBUG_PRINTLN(F(" CO"));
            newDeviceFlags |= DEVICEFLAG_CO_BIT;
            for (int i=KONNEKTING_MEMORYADDRESS_ADDRESSTABLE;i<KONNEKTING_MEMORYADDRESS_PARAMETERTABLE; i++) {
                memoryWrite(i, 0xFF);
            }
        }
        if (msg[5] == 0xFF) {
            DEBUG_PRINTLN(F(" param"));
            newDeviceFlags |= DEVICEFLAG_PARAM_BIT;
            for (int i=KONNEKTING_MEMORYADDRESS_PARAMETERTABLE;i<getMemoryUserSpaceStart(); i++) {
                memoryWrite(i, 0xFF);
            }
        }
        if (msg[6] == 0xFF) {
            DEBUG_PRINT(F(" data"));
            newDeviceFlags |= DEVICEFLAG_DATA_BIT;
            // clear all spi flash data
            if (*_dataRemoveFunc != NULL) {            
                for (int typeId = 0x01; typeId<256; typeId++) {
                    DEBUG_PRINT(F("\n  removing data for typeId=%i\n\t"), typeId);
                    for (int dataId = 0; dataId<256; dataId+=1) {
                        bool res = _dataRemoveFunc(typeId, dataId);
                        DEBUG_PRINT(F("%s%s"), (res?".":"x"), (dataId%64==63?"\n\t":""));
                    } 
                }
                DEBUG_PRINTLN(F(""));
            } else {
                DEBUG_PRINTLN(F(" nothing to remove, no fctptr available"));
            }
        }

    }
    
    DEBUG_PRINTLN(F(" unloaded. new device flags: (bin)" BYTETOBINARYPATTERN), BYTETOBINARY(newDeviceFlags));
    sendMsgAck(ACK, ERR_CODE_OK);
    _deviceFlags = newDeviceFlags;
    memoryWrite(SYSTEMTABLE_DEVICE_FLAGS, _deviceFlags);
    DEBUG_PRINTLN(F("handleMsgUnload *done*"));
    reboot();
}

void KonnektingDevice::handleMsgRestart(byte msg[]) {
    DEBUG_PRINTLN(F("handleMsgRestart"));
    if (_individualAddress == __WORD(msg[2], msg[3])) {
        sendMsgAck(ACK, ERR_CODE_OK);
#ifdef DEBUG_PROTOCOL
        DEBUG_PRINTLN(F("matching IA: 0x%04X"), _individualAddress);
#endif
        // trigger restart
        reboot();
    } else {
#ifdef DEBUG_PROTOCOL
        DEBUG_PRINTLN(F("no matching IA: self=0x%04X got=0x%04X"), _individualAddress, __WORD(msg[2], msg[3]));
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

    uint8_t count = msg[2];
    uint16_t startAddr = __WORD(msg[3], msg[4]);
    DEBUG_PRINTLN(F("  count=%d startAddr=0x%04x"), count, startAddr);

    // It makes no sense to write anything if there is no data.
    // It also makes no sense to send no data.
    // But we never know .. so we check.
    if (count>0) {

        // write data to memory
        for (uint8_t i = 0; i < count; i++) {
            uint16_t addr = startAddr + i;
            byte data = msg[5 + i];
            memoryWrite(addr, data);
        }

        // reload all system table related r/w data, if required
        if (startAddr >= 16 && startAddr < 32) {
            // FIXME introduce extra method for this? Is this called anywhere else as well?
            DEBUG_PRINTLN(F(" reload system table data due to change"));
            byte hiAddr = memoryRead(SYSTEMTABLE_INDIVIDUALADDRESS + 0);
            byte loAddr = memoryRead(SYSTEMTABLE_INDIVIDUALADDRESS + 1);
            _individualAddress = __WORD(hiAddr, loAddr);        
        }

        // howto: clearing bits: https://stackoverflow.com/questions/47981/how-do-you-set-clear-and-toggle-a-single-bit

        if (isFactorySetting()) {
            _deviceFlags &= ~DEVICEFLAG_FACTORY_BIT;
            DEBUG_PRINTLN(F(" set  factory setting bit to 0 in device flags: (bin)" BYTETOBINARYPATTERN), BYTETOBINARY(_deviceFlags));
            memoryWrite(SYSTEMTABLE_DEVICE_FLAGS, _deviceFlags);
        }

        // check if IA has been touched AND IA bit is still on factory
        if ((startAddr == SYSTEMTABLE_INDIVIDUALADDRESS + 0 || startAddr == SYSTEMTABLE_INDIVIDUALADDRESS + 1)
        && ((_deviceFlags & DEVICEFLAG_IA_BIT) == DEVICEFLAG_IA_BIT) ) {
            _deviceFlags &= ~DEVICEFLAG_IA_BIT;
            DEBUG_PRINTLN(F(" set IA bit to 0 in device flags: (bin)" BYTETOBINARYPATTERN), BYTETOBINARY(_deviceFlags));
            memoryWrite(SYSTEMTABLE_DEVICE_FLAGS, _deviceFlags);
        }

        // check if COs have been touched (memoryaddress within Address, Assoc or CO table)) AND CO bit is still on factory
        if ((startAddr >= KONNEKTING_MEMORYADDRESS_ADDRESSTABLE && startAddr < KONNEKTING_MEMORYADDRESS_PARAMETERTABLE)
        && ((_deviceFlags & DEVICEFLAG_CO_BIT) == DEVICEFLAG_CO_BIT) ) {
            _deviceFlags &= ~DEVICEFLAG_CO_BIT;
            DEBUG_PRINTLN(F(" set CO bit to 0 in device flags: (bin)" BYTETOBINARYPATTERN), BYTETOBINARY(_deviceFlags));
            memoryWrite(SYSTEMTABLE_DEVICE_FLAGS, _deviceFlags);
        }

        // check if params have been touched AND params bit is still on factory
        if ((startAddr >= KONNEKTING_MEMORYADDRESS_PARAMETERTABLE)
        && ((_deviceFlags & DEVICEFLAG_PARAM_BIT) == DEVICEFLAG_PARAM_BIT) ) {
            _deviceFlags &= ~DEVICEFLAG_PARAM_BIT;
            DEBUG_PRINTLN(F(" set Params bit to 0 in device flags: (bin)" BYTETOBINARYPATTERN), BYTETOBINARY(_deviceFlags));
            memoryWrite(SYSTEMTABLE_DEVICE_FLAGS, _deviceFlags);
        }
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
    if (*_dataOpenWriteFunc != NULL) {
        _crc32.reset();

        byte type = msg[2];
        byte id = msg[3];
        unsigned long size = __DWORD(msg[4], msg[5], msg[6], msg[7]);

        DEBUG_PRINT(F(" using fctptr"));

        bool result = _dataOpenWriteFunc(type, id, size);
        if (result) {
            sendMsgAck(ACK, ERR_CODE_OK);
        } else {
            sendMsgAck(NACK, ERR_CODE_DATA_OPEN_WRITE_FAILED);
        }

    } else {
        DEBUG_PRINTLN(F("handleMsgDataOpenWrite: missing FCTPTR!"));
        sendMsgAck(NACK, ERR_CODE_NOT_SUPPORTED);
    }
}

void KonnektingDevice::handleMsgDataWrite(byte msg[]) {
    DEBUG_PRINTLN(F("handleMsgDataWrite"));
    if (*_dataWriteFunc != NULL) {
        DEBUG_PRINTLN(F(" using fctptr"));

        int count = msg[2];
        byte *data = (byte *)malloc(count * sizeof(byte));

        // using for, because of issues with memcpy
        for (int i = 0; i < count; i++) {
            data[i] = msg[3 + i];
            DEBUG_PRINTLN(F("  data[%i]=0x%02x"), i, data[i]);
        }
        DEBUG_PRINTLN(F(" call fctptr"));
        bool result = _dataWriteFunc(data, count);
        _crc32.update(data, count);
        free(data);

        if (result) {
            sendMsgAck(ACK, ERR_CODE_OK);
        } else {
            sendMsgAck(NACK, ERR_CODE_DATA_WRITE_FAILED);
        }

    } else {
        DEBUG_PRINTLN(F("handleMsgDataWrite: missing FCTPTR!"));
        sendMsgAck(NACK, ERR_CODE_NOT_SUPPORTED);
    }
}

void KonnektingDevice::handleMsgDataWriteFinish(byte msg[]) {
    DEBUG_PRINTLN(F("handleMsgDataWriteFinish"));
    if (*_dataCloseFunc != NULL) {
        unsigned long otherCrc32 = __DWORD(msg[2], msg[3], msg[4], msg[5]);
        unsigned long thisCrc32 = _crc32.finalize();

        DEBUG_PRINTLN(F(" using fctptr thiscrc32=%lu othercrc32=%lu"), thisCrc32, otherCrc32);
        if (thisCrc32 == otherCrc32) {
            bool result = _dataCloseFunc();
            if (result) {
                sendMsgAck(ACK, ERR_CODE_OK);
                return;
            } else {
                sendMsgAck(NACK, ERR_CODE_DATA_WRITE_FAILED);
                return;
            }

        } else {
            DEBUG_PRINTLN(F(" crc mismatch"));
            bool result = _dataCloseFunc();
            sendMsgAck(NACK, ERR_CODE_DATA_CRC_FAILED);
            return;
        }

    } else {
        DEBUG_PRINTLN(F("handleMsgDataWrite: missing FCTPTR!"));
        sendMsgAck(NACK, ERR_CODE_NOT_SUPPORTED);
    }
}

void KonnektingDevice::handleMsgDataRead(byte msg[]) {
    DEBUG_PRINTLN(F("handleMsgDataRead"));

    if (*_dataOpenReadFunc != NULL && *_dataReadFunc != NULL && *_dataCloseFunc != NULL) {
        byte type = msg[2];
        byte id = msg[3];
        _crc32.reset();
        DEBUG_PRINTLN(F(" using fctptr type=%i id=%i"), type, id);

        // open the file
        unsigned long size = _dataOpenReadFunc(type, id);
        if (size == -1) {
            sendMsgAck(NACK, ERR_CODE_DATA_OPEN_READ_FAILED);
            return;
        }
        DEBUG_PRINTLN(F(" opened file with size=%ld"), size);

        /*
         * prepare 1st answer with filesize
         */
        byte response1[14];
        response1[0] = PROTOCOLVERSION;
        response1[1] = MSGTYPE_DATA_READ_RESPONSE;
        response1[2] = type;
        response1[3] = id;
        response1[4] = ______BB(size);
        response1[5] = ____BB__(size);
        response1[6] = __BB____(size);
        response1[7] = BB______(size);
        fillEmpty(response1, 8);

        for (int i = 0; i < 14; i++) {
            DEBUG_PRINTLN(F(" response1[%i]=0x%02x"), i, response1[i]);
        }

        byte ackCnt = _ackCounter;
        Knx.write(PROGCOMOBJ_INDEX, response1);
        Knx.task();
        bool ackReceived = waitForAck(ackCnt, WAIT_FOR_ACK_TIMEOUT);
        if (!ackReceived) {
            _dataCloseFunc();
            sendMsgAck(NACK, ERR_CODE_TIMEOUT);
            return;
        }

        /*
         * Send data, block by block
         */
        // iterate over file in 11 byte steps, read data and send over bus
        int remainingBytes = size;
        while (remainingBytes > 0) {

            // reset ack counter, otherwise too many data blocks might overflow the counter
            _ackCounter=0;

            int toRead = min(11, remainingBytes);
            DEBUG_PRINTLN(F(" toRead=%i"), toRead);

            byte *data = (byte *)malloc(toRead * sizeof(byte));

            if (!_dataReadFunc(data, toRead)) {
                _dataCloseFunc();
                sendMsgAck(NACK, ERR_CODE_DATA_READ_FAILED);
                return;
            } else {
                for (int i = 0; i < toRead; i++) {
                    DEBUG_PRINTLN(F("  data[%i]=0x%02x"), i, data[i]);
                }
            }
            _crc32.update(data, toRead);
            DEBUG_PRINTLN(F(" read data OK"));

            remainingBytes -= toRead;

            byte readResponseN[14];
            readResponseN[0] = PROTOCOLVERSION;
            readResponseN[1] = MSGTYPE_DATA_READ_DATA;
            readResponseN[2] = toRead;
            for (int i = 0; i < toRead; i++) {
                readResponseN[3 + i] = data[i];
            }
            fillEmpty(readResponseN, 3 + toRead);
            free(data);

            for (int i = 0; i < 14; i++) {
                DEBUG_PRINTLN(F(" readResponseN[%i]=0x%02x"), i, readResponseN[i]);
            }

            ackCnt = _ackCounter;
            Knx.write(PROGCOMOBJ_INDEX, readResponseN);
            ackReceived = waitForAck(ackCnt, WAIT_FOR_ACK_TIMEOUT);
            if (!ackReceived) {
                _dataCloseFunc();
                sendMsgAck(NACK, ERR_CODE_TIMEOUT);
                return;
            }
        }

        // finally reset ack counter last time after al blocks have been transferred
        _ackCounter=0;

        // close file
        if (!_dataCloseFunc()) {
            sendMsgAck(NACK, ERR_CODE_DATA_READ_FAILED);
            return;
        }
        DEBUG_PRINTLN(F(" closed file"));

        /*
         * send last answer with crc32
         */
        unsigned long crc32Value = _crc32.finalize();
        DEBUG_PRINTLN(F(" finalize crc32=%ld"), crc32Value);
        byte response3[14];
        response3[0] = PROTOCOLVERSION;
        response3[1] = MSGTYPE_DATA_READ_RESPONSE;
        response3[2] = type;
        response3[3] = id;
        response3[4] = ______BB(size);
        response3[5] = ____BB__(size);
        response3[6] = __BB____(size);
        response3[7] = BB______(size);
        response3[8] = ______BB(crc32Value);
        response3[9] = ____BB__(crc32Value);
        response3[10] = __BB____(crc32Value);
        response3[11] = BB______(crc32Value);
        fillEmpty(response3, 12);

        for (int i = 0; i < 14; i++) {
            DEBUG_PRINTLN(F(" response3[%i]=0x%02x"), i, response3[i]);
        }

        ackCnt = _ackCounter;
        Knx.write(PROGCOMOBJ_INDEX, response3);
        ackReceived = waitForAck(ackCnt, WAIT_FOR_ACK_TIMEOUT);
        if (!ackReceived) {
            sendMsgAck(NACK, ERR_CODE_TIMEOUT);
            return;
        }

    } else {
        DEBUG_PRINTLN(F("handleMsgDataRead: missing FCTPTR!"));
        sendMsgAck(NACK, ERR_CODE_NOT_SUPPORTED);
    }
}

void KonnektingDevice::handleMsgDataRemove(byte msg[]) {

    DEBUG_PRINTLN(F("handleMsgDataRemove:"));

    if (*_dataRemoveFunc != NULL) {
        byte type = msg[2];
        byte id = msg[3];
        DEBUG_PRINTLN(F(" using fctptr type=%i id=%i"), type, id);
        bool success = _dataRemoveFunc(type, id);
        if (success) {
            sendMsgAck(ACK, ERR_CODE_OK);
        } else {
            sendMsgAck(NACK, ERR_CODE_NOT_SUPPORTED);
        }
    } else {
        DEBUG_PRINTLN(F("handleMsgDataRemove: missing FCTPTR!"));
        sendMsgAck(NACK, ERR_CODE_NOT_SUPPORTED);
    }

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
int KonnektingDevice::getMemoryUserSpaceStart() {
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

// SPI Flash file storage function pointer setters
void KonnektingDevice::setDataOpenWriteFunc(bool (*func)(byte, byte, unsigned long)) {
    _dataOpenWriteFunc = func;
}
void KonnektingDevice::setDataOpenReadFunc(unsigned long (*func)(byte, byte)) {
    _dataOpenReadFunc = func;
}
void KonnektingDevice::setDataWriteFunc(bool (*func)(byte *, int)) {
    _dataWriteFunc = func;
}
void KonnektingDevice::setDataReadFunc(bool (*func)(byte *, int)) {
    _dataReadFunc = func;
}
void KonnektingDevice::setDataRemoveFunc(bool (*func)(byte, byte)) {
    _dataRemoveFunc = func;
}
void KonnektingDevice::setDataCloseFunc(bool (*func)()) {
    _dataCloseFunc = func;
}
