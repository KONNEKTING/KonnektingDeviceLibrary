/*!
 * @file KonnektingDevice.h
 *
 * This is part of KONNEKTING Device Library for the Arduino platform.
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
 */

#ifndef KONNEKTING_h
#define KONNEKTING_h

#define KONNEKTING_DEVICE_LIBRARY_VERSION 10000
#define KONNEKTING_DEVICE_LIBRARY_SNAPSHOT
#define KONNEKTING_1_0_0_beta5

#include "System.h"

#include <Arduino.h>

#include "DebugUtil.h"
#include "KnxDevice.h"
#include "KnxDptConstants.h"
// for doing CRC32 checks in data read/write
#include <CRC32.h> // https://github.com/bakercp/CRC32

// AVR, ESP8266, ESP32 and STM32 uses EEPROM (SAMD21 not ...)
#if defined(__AVR__) || defined(ESP8266) || defined(ESP32) || defined(ARDUINO_ARCH_STM32)
#include <EEPROM.h>
#ifdef ARDUINO_ARCH_AVR
#include <avr/wdt.h>
#endif
#endif

#define EEPROM_DEVICE_FLAGS 2           ///< EEPROM index for device flags
#define EEPROM_INDIVIDUALADDRESS_HI 16  ///< EEPROM index for IA, high byte
#define EEPROM_INDIVIDUALADDRESS_LO 17  ///< EEPROM index for IA, low byte

#define KONNEKTING_VERSION 0x0000
#define PROTOCOLVERSION 1

#define ACK 0x00
#define NACK 0xFF
#define ERR_CODE_OK 0x00
#define ERR_CODE_NOT_SUPPORTED 0x01
#define ERR_CODE_DATA_OPEN_WRITE_FAILED 0x02
#define ERR_CODE_DATA_OPEN_READ_FAILED 0x03
#define ERR_CODE_DATA_WRITE_FAILED 0x04
#define ERR_CODE_DATA_READ_FAILED 0x05
#define ERR_CODE_DATA_CRC_FAILED 0x06
#define ERR_CODE_TIMEOUT 0x07
#define ERR_CODE_ILLEGAL_STATE 0x08

#define SYSTEM_TYPE_SIMPLE 0x00
#define SYSTEM_TYPE_DEFAULT 0x01
#define SYSTEM_TYPE_EXTENDED 0x02  // DRAFT!

#define MSG_LENGTH 14  ///< Message length in bytes

#define MSGTYPE_ACK 0x00                     ///< Message Type: ACK 0x00
#define MSGTYPE_PROPERTY_PAGE_READ 0x01      ///< Message Type: Property Page Read 0x01
#define MSGTYPE_PROPERTY_PAGE_RESPONSE 0x02  ///< Message Type: Property Page Response 0x02

#define MSGTYPE_UNLOAD 0x08                  ///< Message Type: Unload 0x08
#define MSGTYPE_RESTART 0x09                 ///< Message Type: Restart 0x09

#define MSGTYPE_PROGRAMMING_MODE_WRITE 0x0A     ///< Message Type: Programming Mode Write 0x0C
#define MSGTYPE_PROGRAMMING_MODE_READ 0x0B      ///< Message Type: Programming Mode Read 0x0A
#define MSGTYPE_PROGRAMMING_MODE_RESPONSE 0x0C  ///< Message Type: Programming Mode Response 0x0B

#define MSGTYPE_MEMORY_WRITE 0x1E     ///< Message Type: Memory Write 0x1E
#define MSGTYPE_MEMORY_READ 0x1F      ///< Message Type: Memory Read 0x1F
#define MSGTYPE_MEMORY_RESPONSE 0x20  ///< Message Type: Memory Response 0x20

#define MSGTYPE_DATA_WRITE_PREPARE 0x28  ///< Message Type: Data Write Prepare 0x28
#define MSGTYPE_DATA_WRITE 0x29    ///< Message Type: Data Write 0x29
#define MSGTYPE_DATA_WRITE_FINISH 0x2A   ///< Message Type: Data Write Finish 0x2A
#define MSGTYPE_DATA_READ 0x2B   ///< Message Type: Data Read 0x2B
#define MSGTYPE_DATA_READ_RESPONSE 0x2C   ///< Message Type: Data Read Response 0x2C
#define MSGTYPE_DATA_READ_DATA 0x2D   ///< Message Type: Data Read Data 0x2D
#define MSGTYPE_DATA_REMOVE 0x2E   ///< Message Type: Data Remove 0x2E

#define DATA_TYPE_ID_UPDATE 0x00 ///< Firmware update for KONNEKTING device
#define DATA_TYPE_ID_DATA 0x01 ///< Data, f.i. additional configuration, images, sounds, ...

#define DEVICEFLAG_FACTORY_BIT 0x80
#define DEVICEFLAG_IA_BIT 0x40
#define DEVICEFLAG_CO_BIT 0x20
#define DEVICEFLAG_PARAM_BIT 0x10
#define DEVICEFLAG_DATA_BIT 0x08

#define WAIT_FOR_ACK_TIMEOUT 5000

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

inline byte HI__(word w) { return (byte)((w >> 8) & 0xff); }
inline byte __LO(word w) { return (byte)((w >> 0) & 0xff); }

inline byte BB______(unsigned long dw) { return (byte)((dw >> 24) & 0xff); }
inline byte __BB____(unsigned long dw) { return (byte)((dw >> 16) & 0xff); }
inline byte ____BB__(unsigned long dw) { return (byte)((dw >> 8) & 0xff); }
inline byte ______BB(unsigned long dw) { return (byte)((dw >> 0) & 0xff); }

inline word __WORD(byte hi, byte lo) { return (word)((hi << 8) + (lo << 0)); }
inline unsigned long __DWORD(byte b0, byte b1, byte b2, byte b3) { return (unsigned long)((b0 << 24) + (b1 << 16) + (b2 << 8) + (b3 << 0)); }

// process intercepted knxEvents-calls with this method
extern void konnektingKnxEvents(byte index);

typedef struct AssociationTable {
    byte size;
    byte* gaId;
    byte* coId;
};

typedef struct AddressTable {
    byte size;
    word* address;
};

/**
 * see https://wiki.konnekting.de/index.php?title=KONNEKTING_Protocol_Specification_0x01#0x28_DataWritePrepare
 */
typedef struct DataWrite {
    byte count;
    byte data[11];
};


/**************************************************************************/
/*!
 *  @brief  Main class provides KONNEKTING Device API
 */
/**************************************************************************/
class KonnektingDevice {

    friend class KnxTpUart;
    //friend boolean KnxTpUart:IsAddressAssigned(word addr, ArrayList<byte> &indexList) const;

    static byte _paramSizeList[];
    static const int _numberOfParams;
    /**
     * see https://wiki.konnekting.de/index.php?title=KONNEKTING_Protocol_Specification_0x01#Device_Memory_Layout
     */
    static AssociationTable _associationTable;
    /**
     * see https://wiki.konnekting.de/index.php?title=KONNEKTING_Protocol_Specification_0x01#Device_Memory_Layout
     */
    static AddressTable _addressTable;
    /**
     * maximum number of associations of single group address, depends on the programming via suite and the assiciations a usedr set up
    */
    static byte _assocMaxTableEntries;

    byte (*_eepromReadFunc)(int);
    void (*_eepromWriteFunc)(int, byte);
    void (*_eepromUpdateFunc)(int, byte);
    void (*_eepromCommitFunc)(void);
    void (*_progIndicatorFunc)(bool);

    bool (*_dataOpenWriteFunc)(byte, byte, unsigned long);
    unsigned long (*_dataOpenReadFunc)(byte, byte);
    bool (*_dataWriteFunc)(byte*, int);
    bool (*_dataReadFunc)(byte*, int);
    bool (*_dataCloseFunc)(void);

    // Constructor, Destructor
    KonnektingDevice();  // private constructor (singleton design pattern)

    ~KonnektingDevice() {}                 // private destructor (singleton design pattern)
    KonnektingDevice(KonnektingDevice &);  // private copy constructor (singleton design pattern)

   public:
    static KonnektingDevice Konnekting;

    void setMemoryReadFunc(byte (*func)(int));
    void setMemoryWriteFunc(void (*func)(int, byte));
    void setMemoryUpdateFunc(void (*func)(int, byte));
    void setMemoryCommitFunc(void (*func)(void));

    void setDataOpenWriteFunc(bool (*func)(byte, byte, unsigned long));
    void setDataOpenReadFunc(unsigned long (*func)(byte, byte));
    void setDataWriteFunc(bool (*func)(byte*, int));
    void setDataReadFunc(bool (*func)(byte*, int));
    void setDataCloseFunc(bool (*func)());

    void init(HardwareSerial &serial, void (*progIndicatorFunc)(bool),
              word manufacturerID, byte deviceID, byte revisionID);

    void init(HardwareSerial &serial, int progButtonPin, int progLedPin,
              word manufacturerID, byte deviceID, byte revisionID);

    // needs to be public too, due to ISR handler mechanism :-(
    bool internalKnxEvents(byte index);

    // must be public to be accessible from KonnektingProgButtonPressed()
    void toggleProgState();

    byte getParamSize(int index);
    void getParamValue(int index, byte *value);

    uint8_t getUINT8Param(int index);
    int8_t getINT8Param(int index);

    uint16_t getUINT16Param(int index);
    int16_t getINT16Param(int index);

    uint32_t getUINT32Param(int index);
    int32_t getINT32Param(int index);

    String getSTRING11Param(int index);

    bool isActive();
    bool isFactorySetting();

    bool isProgState();

    bool isReadyForApplication();

    void setProgState(bool state);

    int getFreeEepromOffset();

   private:
    CRC32 _crc32;
    byte _ackCounter = 0;
    bool _rebootRequired = false;
    bool _initialized = false;
#ifdef REBOOT_BUTTON
    byte _progbtnCount = 0;
    long _lastProgbtn = 0;
#endif
    word _individualAddress;

    byte _deviceFlags;
    word _manufacturerID;
    byte _deviceID;
    byte _revisionID;

    int _progLED;
    int _progButton;  // (->interrupt)
    void setProgLed(bool state);

    bool _progState;

    KnxComObject createProgComObject();

    void internalInit(HardwareSerial &serial, word manufacturerID, byte deviceID, byte revisionID);
    int calcParamSkipBytes(int index);

    void reboot();

    // prog methods
    void sendMsgAck(byte ackType, byte errorCode);
    bool waitForAck(byte ackCountBefore, unsigned long timeout);

    void handleMsgAck(byte *msg);
    void handleMsgReadDeviceInfo(byte *msg);

    void handleMsgUnload(byte *msg);
    void handleMsgRestart(byte *msg);
    
    void handleMsgProgrammingModeWrite(byte *msg);
    void handleMsgProgrammingModeRead(byte *msg);
    void handleMsgPropertyPageRead(byte *msg);
    
    void handleMsgMemoryWrite(byte *msg);
    void handleMsgMemoryRead(byte *msg);

    void handleMsgDataWritePrepare(byte *msg);
    void handleMsgDataWrite(byte *msg);
    void handleMsgDataWriteFinish(byte *msg);
    void handleMsgDataRead(byte *msg);
    void handleMsgDataRemove(byte *msg);

    byte memoryRead(int index);
    void memoryWrite(int index, byte data);
    void memoryUpdate(int index, byte data);
    void memoryCommit();

    void fillEmpty(byte *msg, int startIndex);
};

// not part of Konnekting class
void KonnektingProgButtonPressed();

// Reference to the KnxDevice unique instance
extern KonnektingDevice &Konnekting;

#endif  // KONNEKTING_h
