/*
 *    This file is part of KONNEKTING Knx Device Library.
 * 
 *    It is derived from another GPLv3 licensed project:
 *      The Arduino Knx Bus Device library allows to turn Arduino into "self-made" KNX bus device.
 *      Copyright (C) 2014 2015 Franck MARINI (fm@liwan.fr)
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

// File : KnxTpUart.cpp
// Author : Franck Marini
// Modified: Alexander Christian <info(at)root1.de>
//           Eugen Burkowski <eugenius707(at)gmail.com>

// Description : Communication with TPUART
// Module dependencies : HardwareSerial, KnxTelegram, KnxComObject

#include "KnxTpUart.h"
#include "KonnektingDevice.h"

#ifndef ESP8266  //ESP8266 does't need pgmspace.h
#ifdef ESP32
#include <pgmspace.h>  //ESP32
#else
#include <avr/pgmspace.h>  //rest of the world
#endif
#endif

/*
 * !!!!! IMPORTANT !!!!!
 * if "#define DEBUG" is set, you must run your KONNEKTING Suite with "-Dde.root1.slicknx.konnekting.debug=true" 
 * A release-version of your development MUST NOT contain "#define DEBUG" ...
 */
//#define DEBUG

static inline word TimeDeltaWord(word now, word before) {
    return (word)(now - before);
}


// Constructor

//KnxTpUart::KnxTpUart(HardwareSerial& serial, word physicalAddr, type_KnxTpUartMode mode)
//: _serial(serial), _physicalAddr(physicalAddr), _mode(mode) {
KnxTpUart::KnxTpUart(HardwareSerial& serial, word physicalAddr, type_KnxTpUartMode mode)
    : _serial(serial), _physicalAddr(physicalAddr), _mode(mode) {
    _rx.state = RX_RESET;
    _tx.state = TX_RESET;
    _tx.sentTelegram = NULL;
    _tx.ackFctPtr = NULL;
    _tx.nbRemainingBytes = 0;
    _tx.txByteIndex = 0;
    _stateIndication = 0;
    _evtCallbackFct = NULL;
    _comObjectsList = NULL;
    _assignedComObjectsNb = 0;
    _stateIndication = 0;

    _addressedComObjects.size = Konnekting._assocMaxTableEntries;
    _addressedComObjects.list = (byte*) malloc(_addressedComObjects.size * sizeof(byte));
}

// Destructor

KnxTpUart::~KnxTpUart() {
//    if (_orderedIndexTable) free(_orderedIndexTable);
    // close the serial communication if opened
    if ((_rx.state > RX_RESET) || (_tx.state > TX_RESET)) {
        _serial.end();
    }
}

// Reset the Arduino UART port and the TPUART device
// Return KNX_TPUART_ERROR in case of TPUART Reset failure

byte KnxTpUart::reset(void) {
    word startTime, nowTime;
    byte attempts = 10;

    DEBUG_PRINTLN(F("Reset triggered!"));

    // HOT RESET case
    if ((_rx.state > RX_RESET) || (_tx.state > TX_RESET)) {
        DEBUG_PRINTLN(F("HOT RESET case"));
        // stop the serial communication before restarting it
        _serial.end();
        _rx.state = RX_RESET;
        _tx.state = TX_RESET;
    }
    // CONFIGURATION OF THE ARDUINO USART WITH CORRECT FRAME FORMAT (19200, 8 bits, parity even, 1 stop bit)
    _serial.begin(19200, SERIAL_8E1);
#ifdef ESP8266
    // TODO: why exactly swapping?? --> document here!
    _serial.swap();
#endif
    while (attempts--) {  // we send a RESET REQUEST and wait for the reset indication answer

        DEBUG_PRINTLN(F("Reset attempts: %d"), attempts);

        // the sequence is repeated every sec as long as we do not get the reset indication
        _serial.write(TPUART_RESET_REQ);  // send RESET REQUEST

        for (nowTime = startTime = (word)millis(); TimeDeltaWord(nowTime, startTime) < 1000 /* 1 sec */; nowTime = (word)millis()) {
            if (_serial.available() > 0) {
                DEBUG_PRINTLN("Data available: %d", _serial.available());
                byte data = _serial.read();
                if (data == TPUART_RESET_INDICATION) {
                    _rx.state = RX_INIT;
                    _tx.state = TX_INIT;
                    DEBUG_PRINTLN(F("Reset successful"));
                    return KNX_TPUART_OK;
                } else {
                    DEBUG_PRINTLN("data not useable: 0x%02x. Expected: 0x%02x", data, TPUART_RESET_INDICATION);
                }
            }
        }  // 1 sec ellapsed
    }      // while(attempts--)
    _serial.end();
    DEBUG_PRINTLN(F("Reset failed, no answer from TPUART device"));
    return KNX_TPUART_ERROR;
}

// Attach a list of com objects
// NB1 : only the objects with "communication" attribute are considered by the TPUART
// NB2 : In case of objects with identical address, the object with highest index only is considered
// return KNX_TPUART_ERROR_NOT_INIT_STATE (254) if the TPUART is not in Init state
// The function must be called prior to Init() execution

byte KnxTpUart::attachComObjectsList(KnxComObject comObjectsList[], byte listSize) {
#define IS_COM(index) (comObjectsList[index].getIndicator() & KNX_COM_OBJ_C_INDICATOR)
#define ADDR(index) (comObjectsList[index].getAddr())

    if ((_rx.state != RX_INIT) || (_tx.state != TX_INIT)) return KNX_TPUART_ERROR_NOT_INIT_STATE;

//    if (_orderedIndexTable) {  // a list is already attached, we detach it
//        free(_orderedIndexTable);
//        _orderedIndexTable = NULL;
        _comObjectsList = NULL;
        _assignedComObjectsNb = 0;
//    }
    if ((!comObjectsList) || (!listSize)) {
        DEBUG_PRINTLN(F("AttachComObjectsList : warning : empty object list!\n"));
        return KNX_TPUART_OK;
    }
    // Count all the com objects with communication indicator
    for (byte i = 0; i < listSize; i++)
        if (IS_COM(i)) _assignedComObjectsNb++;
    if (!_assignedComObjectsNb) {
        DEBUG_PRINTLN(F("AttachComObjectsList : warning : no object with com attribute in the list!"));
        return KNX_TPUART_OK;
    }

    _comObjectsList = comObjectsList;

    // Creation of the ordered index table
    // FIXME old unused code --> remove me
    // _orderedIndexTable = (byte*)malloc(_assignedComObjectsNb);
    // word minMin = 0x0000;    // minimum min value searched
    // word foundMin = 0xFFFF;  // min value found so far
    // for (byte i = 0; i < _assignedComObjectsNb; i++) {
    //     for (byte j = 0; j < listSize; j++) {
    //         if ((IS_COM(j)) && (ADDR(j) >= minMin) && (ADDR(j) <= foundMin)) {
    //             foundMin = ADDR(j);
    //             _orderedIndexTable[i] = j;
    //         }
    //     }
    //     minMin = foundMin + 1;
    //     foundMin = 0xFFFF;
    // }
    //    DEBUG_PRINTLN(F("AttachComObjectsList successful\n"));

    return KNX_TPUART_OK;
}

// Init
// returns ERROR (255) if the TP-UART is not in INIT state, else returns OK (0)
// Init must be called after every reset() execution
byte KnxTpUart::init(void) {
    byte tpuartCmd[3];

    if ((_rx.state != RX_INIT) || (_tx.state != TX_INIT)) return KNX_TPUART_ERROR_NOT_INIT_STATE;

    // BUS MONITORING MODE in case it is selected
    if (_mode == BUS_MONITOR) {
        _serial.write(TPUART_ACTIVATEBUSMON_REQ);  // Send bus monitoring activation request
        DEBUG_PRINTLN(F("Init : Monitoring mode started\n"));
    } else  // NORMAL mode by default
    {
        if (_comObjectsList == NULL) DEBUG_PRINTLN(F("Init : warning : empty object list!\n"));
        if (_evtCallbackFct == NULL) return KNX_TPUART_ERROR_NULL_EVT_CALLBACK_FCT;
        if (_tx.ackFctPtr == NULL) return KNX_TPUART_ERROR_NULL_ACK_CALLBACK_FCT;
        /*
        // Set Physical address. This allows to activate address evaluation by the TPUART
/*      we don't need it anymore. it fixes also bug with PA 1.1.1 on NCN5120
        tpuartCmd[0] = TPUART_SET_ADDR_REQ;
        tpuartCmd[1] = (byte) (_physicalAddr >> 8);
        tpuartCmd[2] = (byte) _physicalAddr;
        _serial.write(tpuartCmd, 3);

        // Call U_State.request-Service in order to have the field _stateIndication up-to-date
        _serial.write(TPUART_STATE_REQ);
*/
        _rx.state = RX_IDLE_WAITING_FOR_CTRL_FIELD;
        _tx.state = TX_IDLE;
        DEBUG_PRINTLN(F("Init : Normal mode started\n"));
    }
    return KNX_TPUART_OK;
}

// Send a KNX telegram
// returns ERROR (255) if TX is not available, or if the telegram is not valid, else returns OK (0)
// NB : the source address is forced to TPUART physical address value

byte KnxTpUart::sendTelegram(KnxTelegram& sentTelegram) {
    if (_tx.state != TX_IDLE) return KNX_TPUART_ERROR;  // TX not initialized or busy

    if (sentTelegram.GetSourceAddress() != _physicalAddr)  // Check that source addr equals TPUART physical addr
    {                                                      // if not, let's force source addr to the correct value
        sentTelegram.SetSourceAddress(_physicalAddr);
        sentTelegram.UpdateChecksum();
    }
    _tx.sentTelegram = &sentTelegram;
    _tx.nbRemainingBytes = sentTelegram.GetTelegramLength();
    _tx.txByteIndex = 0;  // Set index to 0
    _tx.state = TX_TELEGRAM_SENDING_ONGOING;
    return KNX_TPUART_OK;
}

/*
 * Reception task
 * 
 * This function shall be called periodically in order to allow a correct reception of the KNX bus data
 * Assuming the TPUART speed is configured to 19200 baud, a character (8 data + 1 start + 1 parity + 1 stop)
 * is transmitted in 0,58ms.
 * In order not to miss any End Of Packets (i.e. a gap from 2 to 2,5ms), the function shall be called at a 
 * max period of 0,5ms.
 * Typical calling period is 400 usec.
 * 
 * DO NOT PUT TOO MUCH DEBUG PRINT CODE HERE! Telegram receiving might break!
 */
void KnxTpUart::rxTask(void) {
    byte incomingByte;
    word nowTime;
    static byte readBytesNb;                               // Nb of read bytes during an KNX telegram reception
    static KnxTelegram telegram;                           // telegram being received
    //static type_AddressedComObjects addressedComObjIndex;  // index of the com object targeted by the received telegram
    static word lastByteRxTimeMicrosec;

    // === STEP 1 : Check EOP in case a Telegram is being received ===
    if (_rx.state >= RX_KNX_TELEGRAM_RECEPTION_STARTED) {  // a telegram reception is ongoing

        // word cast because a 65ms looping counter is long enough
        nowTime = (word)micros();

        if (TimeDeltaWord(nowTime, lastByteRxTimeMicrosec) > 2000 /* 2 ms */) {
            // EOP detected, the telegram reception is completed

            //DEBUG_PRINTLN(F("EOP REACHED"));
            switch (_rx.state) {
                case RX_KNX_TELEGRAM_RECEPTION_STARTED:  // we are not supposed to get EOP now, the telegram is incomplete
                    //DEBUG_PRINTLN(F("RX_KNX_TELEGRAM_RECEPTION_STARTED"));
                case RX_KNX_TELEGRAM_RECEPTION_LENGTH_INVALID:
                    DEBUG_PRINTLN(F("RX_KNX_TELEGRAM_RECEPTION_LENGTH_INVALID"));
                    _evtCallbackFct(TPUART_EVENT_KNX_TELEGRAM_RECEPTION_ERROR);  // Notify telegram reception error
                    break;

                case RX_KNX_TELEGRAM_RECEPTION_ADDRESSED:
                    //DEBUG_PRINTLN(F("RX_KNX_TELEGRAM_RECEPTION_ADDRESSED"));
                    if (telegram.IsChecksumCorrect()) {
                        // checksum correct, let's update the _rx struct with the received telegram and correct index
                        //DEBUG_PRINTLN(F("RX_KNX_TELEGRAM_RECEPTION_ADDRESSED 1"));
                        telegram.Copy(_rx.receivedTelegram);
                        //DEBUG_PRINTLN(F("RX_KNX_TELEGRAM_RECEPTION_ADDRESSED 2"));
                        
                        //_rx.addressedComObjects = _addressedComObjects;

                        //DEBUG_PRINTLN(F("RX_KNX_TELEGRAM_RECEPTION_ADDRESSED 3"));
                        // Notify the new received telegram
                        _evtCallbackFct(TPUART_EVENT_RECEIVED_KNX_TELEGRAM);
                    } else {
                        // checksum incorrect, notify error
                        DEBUG_PRINTLN(F("checksum incorrect."));
                        _evtCallbackFct(TPUART_EVENT_KNX_TELEGRAM_RECEPTION_ERROR);  // Notify telegram reception error
                    }
                    break;

                case RX_KNX_TELEGRAM_RECEPTION_NOT_ADDRESSED:
                    break;  // nothing to do!

                default:
                    break;
            }  // end of switch

            // we move state back to RX IDLE in any case
            _rx.state = RX_IDLE_WAITING_FOR_CTRL_FIELD;
        }  // end EOP detected
    }

    // === STEP 2 : Get New RX Data ===
    if (_serial.available() > 0) {
        incomingByte = (byte)(_serial.read());
        lastByteRxTimeMicrosec = (word)micros();
        // DEBUG_PRINTLN(F("RX:  incomingByte=0x%02x, readBytesNb=%d"), incomingByte, readBytesNb);

        switch (_rx.state) {
            case RX_IDLE_WAITING_FOR_CTRL_FIELD:
                // DEBUG_PRINTLN(F("RX_IDLE_WAITING_FOR_CTRL_FIELD incomingByte=0x%02x, readBytesNb=%d"), incomingByte, readBytesNb);

                // CASE OF KNX MESSAGE
                if ((incomingByte & KNX_CONTROL_FIELD_PATTERN_MASK) == KNX_CONTROL_FIELD_VALID_PATTERN) {
                    _rx.state = RX_KNX_TELEGRAM_RECEPTION_STARTED;
                    readBytesNb = 1;
                    telegram.WriteRawByte(incomingByte, 0);
                }
                // CASE OF TPUART_DATA_CONFIRM_SUCCESS NOTIFICATION
                else if (incomingByte == TPUART_DATA_CONFIRM_SUCCESS) {
                    if (_tx.state == TX_WAITING_ACK) {
                        _tx.ackFctPtr(ACK_RESPONSE);
                        _tx.state = TX_IDLE;
                    } else {
                        // DEBUG_PRINTLN(F("Rx: unexpected TPUART_DATA_CONFIRM_SUCCESS received!"));
                    }
                }
                // CASE OF TPUART_RESET NOTIFICATION
                else if (incomingByte == TPUART_RESET_INDICATION) {
                    if ((_tx.state == TX_TELEGRAM_SENDING_ONGOING) || (_tx.state == TX_WAITING_ACK)) {  // response to the TP UART transmission
                        _tx.ackFctPtr(TPUART_RESET_RESPONSE);
                    }
                    _tx.state = TX_STOPPED;
                    _rx.state = RX_STOPPED;
                    // Notify RESET
                    _evtCallbackFct(TPUART_EVENT_RESET);
                    // DEBUG_PRINTLN(F("Rx: Reset Indication Received"));
                    return;
                }
                // CASE OF STATE_INDICATION RESPONSE
                else if ((incomingByte & TPUART_STATE_INDICATION_MASK) == TPUART_STATE_INDICATION) {
                    _evtCallbackFct(TPUART_EVENT_STATE_INDICATION);  // Notify STATE INDICATION
                    _stateIndication = incomingByte;
                    // DEBUG_PRINTLN(F("Rx: State Indication Received"));
                }
                // CASE OF TPUART_DATA_CONFIRM_FAILED NOTIFICATION
                else if (incomingByte == TPUART_DATA_CONFIRM_FAILED) {
                    // NACK following Telegram transmission
                    if (_tx.state == TX_WAITING_ACK) {
                        _tx.ackFctPtr(NACK_RESPONSE);
                        _tx.state = TX_IDLE;
                    } else
                        DEBUG_PRINTLN(F("Rx: unexpected TPUART_DATA_CONFIRM_FAILED received!"));
                }
                // UNKNOWN CONTROL FIELD RECEIVED
                else if (incomingByte) {
                    // DEBUG_PRINTLN(F("Rx: Unknown Control Field received: byte=0x%02x"), incomingByte);
                }
                // else ignore "0" value sent on Reset by TPUART prior to TPUART_RESET_INDICATION
                break;

            case RX_KNX_TELEGRAM_RECEPTION_STARTED:
                // DEBUG_PRINTLN(F("RX_KNX_TELEGRAM_RECEPTION_STARTED incomingByte=0x%02x, readBytesNb=%d"), incomingByte, readBytesNb);
                telegram.WriteRawByte(incomingByte, readBytesNb);
                readBytesNb++;

                if (readBytesNb == 3) {
                    // We have just received the source address

                    // we check whether the received KNX telegram is coming from us (i.e. telegram is sent by the TPUART itself)
                    if (telegram.GetSourceAddress() == _physicalAddr) {
                        // the message is coming from us, we consider it as not addressed and we don't send any ACK service
                        // DEBUG_PRINTLN(F("message from us, skip."));
                        _rx.state = RX_KNX_TELEGRAM_RECEPTION_NOT_ADDRESSED;
                    }
                } else if (readBytesNb == 6)
                // We have just read the routing field containing the address type and the payload length
                {
                    // We check if the message is addressed to us in order to send the appropriate acknowledge
                    if (isAddressAssigned(telegram.GetTargetAddress() /*, addressedComObjIndex*/)) {  // Message addressed to us

                        // DEBUG_PRINTLN(F("assigned to us: ga=0x%04x index=%d"), telegram.GetTargetAddress(), addressedComObjectIndex);

                        _rx.state = RX_KNX_TELEGRAM_RECEPTION_ADDRESSED;
                        // sent the correct ACK service now
                        // the ACK info must be sent latest 1,7 ms after receiving the address type octet of an addressed frame
                        _serial.write(TPUART_RX_ACK_SERVICE_ADDRESSED);

                        // dirty workaround for sending ACk just before reset?
                        // _serial.flush();

                    } else {  // Message NOT addressed to us
                        _rx.state = RX_KNX_TELEGRAM_RECEPTION_NOT_ADDRESSED;
                        //sent the correct ACK service now
                        // the ACK info must be sent latest 1,7 ms after receiving the address type octet of an addressed frame
                        _serial.write(TPUART_RX_ACK_SERVICE_NOT_ADDRESSED);

                        // dirty workaround for sending ACk just before reset?
                        // _serial.flush();
                    }
                }
                break;

            case RX_KNX_TELEGRAM_RECEPTION_ADDRESSED:

                // DEBUG_PRINTLN(F("RX_KNX_TELEGRAM_RECEPTION_ADDRESSED"));

                if (readBytesNb == KNX_TELEGRAM_MAX_SIZE) {
                    _rx.state = RX_KNX_TELEGRAM_RECEPTION_LENGTH_INVALID;
                } else {
                    telegram.WriteRawByte(incomingByte, readBytesNb);
                    readBytesNb++;
                }
                break;

            // if the message is too long, nothing to do except waiting for EOP
            case RX_KNX_TELEGRAM_RECEPTION_LENGTH_INVALID:
                break;

            // if the message is not addressed, nothing to do except waiting for EOP
            case RX_KNX_TELEGRAM_RECEPTION_NOT_ADDRESSED:
                break;

            default:
                break;
        }  // end of: switch (_rx.state)
    }      // end of: if (_serial.available() > 0)
}

/**
 * Transmission task
 * This function shall be called periodically in order to allow a correct transmission of the KNX bus data
 * Assuming the TP-Uart speed is configured to 19200 baud, a character (8 data + 1 start + 1 parity + 1 stop)
 * is transmitted in 0,58ms.
 * Sending one byte of a telegram consists in transmitting 2 characters (1,16ms)
 * Let's wait around 800us between each telegram piece sending so that the 64byte TX buffer remains almost empty.
 * Typical calling period is 800 usec.
 */
void KnxTpUart::txTask(void) {
    word nowTime;
    byte txByte[2];
    static word sentMessageTimeMillisec;

    // STEP 1 : Manage Message Acknowledge timeout
    switch (_tx.state) {
        case TX_WAITING_ACK:
            // A transmission ACK is awaited, increment Acknowledge timeout
            nowTime = (word)millis();                                                  // word is enough to count up to 500
            if (TimeDeltaWord(nowTime, sentMessageTimeMillisec) > 500 /* 500 ms */) {  // The no-answer timeout value is defined as follows :
                // - The emission duration for a single max sized telegram is 40ms
                // - The telegram emission might be repeated 3 times (120ms)
                // - The telegram emission might be delayed by another message transmission ongoing
                // - The telegram emission might be delayed by the simultaneous transmission of higher prio messages
                // Let's take around 3 times the max emission duration (160ms) as arbitrary value
                _tx.ackFctPtr(NO_ANSWER_TIMEOUT);  // Send a No Answer TIMEOUT
                _tx.state = TX_IDLE;
            }
            break;

        case TX_TELEGRAM_SENDING_ONGOING:
            // STEP 2 : send message if any to send
            // In case a telegram reception has just started, and the ACK has not been sent yet,
            // we block the transmission (for around 3,3ms) till the ACK is sent
            // In that way, the TX buffer will remain empty and the ACK will be sent immediately
            if (_rx.state != RX_KNX_TELEGRAM_RECEPTION_STARTED) {
                if (_tx.nbRemainingBytes == 1) {  // We are sending the last byte, i.e checksum
                    txByte[0] = TPUART_DATA_END_REQ + _tx.txByteIndex;
                    txByte[1] = _tx.sentTelegram->ReadRawByte(_tx.txByteIndex);
                    // DEBUG_PRINTLN(F("data1[%d]=0x%02x"),_tx.txByteIndex, txByte[1]);
                    _serial.write(txByte, 2);  // write the UART control field and the data byte

                    // Message sending completed
                    sentMessageTimeMillisec = (word)millis();  // memorize sending time in order to manage ACK timeout
                    _tx.state = TX_WAITING_ACK;
                } else {
                    txByte[0] = TPUART_DATA_START_CONTINUE_REQ + _tx.txByteIndex;
                    txByte[1] = _tx.sentTelegram->ReadRawByte(_tx.txByteIndex);
                    // DEBUG_PRINTLN(F("data2[%d]=0x%02x"),_tx.txByteIndex, txByte[1]);
                    _serial.write(txByte, 2);  // write the UART control field and the data byte
                    _tx.txByteIndex++;
                    _tx.nbRemainingBytes--;
                }
            }
            break;

        default:
            break;
    }  // switch
}

/** Get bus monitoring data (BUS MONITORING mode)
 * The function returns true if a new data has been retrieved (data pointer in argument), else false
 * It shall be called periodically (max period of 0,5ms) in order to allow correct data reception
 * Typical calling period is 400 usec.
 */
boolean KnxTpUart::getMonitoringData(type_MonitorData& data) {
    word nowTime;
    static type_MonitorData currentData = {true, 0};
    static word lastByteRxTimeMicrosec;

    // STEP 1 : Check EOP
    if (!(currentData.isEOP))  // check that we have not already detected an EOP
    {
        // word cast because a 65ms counter is enough
        nowTime = (word)micros();
        if (TimeDeltaWord(nowTime, lastByteRxTimeMicrosec) > 2000 /* 2 ms */) {  // EOP detected
            currentData.isEOP = true;
            currentData.dataByte = 0;
            data = currentData;
            return true;
        }
    }
    // STEP 2 : Get New RX Data
    if (_serial.available() > 0) {
        currentData.dataByte = (byte)(_serial.read());
        currentData.isEOP = false;
        data = currentData;
        lastByteRxTimeMicrosec = (word)micros();
        return true;
    }
    return false;  // No data received
}

/**
 * Check if the target address is an assigned com object one
 * if yes, then update index parameter with the index (in the list) 
 * of the targeted com object and return true else return false
 * 
 * WARNING: DO NOT ADD DEBUG CODE HERE, AS IT WILL BREAK TELEGRAM RECEIVIBG (Timing issues)
 * 
 * @param addr the GA to check for assignment
 * @param index the index variable will be filled with the index matching this GA
 * @return true if assigned & active, false if not
 */
boolean KnxTpUart::isAddressAssigned(word addr) {
    long start = micros();
    DEBUG_PRINTLN(F("IsAddressAssigned: Searching for 0x%04x"), addr);

    // clean up old findings
    _addressedComObjects.items = 0;

    // in case of empty list, we return immediately
    if (!_assignedComObjectsNb) return false;

    // in case of Programming Group Address, we also return immediately
    if (addr == 0x7fff) {     // 0x7fff = 15/7/255
        _addressedComObjects.list[0] = 255;  // set ProgComObj
        _addressedComObjects.items = 1;
        DEBUG_PRINTLN(F("  0x%04x == 0x7fff --> progComObj. [%dus]"), addr, (micros() - start));
        return true;
    }

    // search for adressid in address table
    AddressTable addressTable = Konnekting._addressTable;
    byte l = 0;                                    // left end of array
    byte r = addressTable.size - 1;  // right end of array
    boolean addressFound = false;                  // true if address found in address table
    byte addressId;                                // address-id of groupaddress in address table

    // do binary search in address table
    while (l <= r) {
        byte m = l + (r - l) / 2;  // mid

        // Check if 'addr' is present at mid
        if (addressTable.address[m] == addr) {
            addressFound = true;
            addressId = m;
            break;
        }

        // If 'addr' greater, ignore left half by setting l to next right from mid
        if (addr > addressTable.address[m]) {
            l = m + 1;
        } else {
            // 'addr' is smaller, so ignore right half by setting r to next left from mid
            r = m - 1;
        }
    }
    if (!addressFound) {
        DEBUG_PRINTLN(F("  0x%04x not in addr table. [%dus]"), addr, (micros() - start));
        return false;  // address unknown
    } else {
        DEBUG_PRINTLN(F("  0x%04x has id #%d"), addr, addressId);
    }

    // get the assoc table from konnkting. We handle them as two arrays
    // to make search for GA easier, as we can search in an
    // AddressId-Array-Only for the matching index and can use the ComObj
    // array to get the ComObjID based on the found index
    AssociationTable associationTable = Konnekting._associationTable;

    // inspired by https://www.geeksforgeeks.org/binary-search/
    l = 0;                          // left end of array
    r = associationTable.size - 1;  // right end of array

    // some helper variables for doing iterative binary search
    int addressIdOnIndex = 0;
    boolean addressIdFound = false;

    // do binary search in assoc table
    while (l <= r) {
        byte m = l + (r - l) / 2;  // mid

        // Check if 'addressId' is present at mid
        if (associationTable.gaId[m] == addressId) {
            addressIdFound = true;
            addressIdOnIndex = m;
            break;
        }

        // If 'addressId' greater, ignore left half by setting l to next right from mid
        if (addressId > associationTable.gaId[m]) {
            l = m + 1;
        } else {
            // 'addressId' is smaller, so ignore right half by setting r to next left from mid
            r = m - 1;
        }
    }

    byte listIndex = 0;

    if (addressIdFound) {
        DEBUG_PRINTLN(F("  addrID=%d found on assoc table index %d"), addressId, addressIdOnIndex);

        // add first occurence
        _addressedComObjects.list[listIndex] = associationTable.coId[addressIdOnIndex];
        _addressedComObjects.items++;
        listIndex++;

        // search backwards until other address gets visible
        byte i = addressIdOnIndex - 1;

        if (i > 0) {
            while (associationTable.gaId[i] == addressId) {
                DEBUG_PRINTLN(F("    found prev index: %d"), i);

                _addressedComObjects.list[listIndex] = associationTable.coId[i];
                _addressedComObjects.items++;
                listIndex++;

                i--;
            }
        }

        // search forward until other address gets visible
        i = addressIdOnIndex + 1;
        if (i < associationTable.size - 1) {
            while (associationTable.gaId[i] == addressId) {
                DEBUG_PRINTLN(F("    found next index: %d"), i);
                _addressedComObjects.list[listIndex] = associationTable.coId[i];
                _addressedComObjects.items++;
                listIndex++;

                i++;
            }
        }
    }

    boolean foundSomeIndex = _addressedComObjects.items > 0;

    if (!foundSomeIndex) {
        DEBUG_PRINTLN(F("IsAddressAssigned: found nothing, skipping this GA"), addr);
    } else {
        DEBUG_PRINTLN(F("IsAddressAssigned: found %d matching ComObjs"), _addressedComObjects.items);
    }
    DEBUG_PRINTLN(F("IsAddressAssigned: [%dus]"), (micros() - start));
    return foundSomeIndex;
}
//EOF
