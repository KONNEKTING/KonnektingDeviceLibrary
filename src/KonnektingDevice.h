/*!
 * @file KonnektingDevice.h
 *
 * This is part of KONNEKTING Device Library for the Arduino platform.
 *
 *    Copyright (C) 2016 Alexander Christian <info(at)root1.de>. All rights
 * reserved. This file is part of KONNEKTING Knx Device Library.
 *
 *    The KONNEKTING Knx Device Library is free software: you can redistribute
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
#include <DebugUtil.h>
#include <KnxDevice.h>
#include <KnxDptConstants.h>

// AVR, ESP8266 and STM32 uses EEPROM (SAMD21 not ...)
#if defined(__AVR__) || defined(ESP8266) || defined(ESP32) ||                  \
    defined(ARDUINO_ARCH_STM32)
#include <EEPROM.h>
#ifdef ARDUINO_ARCH_AVR
#include <avr/wdt.h>
#endif
#endif

#define EEPROM_DEVICE_FLAGS 2         ///< EEPROM index for device flags
#define EEPROM_INDIVIDUALADDRESS_HI 16  ///< EEPROM index for IA, high byte
#define EEPROM_INDIVIDUALADDRESS_LO 17  ///< EEPROM index for IA, low byte

#define KONNEKTING_VERSION 0x0000
#define PROTOCOLVERSION 1

#define ACK 0x00
#define NACK 0xFF
#define ERR_CODE_OK 0x00

#define SYSTEM_TYPE_SIMPLE 0x00
#define SYSTEM_TYPE_DEFAULT 0x01
#define SYSTEM_TYPE_EXTENDED 0x02 // DRAFT!

#define MSG_LENGTH 14 ///< Message length in bytes

#define MSGTYPE_ACK 0x00 ///< Message Type: ACK 0x00
#define MSGTYPE_PROPERTY_PAGE_READ                                             \
  0x01 ///< Message Type: Property Page Read 0x01
#define MSGTYPE_PROPERTY_PAGE_RESPONSE                                         \
  0x02                       ///< Message Type: Property Page Response 0x02
#define MSGTYPE_RESTART 0x09 ///< Message Type: Restart 0x09

#define MSGTYPE_PROGRAMMING_MODE_WRITE 0x0A ///< Message Type: Programming Mode Write 0x0C
#define MSGTYPE_PROGRAMMING_MODE_READ                                          \
  0x0B ///< Message Type: Programming Mode Read 0x0A
#define MSGTYPE_PROGRAMMING_MODE_RESPONSE                                      \
  0x0C ///< Message Type: Programming Mode Response 0x0B

#define MSGTYPE_MEMORY_WRITE 0x1E    ///< Message Type: Memory Write 0x1E
#define MSGTYPE_MEMORY_READ 0x1F     ///< Message Type: Memory Read 0x1F
#define MSGTYPE_MEMORY_RESPONSE 0x20 ///< Message Type: Memory Response 0x20

#define MSGTYPE_DATA_PREPARE 0x28 ///< Message Type: Data Prepare 0x28
#define MSGTYPE_DATA_WRITE 0x29   ///< Message Type: Data Write 0x29
#define MSGTYPE_DATA_FINISH 0x2A  ///< Message Type: Data Finish 0x2A
#define MSGTYPE_DATA_REMOVE 0x2B  ///< Message Type: Data Finish 0x2B

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

inline word __WORD(byte hi, byte lo) { return (word)((hi << 8) + (lo << 0)); }

// process intercepted knxEvents-calls with this method
extern void konnektingKnxEvents(byte index);

/**************************************************************************/
/*!
 *  @brief  Main class provides KONNEKTING Device API
 */

/**************************************************************************/
class KonnektingDevice {
  static byte _paramSizeList[];
  static const int _numberOfParams;

  byte (*_eepromReadFunc)(int);
  void (*_eepromWriteFunc)(int, byte);
  void (*_eepromUpdateFunc)(int, byte);
  void (*_eepromCommitFunc)(void);
  void (*_progIndicatorFunc)(bool);

  // Constructor, Destructor
  KonnektingDevice(); // private constructor (singleton design pattern)

  ~KonnektingDevice() {} // private destructor (singleton design pattern)
  KonnektingDevice(KonnektingDevice &); // private copy constructor (singleton
                                        // design pattern)

public:
  static KonnektingDevice Konnekting;

  void setMemoryReadFunc(byte (*func)(int));
  void setMemoryWriteFunc(void (*func)(int, byte));
  void setMemoryUpdateFunc(void (*func)(int, byte));
  void setMemoryCommitFunc(void (*func)(void));

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
  int _progButton; // (->interrupt)
  void setProgLed(bool state);

  bool _progState;

  KnxComObject createProgComObject();

  void internalInit(HardwareSerial &serial, word manufacturerID, byte deviceID,
                    byte revisionID);
  int calcParamSkipBytes(int index);

  void reboot();

  // prog methods
  void sendAck(byte ackType, byte errorCode);
  void handleMsgReadDeviceInfo(byte *msg);
  void handleMsgRestart(byte *msg);
  void handleMsgProgrammingModeWrite(byte *msg);
  void handleMsgProgrammingModeRead(byte *msg);
  void handleMsgPropertyPageRead(byte *msg);
  void handleMsgMemoryWrite(byte *msg);
  void handleMsgMemoryRead(byte *msg);

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

#endif // KONNEKTING_h
