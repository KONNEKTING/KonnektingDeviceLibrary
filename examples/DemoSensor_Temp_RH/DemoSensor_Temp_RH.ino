#include <KonnektingDevice.h>

// include device related configuration code, created by "KONNEKTING CodeGenerator"
#include "kdevice_Temp_RH.h"

// ################################################
// ### DEBUG CONFIGURATION
// ################################################
//#define KDEBUG // comment this line to disable DEBUG mode
#ifdef KDEBUG
#include <DebugUtil.h>

// Get correct serial port for debugging
#ifdef __AVR_ATmega32U4__
// Leonardo/Micro/ProMicro use the USB serial port
#define DEBUGSERIAL Serial
#elif ESP8266
// ESP8266 use the 2nd serial port with TX only
#define DEBUGSERIAL Serial1
#else
// All other, (ATmega328P f.i.) use software serial
#include <SoftwareSerial.h>
SoftwareSerial softserial(11, 10); // RX, TX
#define DEBUGSERIAL softserial
#endif
// end of debugging defs
#endif

#include <Wire.h>
#include "SparkFunHTU21D.h" // http://librarymanager/All#SparkFun_HTU21D

// ################################################
// ### KONNEKTING Configuration
// ################################################
#ifdef __AVR_ATmega328P__
#define KNX_SERIAL Serial // Nano/ProMini etc. use Serial
#elif ESP8266
#define KNX_SERIAL Serial // ESP8266 use Serial
#else
#define KNX_SERIAL Serial1 // Leonardo/Micro etc. use Serial1
#endif

// ################################################
// ### IO Configuration
// ################################################
#define PROG_LED_PIN 13
#define PROG_BUTTON_PIN 7


// Create an instance for HTU21D sensor
HTU21D htu;

// ################################################
// ### Global variables, sketch related
// ################################################

unsigned long previousTimeTemp = 0;
unsigned long previousTimeHumd = 0;

float previousTemp = 0;
float previousHumd = 0;
float currentTemp = 0;
float currentHumd = 0;

int typeTemp;
long intervalTempUser; //typical temperatur polling interval (ms)
float diffTempUser; //minimal difference between previous and current temperature [°C]
uint8_t valueTempMin;
int16_t limitTempMin;
uint8_t valueTempMax;
int16_t limitTempMax;

uint8_t typeHumd;
long intervalHumdUser; //typical temperatur polling interval (ms)
float diffHumdUser;
uint8_t valueHumdMin;
int16_t limitHumdMin;
uint8_t valueHumdMax;
int16_t limitHumdMax;

// Callback function to handle com objects updates

void knxEvents(byte index) {
    // nothing to do in this sketch
};

void limitReached(float comVal, float comValMin, float comValMax, int minObj, int maxObj, int minVal, int maxVal) {
    if (minVal != 255) {
        if (comVal <= comValMin) {
            Knx.write(minObj, minVal);
        }
    }
    if (maxVal != 255) {
        if (comVal >= comValMax) {
            Knx.write(maxObj, maxVal);
        }
    }
};

void setup() {

    // debug related stuff
#ifdef KDEBUG

    // Start debug serial with 9600 bauds
    DEBUGSERIAL.begin(9600);

#ifdef __AVR_ATmega32U4__
    // wait for serial port to connect. Needed for Leonardo/Micro/ProMicro only
    while (!DEBUGSERIAL)
#endif

    // make debug serial port known to debug class
    // Means: KONNEKTING will sue the same serial port for console debugging
    Debug.setPrintStream(&DEBUGSERIAL);
#endif

    // Initialize KNX enabled Arduino Board
    Konnekting.init(KNX_SERIAL, PROG_BUTTON_PIN, PROG_LED_PIN, MANUFACTURER_ID, DEVICE_ID, REVISION);
    if (!Konnekting.isFactorySetting()){
        int startDelay = (int) Konnekting.getUINT16Param(PARAM_initialDelay);
        if (startDelay > 0) {
#ifdef KDEBUG  
     Debug.println(F("delay for %d ms"),startDelay);
#endif
     delay(startDelay);
#ifdef KDEBUG  
     Debug.println(F("ready!"));
#endif
    }
    //set i2c clock to 400kHz
    Wire.setClock(400000);
    //htu sensor start
    htu.begin();

    typeTemp = (int) Konnekting.getUINT8Param(PARAM_tempSendUpdate);
    
    //temperature polling interval (ms)
    intervalTempUser = (long) Konnekting.getUINT32Param(PARAM_tempPollingTime)*1000; 
    
    //minimal difference between previous and current temperature [°C]
    diffTempUser = (float) Konnekting.getUINT8Param(PARAM_tempDiff)*0.1; 
    valueTempMin = Konnekting.getUINT8Param(PARAM_tempMinValue);
    limitTempMin = Konnekting.getINT16Param(PARAM_tempMinLimit);
    valueTempMax = Konnekting.getUINT8Param(PARAM_tempMaxValue);
    limitTempMax = Konnekting.getINT16Param(PARAM_tempMaxLimit);

    typeHumd = Konnekting.getUINT8Param(PARAM_rhSendUpdate);
    intervalHumdUser = (long) Konnekting.getUINT32Param(PARAM_rhPollingTime)*1000; //humidity polling interval (ms)
    diffHumdUser = (float) Konnekting.getUINT8Param(PARAM_rhDiff)*0.1;
    valueHumdMin = Konnekting.getUINT8Param(PARAM_rhMinValue);
    limitHumdMin = Konnekting.getINT16Param(PARAM_rhMinLimit);
    valueHumdMax = Konnekting.getUINT8Param(PARAM_rhMaxValue);
    limitHumdMax = Konnekting.getINT16Param(PARAM_rhMaxLimit);
    }
    Debug.println(F("Setup is ready. go to loop..."));
}

void loop() {
    Knx.task();
    unsigned long currentTime = millis();
    
    // only do measurements and other sketch related stuff if not in programming mode
    if (Konnekting.isReadyForApplication()) {
        
        // Get temperature
        if ((currentTime - previousTimeTemp) >= intervalTempUser) {
           
            unsigned long start = millis();
            currentTemp = htu.readTemperature();
            unsigned long end = millis();
#ifdef KDEBUG              
            char currTempChar[10];
            dtostrf(currentTemp,6,2,currTempChar);
            Debug.println(F("currentTemp: %s time: %d ms"), currTempChar, (end-start));
#endif
            if (currentTemp < 900) {
                switch (typeTemp) {
                    case 0:
                        Knx.write(COMOBJ_tempValue, currentTemp);
                        break;
                    case 1:
                        if (abs(currentTemp * 100 - previousTemp * 100) >= diffTempUser * 100) {//"*100" => "float to int"
                            Knx.write(COMOBJ_tempValue, currentTemp);
                            previousTemp = currentTemp;
                        }
                        break;
                    default:
                        break;
                }
                limitReached(currentTemp, limitTempMin, limitTempMax, COMOBJ_tempMin, COMOBJ_tempMax, valueTempMin, valueTempMax);
                
            }else{
#ifdef KDEBUG              
            Debug.println(F("Sensor error!"));
#endif 
            }
            previousTimeTemp = currentTime;
        }
        // Get humidity
        if ((currentTime - previousTimeHumd) >= intervalHumdUser) {
            unsigned long start = millis();
            currentHumd = htu.readHumidity();
            unsigned long end = millis();
#ifdef KDEBUG              
            char currRHChar[10];
            dtostrf(currentHumd,6,2,currRHChar);
            Debug.println(F("currentHumd: %s time: %d ms"), currRHChar, (end-start));
#endif
            if (currentHumd < 900) {
                
                switch (typeHumd) {
                    case 0:
                        Knx.write(COMOBJ_rhValue, currentHumd);
                        break;
                    case 1:
                        if (abs(currentHumd * 100 - previousHumd * 100) >= diffHumdUser * 100) {
                            Knx.write(COMOBJ_rhValue, currentHumd);
                            previousHumd = currentHumd;
                        }
                    default:
                        break;
                }
                limitReached(currentHumd, limitHumdMin, limitHumdMax, COMOBJ_rhMin, COMOBJ_rhMax, valueHumdMin, valueHumdMax);
                
            }else{
#ifdef KDEBUG              
            Debug.println(F("Sensor error!"));
#endif 
            }
            previousTimeHumd = currentTime;
        }
    }
}
