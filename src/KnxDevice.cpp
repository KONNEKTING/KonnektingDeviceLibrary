/*!
 * @file KnxDevice.cpp
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

#include "KnxDevice.h"
#include "KonnektingDevice.h"

static inline word TimeDeltaWord(word now, word before) {
    return (word)(now - before);
}

// KnxDevice unique instance creation
KnxDevice KnxDevice::Knx;
KnxDevice& Knx = KnxDevice::Knx;

// Constructor

KnxDevice::KnxDevice() {
    _state = INIT;
    _tpuart = NULL;
    _txActionList = RingBuff<TxAction, ACTIONS_QUEUE_SIZE>();
    _initCompleted = false;
    _initIndex = 0;
    _rxTelegram = NULL;

    _progComObj.setAddr(G_ADDR(15, 7, 255));
}

int KnxDevice::getNumberOfComObjects() {
    return _numberOfComObjects;
}

// Start the KNX Device
// return KNX_DEVICE_ERROR (255) if begin() failed
// else return KNX_DEVICE_OK

KnxDeviceStatus KnxDevice::begin(HardwareSerial& serial, word physicalAddr) {
    delete _tpuart;  // always safe to delete null ptr
    _tpuart = new KnxTpUart(serial, physicalAddr, NORMAL);
    _rxTelegram = &_tpuart->getReceivedTelegram();
    //delay(10000); // Workaround for init issue with bus-powered arduino
    // the issue is reproduced on one (faulty?) TPUART device only, so remove it for the moment.
    if (_tpuart->reset() != KNX_TPUART_OK) {
        delete (_tpuart);
        _tpuart = NULL;
        _rxTelegram = NULL;
        DEBUG_PRINTLN(F("Init Error!"));
        return KNX_DEVICE_INIT_ERROR;
    }
    _tpuart->attachComObjectsList(_comObjectsList, _numberOfComObjects);
    _tpuart->setEvtCallback(&KnxDevice::GetTpUartEvents);
    _tpuart->setAckCallback(&KnxDevice::TxTelegramAck);
    _tpuart->init();
    _state = IDLE;
    DEBUG_PRINTLN(F("Init successful"));
    _lastInitTimeMillis = millis();
    _lastRXTimeMicros = micros();
    _lastTXTimeMicros = _lastRXTimeMicros;
#if defined(KNXDEVICE_DEBUG_INFO)
    _nbOfInits = 0;
#endif
    return KNX_DEVICE_OK;
}

// Stop the KNX Device
void KnxDevice::end() {
    TxAction action;

    _state = INIT;
    while (_txActionList.pop(action))
        ;  // empty ring buffer
    _initCompleted = false;
    _initIndex = 0;
    _rxTelegram = NULL;
    delete (_tpuart);
    _tpuart = NULL;
}

/** 
 * KNX device execution task
 * This function call shall be placed in the "loop()" Arduino function
 */
void KnxDevice::task(void) {
    TxAction action;
    word nowTimeMillis, nowTimeMicros;

    // STEP 1 : Initialize Com Objects having Init Read attribute
    if (!_initCompleted) {
        nowTimeMillis = millis();
        // To avoid KNX bus overloading, we wait for 500 ms between each Init read request
        if (TimeDeltaWord(nowTimeMillis, _lastInitTimeMillis) > 500) {
            while (
                (_initIndex < _numberOfComObjects) &&
                (_comObjectsList[_initIndex].getValidity() || !_comObjectsList[_initIndex].isActive()) /* either valid (=init done or not required) or not-active required to jump to next index */
                ) _initIndex++;

            if (_initIndex == _numberOfComObjects) {
                _initCompleted = true;  // All the Com Object initialization have been performed
            } else {                    // Com Object to be initialised has been found
                // Add a READ request in the TX action list
                action.command = KNX_READ_REQUEST;
                action.index = _initIndex;
                _txActionList.append(action);
                _lastInitTimeMillis = millis();  // Update the timer
            }
        }
    }

    // STEP 2 : Get new received KNX messages from the TPUART
    // The TPUART RX task is executed every 400 us
    nowTimeMicros = micros();
    if (TimeDeltaWord(nowTimeMicros, _lastRXTimeMicros) > 400) {
        _lastRXTimeMicros = nowTimeMicros;
        _tpuart->rxTask();

        // TODO: check for rx_state in tpuart and call rxtask repeatedly until telegram is received?!
    }

    // STEP 3 : Send KNX messages following TX actions
    if (_state == IDLE) {
        if (_txActionList.pop(action)) {  // Data to be transmitted

            //DEBUG_PRINTLN(F("Data to be transmitted index=%d"), action.index);
            KnxComObject* comObj = (action.index == 255 ? &_progComObj : &_comObjectsList[action.index]);

            // hier bereits kaputt!
            /*
                -> action.valuePtr[0]=0x7d // 0111 1101
                -> action.valuePtr[1]=0x03 // 0000 0011
             * statt
                -> action.valuePtr[0]=0x00 // 0000 0000
                -> action.valuePtr[1]=0x16 // 0001 0110
             */
            //for (byte i = 0; i < comObj.GetLength() - 1; i++) {
            //    DEBUG_PRINTLN(F("-> action.valuePtr[%d]=0x%02x"), i, action.valuePtr[i]);
            //}

            switch (action.command) {
                case KNX_READ_REQUEST:  // a read operation of a Com Object on the KNX network is required
                    //_objectsList[action.index].CopyToTelegram(_txTelegram, KNX_COMMAND_VALUE_READ);
                    comObj->copyAttributes(_txTelegram);
                    _txTelegram.ClearLongPayload();
                    _txTelegram.ClearFirstPayloadByte();  // Is it required to have a clean payload ??
                    _txTelegram.SetCommand(KNX_COMMAND_VALUE_READ);
                    _txTelegram.UpdateChecksum();
                    _tpuart->sendTelegram(_txTelegram);
                    _state = TX_ONGOING;
                    break;

                case KNX_RESPONSE_REQUEST:  // a response operation of a Com Object on the KNX network is required
                    comObj->copyAttributes(_txTelegram);
                    comObj->copyValue(_txTelegram);
                    _txTelegram.SetCommand(KNX_COMMAND_VALUE_RESPONSE);
                    _txTelegram.UpdateChecksum();
                    _tpuart->sendTelegram(_txTelegram);
                    _state = TX_ONGOING;
                    break;

                case KNX_WRITE_REQUEST:  // a write operation of a Com Object on the KNX network is required
                    // update the com obj value
                    //DEBUG_PRINTLN(F("KNX_WRITE_REQUEST index=%d"), action.index);

                    if ((comObj->getLength()) <= 2) {
                        //DEBUG_PRINTLN(F("len <= 2"));
                        comObj->updateValue(action.byteValue);
                    } else {
                        //DEBUG_PRINTLN(F("len > 2"));
                        comObj->updateValue(action.valuePtr);
                        free(action.valuePtr);
                    }
                    // transmit the value through KNX network only if the Com Object has transmit attribute
                    if ((comObj->getIndicator()) & KNX_COM_OBJ_T_INDICATOR) {
                        //DEBUG_PRINTLN(F("set tx ongoing"));
                        comObj->copyAttributes(_txTelegram);
                        comObj->copyValue(_txTelegram);
                        _txTelegram.SetCommand(KNX_COMMAND_VALUE_WRITE);
                        _txTelegram.UpdateChecksum();
                        _tpuart->sendTelegram(_txTelegram);
                        _state = TX_ONGOING;
                    }
                    break;

                default:
                    break;
            }
        }
    }

    // STEP 4 : LET THE TP-UART TRANSMIT KNX MESSAGES
    // The TPUART TX task is executed every 800 us
    nowTimeMicros = micros();
    if (TimeDeltaWord(nowTimeMicros, _lastTXTimeMicros) > 800) {
        _lastTXTimeMicros = nowTimeMicros;
        _tpuart->txTask();
    }
}

/**
 * Quick method to read a short (<=1 byte) com object
 * NB : The returned value will be hazardous in case of use with long objects
 * @param objectIndex
 * @retreturn 1-byte value of comobj
 */
byte KnxDevice::read(byte objectIndex) {
    //    return _comObjectsList[objectIndex].getValue();
    return (objectIndex == 255 ? _progComObj.getValue() : _comObjectsList[objectIndex].getValue());
}

/**
 * Read an usual format com object
 * Supported DPT formats are short com object, U16, V16, U32, V32, F16 and F32 (not implemented yet)
 */
template <typename T>
KnxDeviceStatus KnxDevice::read(byte objectIndex, T& returnedValue) {
    KnxComObject* comObj = (objectIndex == 255 ? &_progComObj : &_comObjectsList[objectIndex]);
    // Short com object case
    if (comObj->getLength() <= 2) {
        returnedValue = (T)comObj->getValue();
        return KNX_DEVICE_OK;
    } else  // long object case, let's see if we are able to translate the DPT value
    {
        byte dptValue[14];  // define temporary DPT value with max length
        comObj->getValue(dptValue);
        return ConvertFromDpt(dptValue, returnedValue, pgm_read_byte(&KnxDptToFormat[comObj->getDptId()]));
    }
}

template KnxDeviceStatus KnxDevice::read<bool>(byte objectIndex, bool& returnedValue);
template KnxDeviceStatus KnxDevice::read<byte>(byte objectIndex, byte& returnedValue);
template KnxDeviceStatus KnxDevice::read<short>(byte objectIndex, short& returnedValue);
template KnxDeviceStatus KnxDevice::read<unsigned short>(byte objectIndex, unsigned short& returnedValue);
template KnxDeviceStatus KnxDevice::read<int>(byte objectIndex, int& returnedValue);
template KnxDeviceStatus KnxDevice::read<unsigned int>(byte objectIndex, unsigned int& returnedValue);
template KnxDeviceStatus KnxDevice::read<long>(byte objectIndex, long& returnedValue);
template KnxDeviceStatus KnxDevice::read<unsigned long>(byte objectIndex, unsigned long& returnedValue);
template KnxDeviceStatus KnxDevice::read<float>(byte objectIndex, float& returnedValue);
template KnxDeviceStatus KnxDevice::read<double>(byte objectIndex, double& returnedValue);

// Read any type of com object (DPT value provided as is)

KnxDeviceStatus KnxDevice::read(byte objectIndex, byte returnedValue[]) {
    KnxComObject* comObj = (objectIndex == 255 ? &_progComObj : &_comObjectsList[objectIndex]);
    comObj->getValue(returnedValue);
    return KNX_DEVICE_OK;
}

// Update an usual format com object
// Supported DPT types are short com object, U16, V16, U32, V32, F16 and F32
// The Com Object value is updated locally
// And a telegram is sent on the KNX bus if the com object has communication & transmit attributes

template <typename T>
KnxDeviceStatus KnxDevice::write(byte objectIndex, T value) {
    TxAction action;
    byte* destValue;

    KnxComObject* comObj = (objectIndex == 255 ? &_progComObj : &_comObjectsList[objectIndex]);
    if (!comObj->isActive()) {
        return KNX_DEVICE_COMOBJ_INACTIVE;
    }
    byte length = comObj->getLength();

    if (length <= 2)
        action.byteValue = (byte)value;         // short object case
    else {                                      // long object case, let's try to translate value to the com object DPT
        destValue = (byte*)malloc(length - 1);  // allocate the memory for DPT
        KnxDeviceStatus status = ConvertToDpt(value, destValue, pgm_read_byte(&KnxDptToFormat[comObj->getDptId()]));
        if (status)  // translation error
        {
            free(destValue);
            return status;  // we cannot convert, we stop here
        } else
            action.valuePtr = destValue;
    }
    // add WRITE action in the TX action queue
    action.command = KNX_WRITE_REQUEST;
    action.index = objectIndex;
    _txActionList.append(action);
    return KNX_DEVICE_OK;
}

template KnxDeviceStatus KnxDevice::write<bool>(byte objectIndex, bool value);
template KnxDeviceStatus KnxDevice::write<byte>(byte objectIndex, byte value);
template KnxDeviceStatus KnxDevice::write<short>(byte objectIndex, short value);
template KnxDeviceStatus KnxDevice::write<unsigned short>(byte objectIndex, unsigned short value);
template KnxDeviceStatus KnxDevice::write<int>(byte objectIndex, int value);
template KnxDeviceStatus KnxDevice::write<unsigned int>(byte objectIndex, unsigned int value);
template KnxDeviceStatus KnxDevice::write<long>(byte objectIndex, long value);
template KnxDeviceStatus KnxDevice::write<unsigned long>(byte objectIndex, unsigned long value);
template KnxDeviceStatus KnxDevice::write<float>(byte objectIndex, float value);
template KnxDeviceStatus KnxDevice::write<double>(byte objectIndex, double value);

/**
 * Update any type of com object (rough DPT value shall be provided)
 * The Com Object value is updated locally
 * And a telegram is sent on the KNX bus if the com object has communication & transmit attributes
 */
KnxDeviceStatus KnxDevice::write(byte objectIndex, byte valuePtr[]) {
    TxAction action;
    //TxAction action2;

    // get length of comobj for copying value into tx-action struct
    KnxComObject* comObj = (objectIndex == 255 ? &_progComObj : &_comObjectsList[objectIndex]);
    byte length = comObj->getLength();

    //if (!(objectIndex == 255 ? _progComObj.isActive() : _comObjectsList[objectIndex].isActive())) {
    //	return KNX_DEVICE_COMOBJ_INACTIVE;
    //}

    //byte length = (objectIndex == 255 ? _progComObj.GetLength() : _comObjectsList[objectIndex].GetLength());

    // check we are in long object case
    if (length > 2) {
        // add WRITE action in the TX action queue
        action.command = KNX_WRITE_REQUEST;
        action.index = objectIndex;

        // allocate the memory for long value
        byte* dptValue = (byte*)malloc(length - 1);

        for (byte i = 0; i < length - 1; i++) {
            dptValue[i] = valuePtr[i];  // copy value
                                        //            DEBUG_PRINTLN(F("dptValue[%d]=0x%02x == valuePtr[%d]=0x%02x"), i, dptValue[i], i, valuePtr[i]);
        }
        action.valuePtr = (byte*)dptValue;

        // bis hier hin alles okay
        _txActionList.append(action);
        //for (byte i = 0; i < length - 1; i++) {
        //    DEBUG_PRINTLN(F("action.valuePtr[%d]=0x%02x"), i, action.valuePtr[i]);
        //}
        // immer noch okay.

        // TODO: pop machen und dann daten testen. Wenn dann noch okay, muss jemand die Daten zu späterem zeitpunkt manipulieren.
        //_txActionList.pop(action2);
        //for (byte i = 0; i < length - 1; i++) {
        //    DEBUG_PRINTLN(F("action2.valuePtr[%d]=0x%02x"), i, action2.valuePtr[i]);
        //}

        return KNX_DEVICE_OK;
    }
    return KNX_DEVICE_ERROR;
}

/**
 *  Com Object KNX Bus Update request
 * Request the local object to be updated with the value from the bus
 * NB : the function is asynchroneous, the update completion is notified by the knxEvents() callback
 */
void KnxDevice::update(byte objectIndex) {
    TxAction action;
    action.command = KNX_READ_REQUEST;
    action.index = objectIndex;
    _txActionList.append(action);
}

/**
 *  The function returns true if there is rx/tx activity ongoing, else false
 */
bool KnxDevice::isActive() const {
    if (_tpuart->isActive()) return true;           // TPUART is active
    if (_state == TX_ONGOING) return true;          // the Device is sending a request
    if (_txActionList.getItemCount()) return true;  // there is at least one tx action in the queue
    return false;
}

KnxDeviceStatus KnxDevice::setComObjectAddress(byte index, word addr) {
    if (_state != INIT) return KNX_DEVICE_INIT_ERROR;
    if (index >= _numberOfComObjects) return KNX_DEVICE_INVALID_INDEX;
    _comObjectsList[index].setAddr(addr);
    return KNX_DEVICE_OK;
}
KnxDeviceStatus KnxDevice::setComObjectIndicator(byte index, byte indicator) {
    if (_state != INIT) return KNX_DEVICE_INIT_ERROR;
    if (index >= _numberOfComObjects) return KNX_DEVICE_INVALID_INDEX;
    _comObjectsList[index].setIndicator(indicator);
    return KNX_DEVICE_OK;
}

word KnxDevice::getComObjectAddress(byte index) {
    return _comObjectsList[index].getAddr();
}

/**
 * Static GetTpUartEvents() function called by the KnxTpUart layer (callback)
 */
void KnxDevice::GetTpUartEvents(KnxTpUartEvent event) {
    TxAction action;
    byte targetedComObjIndex;  // index of the Com Object targeted by the event
    DEBUG_PRINTLN(F("KnxDevice::GetTpUartEvents"));

    switch (event) {
        // Manage RECEIVED MESSAGES
        case TPUART_EVENT_RECEIVED_KNX_TELEGRAM: {
            Knx._state = IDLE;

            AddressedComObjects addressedComObjects = Knx._tpuart->getAddressedComObjects();  //.get(0, targetedComObjIndex);

            DEBUG_PRINTLN(F("KnxDevice::GetTpUartEvents need to process %d comobjs."), addressedComObjects.items);

            // handle all addressed comobjs
            for (int i = 0; i < addressedComObjects.items; i++) {
                targetedComObjIndex = addressedComObjects.list[i];

                KnxComObject* comObj = (targetedComObjIndex == 255 ? &Knx._progComObj : &_comObjectsList[targetedComObjIndex]);

                DEBUG_PRINTLN(F("KnxDevice::GetTpUartEvents targetedComObjIndex=%d command=%d"), targetedComObjIndex, Knx._rxTelegram->GetCommand());

                byte indicator = comObj->getIndicator();

                switch (Knx._rxTelegram->GetCommand()) {
                    case KNX_COMMAND_VALUE_READ:
                        // READ command coming from the bus
                        // if the Com Object has read attribute, then add RESPONSE action in the TX action list
                        if ((comObj->getIndicator()) & KNX_COM_OBJ_R_INDICATOR) {  // The targeted Com Object can indeed be read
                            action.command = KNX_RESPONSE_REQUEST;
                            action.index = targetedComObjIndex;
                            Knx._txActionList.append(action);
                        }
                        break;

                    case KNX_COMMAND_VALUE_RESPONSE:
                        // RESPONSE command coming from KNX network, we update the value of the corresponding Com Object.
                        // We 1st check that the corresponding Com Object has UPDATE attribute
                        if ((comObj->getIndicator()) & KNX_COM_OBJ_U_INDICATOR) {
                            comObj->updateValue(*(Knx._rxTelegram));
                            //We notify the upper layer of the update
                            knxEvents(targetedComObjIndex);
                        }
                        break;

                    case KNX_COMMAND_VALUE_WRITE:
                        // WRITE command coming from KNX network, we update the value of the corresponding Com Object.
                        // We 1st check that the corresponding Com Object has WRITE attribute

                        DEBUG_PRINTLN(F("ComObj Indicator=0x%02X"), indicator);
                        if ((indicator)&KNX_COM_OBJ_W_INDICATOR) {
                            comObj->updateValue(*(Knx._rxTelegram));
                            //We notify the upper layer of the update
                            if (Konnekting.isActive()) {
                                DEBUG_PRINTLN(F("Routing event to konnektingKnxEvents #%d"), targetedComObjIndex);
                                konnektingKnxEvents(targetedComObjIndex);
                            } else {
                                DEBUG_PRINTLN(F("No event routing, because not active: #%d"), targetedComObjIndex);
                                //                        knxEvents(targetedComObjIndex);
                            }
                        } else {
                            DEBUG_PRINTLN(F("Wrong config byte on comobj #%d: 0x%02X"), targetedComObjIndex, indicator);
                        }
                        break;

                        // case KNX_COMMAND_MEMORY_WRITE : break; // Memory Write not handled

                    default:
                        break;  // not supposed to happen
                }
            }

        } break;
        // Manage RESET events
        case TPUART_EVENT_RESET: {
            while (Knx._tpuart->reset() == KNX_TPUART_ERROR)
                ;
            Knx._tpuart->init();
            Knx._state = IDLE;
        } break;
        // just log unhandled event id
        default:
            DEBUG_PRINTLN(F("GetTpUartEvents unhandled event=%d"), event);
    }
}

// Static TxTelegramAck() function called by the KnxTpUart layer (callback)
void KnxDevice::TxTelegramAck(TpUartTxAck) {
    Knx._state = IDLE;
}

// Functions to convert a standard C type to a DPT format
// NB : only the usual DPT formats are supported (U16, V16, U32, V32, F16 and F32 (not yet implemented)
template <typename T>
KnxDeviceStatus ConvertFromDpt(const byte dptOriginValue[], T& resultValue, byte dptFormat) {
    switch (dptFormat) {
        case KNX_DPT_FORMAT_U16:
        case KNX_DPT_FORMAT_V16:
            resultValue = (T)((unsigned int)dptOriginValue[0] << 8);
            resultValue += (T)(dptOriginValue[1]);
            return KNX_DEVICE_OK;
            break;

        case KNX_DPT_FORMAT_U32:
        case KNX_DPT_FORMAT_V32:
            resultValue = (T)((unsigned long)dptOriginValue[0] << 24);
            resultValue += (T)((unsigned long)dptOriginValue[1] << 16);
            resultValue += (T)((unsigned long)dptOriginValue[2] << 8);
            resultValue += (T)(dptOriginValue[3]);
            return KNX_DEVICE_OK;
            break;

        case KNX_DPT_FORMAT_F16: {
            // Get the DPT sign, mantissa and exponent
            int signMultiplier = (dptOriginValue[0] & 0x80) ? -1 : 1;
            word absoluteMantissa = dptOriginValue[1] + ((dptOriginValue[0] & 0x07) << 8);
            if (signMultiplier == -1) {  // Calculate absolute mantissa value in case of negative mantissa
                // Abs = 2's complement + 1
                absoluteMantissa = ((~absoluteMantissa) & 0x07FF) + 1;
            }
            byte exponent = (dptOriginValue[0] & 0x78) >> 3;
            resultValue = (T)(0.01 * ((long)absoluteMantissa << exponent) * signMultiplier);
            return KNX_DEVICE_OK;
        } break;

        case KNX_DPT_FORMAT_F32:
            return KNX_DEVICE_NOT_IMPLEMENTED;
            break;

        default:
            return KNX_DEVICE_ERROR;
    }
}

template KnxDeviceStatus ConvertFromDpt<unsigned char>(const byte dptOriginValue[], unsigned char&, byte dptFormat);
template KnxDeviceStatus ConvertFromDpt<char>(const byte dptOriginValue[], char&, byte dptFormat);
template KnxDeviceStatus ConvertFromDpt<unsigned int>(const byte dptOriginValue[], unsigned int&, byte dptFormat);
template KnxDeviceStatus ConvertFromDpt<int>(const byte dptOriginValue[], int&, byte dptFormat);
template KnxDeviceStatus ConvertFromDpt<unsigned long>(const byte dptOriginValue[], unsigned long&, byte dptFormat);
template KnxDeviceStatus ConvertFromDpt<long>(const byte dptOriginValue[], long&, byte dptFormat);
template KnxDeviceStatus ConvertFromDpt<float>(const byte dptOriginValue[], float&, byte dptFormat);
template KnxDeviceStatus ConvertFromDpt<double>(const byte dptOriginValue[], double&, byte dptFormat);

// Functions to convert a standard C type to a DPT format
// NB : only the usual DPT formats are supported (U16, V16, U32, V32, F16 and F32 (not yet implemented)

template <typename T>
KnxDeviceStatus ConvertToDpt(T originValue, byte dptDestValue[], byte dptFormat) {
    switch (dptFormat) {
        case KNX_DPT_FORMAT_U16:
        case KNX_DPT_FORMAT_V16:
            dptDestValue[0] = (byte)((unsigned int)originValue >> 8);
            dptDestValue[1] = (byte)(originValue);
            return KNX_DEVICE_OK;
            break;

        case KNX_DPT_FORMAT_U32:
        case KNX_DPT_FORMAT_V32:
            dptDestValue[0] = (byte)((unsigned long)originValue >> 24);
            dptDestValue[1] = (byte)((unsigned long)originValue >> 16);
            dptDestValue[2] = (byte)((unsigned long)originValue >> 8);
            dptDestValue[3] = (byte)(originValue);
            return KNX_DEVICE_OK;
            break;

        case KNX_DPT_FORMAT_F16: {
            long longValuex100 = (long)(100.0 * originValue);
            boolean negativeSign = (longValuex100 & 0x80000000) ? true : false;
            byte exponent = 0;
            byte round = 0;

            if (negativeSign) {
                while (longValuex100 < (long)(-2048)) {
                    exponent++;
                    round = (byte)(longValuex100)&1;
                    longValuex100 >>= 1;
                    longValuex100 |= 0x80000000;
                }
            } else {
                while (longValuex100 > (long)(2047)) {
                    exponent++;
                    round = (byte)(longValuex100)&1;
                    longValuex100 >>= 1;
                }
            }
            if (round) longValuex100++;
            dptDestValue[1] = (byte)longValuex100;
            dptDestValue[0] = (byte)(longValuex100 >> 8) & 0x7;
            dptDestValue[0] += exponent << 3;
            if (negativeSign) dptDestValue[0] += 0x80;
            return KNX_DEVICE_OK;
        } break;

        case KNX_DPT_FORMAT_F32:
            return KNX_DEVICE_NOT_IMPLEMENTED;
            break;

        default:
            return KNX_DEVICE_ERROR;
    }
}

template KnxDeviceStatus ConvertToDpt<unsigned char>(unsigned char, byte dptDestValue[], byte dptFormat);
template KnxDeviceStatus ConvertToDpt<char>(char, byte dptDestValue[], byte dptFormat);
template KnxDeviceStatus ConvertToDpt<unsigned int>(unsigned int, byte dptDestValue[], byte dptFormat);
template KnxDeviceStatus ConvertToDpt<int>(int, byte dptDestValue[], byte dptFormat);
template KnxDeviceStatus ConvertToDpt<unsigned long>(unsigned long, byte dptDestValue[], byte dptFormat);
template KnxDeviceStatus ConvertToDpt<long>(long, byte dptDestValue[], byte dptFormat);
template KnxDeviceStatus ConvertToDpt<float>(float, byte dptDestValue[], byte dptFormat);
template KnxDeviceStatus ConvertToDpt<double>(double, byte dptDestValue[], byte dptFormat);

// EOF
