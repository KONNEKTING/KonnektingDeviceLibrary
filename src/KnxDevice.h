/*
 *    This file is part of KONNEKTING Device Library.
 *
 *    The KONNEKTING Device Library is free software: you can redistribute it and/or modify
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

#ifndef KNXDEVICE_H
#define KNXDEVICE_H

#include "Arduino.h"
#include "KnxTelegram.h"
#include "KnxComObject.h"
#include "RingBuff.h"
#include "KnxTpUart.h"
#include "KonnektingDevice.h"

// !!!!!!!!!!!!!!! FLAG OPTIONS !!!!!!!!!!!!!!!!!
// DEBUG :
//#define KNXDEVICE_DEBUG_INFO   // Uncomment to activate info traces

// Values returned by the KnxDevice functions
enum KnxDeviceStatus {
  KNX_DEVICE_OK = 0,
  KNX_DEVICE_INVALID_INDEX = 1,
  KNX_DEVICE_INIT_ERROR = 2,
  KNX_DEVICE_COMOBJ_INACTIVE = 3,
  KNX_DEVICE_NOT_IMPLEMENTED = 254,
  KNX_DEVICE_ERROR = 255
};

// Macro functions for conversion of physical and group addresses
inline word P_ADDR(byte area, byte line, byte busdevice)
{ return (word) ( ((area&0xF)<<12) + ((line&0xF)<<8) + busdevice ); }

inline word G_ADDR(byte maingrp, byte midgrp, byte subgrp)
{ return (word) ( ((maingrp&0x1F)<<11) + ((midgrp&0x7)<<8) + subgrp ); }

#define ACTIONS_QUEUE_SIZE 16

// KnxDevice internal state
enum InternalDeviceState {
  INIT,
  IDLE,
  TX_ONGOING,
};

// Action types
enum TxActionType {
  KNX_READ_REQUEST,
  KNX_WRITE_REQUEST,
  KNX_RESPONSE_REQUEST
};

typedef struct TxAction{
  TxActionType command; // Action type to be performed
  byte index; // Index of the involved ComObject
  union { // Value
    // Field used in case of short value (value width <= 1 byte)
    struct {
      byte byteValue;
      byte notUsed;
    };
    byte *valuePtr; // Field used in case of long value (width > 1 byte), space is allocated dynamically
  };
} TxAction;


// Callback function to catch and treat KNX events
// The definition shall be provided by the end-user
extern void knxEvents(byte);


// --------------- Definition of the functions for DPT translation --------------------
// Functions to convert a DPT format to a standard C type
// NB : only the usual DPT formats are supported (U16, V16, U32, V32, F16 and F32)
template <typename T> KnxDeviceStatus ConvertFromDpt(const byte dpt[], T& result, byte dptFormat);

// Functions to convert a standard C type to a DPT format
// NB : only the usual DPT formats are supported (U16, V16, U32, V32, F16 and F32)
template <typename T> KnxDeviceStatus ConvertToDpt(T value, byte dpt[], byte dptFormat);


class KnxDevice {
        
    // List of Com Objects attached to the KNX Device
    // The definition shall be provided by the end-user
    static KnxComObject _comObjectsList[];          
    
    // Nb of attached Com Objects
    // The value shall be provided by the end-user
    static const byte _numberOfComObjects;   
    
    KnxComObject _progComObj = KnxComObject(KNX_DPT_60000_60000 /* KNX PROGRAM */, KNX_COM_OBJ_C_W_U_T_INDICATOR);  
    
    // Current KnxDevice state
    InternalDeviceState _state;  
    
    // TPUART associated to the KNX Device
    KnxTpUart *_tpuart;                             
    
    // Queue of transmit actions to be performed
    RingBuff<TxAction, ACTIONS_QUEUE_SIZE> _txActionList; 
    
    // True when all the Com Object with Init attr have been initialized
    bool _initCompleted;                         
    
    // Index to the last initiated object
    byte _initIndex;                                
    
    // Time (in msec) of the last init (read) request on the bus
    word _lastInitTimeMillis;                       
    
    // Time (in msec) of the last Tpuart Rx activity;
    word _lastRXTimeMicros;                         
    
    // Time (in msec) of the last Tpuart Tx activity;
    word _lastTXTimeMicros;                         
    
    // Telegram object used for telegrams sending
    KnxTelegram _txTelegram;                        
    
    // Reference to the telegram received by the TPUART
    KnxTelegram *_rxTelegram;                       
    
    // Constructor, Destructor
    // private constructor (singleton design pattern)
    KnxDevice();  
    // private destructor (singleton design pattern)
    ~KnxDevice() {}  
    // private copy constructor (singleton design pattern) 
    KnxDevice (const KnxDevice&); 

    
  public:
      
    // unique KnxDevice instance (singleton design pattern)      
    static KnxDevice Knx; 
    
    int getNumberOfComObjects();
    
    /*
     * Start the KNX Device
     * return KNX_DEVICE_ERROR (255) if begin() failed
     * else return KNX_DEVICE_OK
     */
    KnxDeviceStatus begin(HardwareSerial& serial, word physicalAddr);

    /*
     * Stop the KNX Device
     */ 
    void end();

    /*
     * KNX device execution task
     * This function shall be called in the "loop()" Arduino function
     */
    void task(void);

    /* 
     * Quick method to read a short (<=1 byte) com object
     * NB : The returned value will be hazardous in case of use with long objects
     */
    byte read(byte objectIndex);  

    /*
     *  Read an usual format com object
     * Supported DPT formats are short com object, U16, V16, U32, V32, F16 and F32
     */
    template <typename T>  KnxDeviceStatus read(byte objectIndex, T& returnedValue);

    /*
     *  Read any type of com object (DPT value provided as is)
     */
    KnxDeviceStatus read(byte objectIndex, byte returnedValue[]);

    // Update com object functions :
    // For all the update functions, the com object value is updated locally
    // and a telegram is sent on the KNX bus if the object has both COMMUNICATION & TRANSMIT attributes set

    /*
     * Update an usual format com object
     * Supported DPT types are short com object, U16, V16, U32, V32, F16 and F32
     */
    template <typename T>  KnxDeviceStatus write(byte objectIndex, T value);

    /*
     * Update any type of com object (rough DPT value shall be provided)
     */
    KnxDeviceStatus write(byte objectIndex, byte valuePtr[]);
    

    /*
     * Com Object KNX Bus Update request
     * Request the local object to be updated with the value from the bus
     * NB : the function is asynchroneous, the update completion is notified by the knxEvents() callback
     */
    void update(byte objectIndex);

    
    /**
     * TODO document me
     * @return 
     */ 
    bool isActive(void) const;
        
    KnxDeviceStatus setComObjectIndicator(byte index, byte indicator);
    KnxDeviceStatus setComObjectAddress(byte index, word addr);
    
    /*
     *  Gets the address of an commobjects
     */
    word getComObjectAddress(byte index);
    
  private:
    /*
     * Static getTpUartEvents() function called by the KnxTpUart layer (callback)
     */
    static void getTpUartEvents(KnxTpUartEvent event);

    /* 
     * Static txTelegramAck() function called by the KnxTpUart layer (callback)
     */
    static void txTelegramAck(TpUartTxAck);
    
};

// Reference to the KnxDevice unique instance
extern KnxDevice& Knx;

#endif // KNXDEVICE_H
