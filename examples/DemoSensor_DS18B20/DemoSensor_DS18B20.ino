#include <KonnektingDevice.h>

// include device related configuration code, created by "KONNEKTING CodeGenerator"
#include "kdevice_Temperature_DS18B20.h"

#include <OneWire.h> //https://github.com/PaulStoffregen/OneWire
#include <DallasTemperature.h> //https://github.com/milesburton/Arduino-Temperature-Control-Library


// ################################################
// ### DEBUG CONFIGURATION
// ################################################
#define KDEBUG // comment this line to disable DEBUG mode
#ifdef KDEBUG
#include <DebugUtil.h>

// Get correct serial port for debugging
#ifdef __AVR_ATmega32U4__
// Leonardo/Micro/ProMicro use the USB serial port
#define DEBUGSERIAL Serial
#elif ESP8266
// ESP8266 use the 2nd serial port with TX only
#define DEBUGSERIAL Serial1
#elif __SAMD21G18A__
#define DEBUGSERIAL SerialUSB
#else
// All other, (ATmega328P f.i.) use software serial
#include <SoftwareSerial.h>
SoftwareSerial softserial(11, 10); // RX, TX
#define DEBUGSERIAL softserial
#endif
// end of debugging defs
#endif

// ################################################
// ### IO Configuration
// ################################################
#define PROG_LED_PIN 13
#define PROG_BUTTON_PIN 7
// OneWire on Pin 5
#define ONE_WIRE 5 

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


OneWire oneWire(ONE_WIRE); 
DallasTemperature sensors(&oneWire);/* Dallas Temperature Library */

unsigned long previousTimeTemp = 0;
unsigned long previousTimeHumd = 0;

float previousTemp = 0;
float previousHumd = 0;
float currentTemp = 0;
float currentHumd = 0;

int typeTemp = 0;
long intervalTempUser = 5; //typical temperatur polling interval (ms)
float diffTempUser = 0.2; //minimal difference between previous and current temperature [°C]
uint8_t valueTempMin = 255;
int16_t limitTempMin = 0;
uint8_t valueTempMax = 255;
int16_t limitTempMax = 0;

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
    
    sensors.begin();
    sensors.setResolution(10); // 10-Bit resolution for DS18B20
    
    typeTemp = (int) Konnekting.getUINT8Param(PARAM_tempSendUpdate);
    
    //temperature polling interval (ms)
    intervalTempUser = (long) Konnekting.getUINT32Param(PARAM_tempPollingTime)*1000; 
    
    //minimal difference between previous and current temperature [°C]
    diffTempUser = (float) Konnekting.getUINT8Param(PARAM_tempDiff)*0.1; 
    valueTempMin = Konnekting.getUINT8Param(PARAM_tempMinValue);
    limitTempMin = Konnekting.getINT16Param(PARAM_tempMinLimit);
    valueTempMax = Konnekting.getUINT8Param(PARAM_tempMaxValue);
    limitTempMax = Konnekting.getINT16Param(PARAM_tempMaxLimit);
    }
    Debug.println(F("Setup is ready. go to loop..."));
}

void loop() {
    Knx.task();
    unsigned long currentTime = millis();
    
    // only do measurements and other sketch related stuff if not in programming mode & not with factory settings
    if (Konnekting.isReadyForApplication()) {
        
        // Get temperature
        if ((currentTime - previousTimeTemp) >= intervalTempUser) {
           Knx.task();
           unsigned long start = millis();
           sensors.requestTemperatures();
           currentTemp = sensors.getTempCByIndex(0);
           Knx.write(COMOBJ_tempValue, currentTemp);
           unsigned long end = millis();
           Knx.task();
#ifdef KDEBUG              
            char currTempChar[10];
            dtostrf(currentTemp,6,2,currTempChar);
            Debug.println(F("currentTemp: %s time: %d ms"), currTempChar, (end-start));
#endif
            if (currentTemp < 200) {
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
                
            }
            previousTimeTemp = currentTime;
        }
    }
}
