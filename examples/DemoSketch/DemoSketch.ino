#include <KonnektingDevice.h>

// include device related configuration code, created by "KONNEKTING CodeGenerator"
#include "KONNEKTING_deviceconfiguration_DemoSketch.h"

// ################################################
// ### DEBUG CONFIGURATION
// ################################################
//#define DEBUG // comment this line to disable DEBUG mode
#ifdef DEBUG
#include <DebugUtil.h>

// Get right serial port for debugging
#ifdef __AVR_ATmega32U4__
    // Leonardo/Micro/ProMicro use the USB serial port
    #define DEBUGSERIAL Serial
#elif __SAMD21G18A__ 
    // Zero use native USB port
    #define DEBUGSERIAL SerialUSB
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


// ################################################
// ### KONNEKTING Configuration
// ################################################
#ifdef __AVR_ATmega328P__
#define KNX_SERIAL Serial // Nano/ProMini etc. use Serial
#elif ESP8266
#define KNX_SERIAL Serial // ESP8266 use Serial
#else
#define KNX_SERIAL Serial1 // Leonardo/Micro/Zero etc. use Serial1
#endif

// ################################################
// ### IO Configuration
// ################################################
#define PROG_LED_PIN 9
#define PROG_BUTTON_PIN 3
#define TEST_LED 5 //or change it to another pin

    
// ################################################
// ### Global variables, sketch related
// ################################################
unsigned long diffmillis = 0;
unsigned long lastmillis = millis();
int laststate = false;


// ################################################
// ### Optional (HW dependant): EEPROM access
// ### Only required if HW does not support 
// ### internal eeprom, like Arduino Zero
// ################################################
int readEeprom(int index) {
    // when using external storage, implement READ command here
    //return EEPROM.read(index);
    return 0;
}

void writeEeprom(int index, int val) {
    // when using external storage, implement WRITE command here
    //return EEPROM.write(index, val);
    return;
}

void updateEeprom(int index, int val) {
    // when using external storage, implement UPDATE command here
    //return EEPROM.update(index, val);
}


// ################################################
// ### SETUP
// ################################################
void setup() {

// debug related stuff
#ifdef DEBUG
    
    // Start debug serial with 9600 bauds
    DEBUGSERIAL.begin(9600);
    
#if defined(__AVR_ATmega32U4__) || (__SAMD21G18A__)
    // wait for serial port to connect. Needed for Leonardo/Micro/ProMicro/Zero only
    while (!DEBUGSERIAL)
#endif
        
    // make debug serial port known to debug class
    // Means: KONNEKTING will sue the same serial port for console debugging
    Debug.setPrintStream(&DEBUGSERIAL);
#endif
    
    Debug.print(F("KONNEKTING DemoSketch\n"));
    
    pinMode(TEST_LED, OUTPUT);  

    /*
     * Only required when using external eeprom (or similar) storage.
     * function pointers should match the methods you have implemented above.
     * If no external eeprom required, please remove all three Konnekting.setEeprom* lines below
     */
#ifdef __SAMD21G18A__    
    Konnekting.setEepromReadFunc(&readEeprom);
    Konnekting.setEepromWriteFunc(&writeEeprom);
    Konnekting.setEepromUpdateFunc(&updateEeprom);
#endif    
    
    // Initialize KNX enabled Arduino Board
    Konnekting.init(KNX_SERIAL,
            PROG_BUTTON_PIN,
            PROG_LED_PIN,
            MANUFACTURER_ID,
            DEVICE_ID,
            REVISION);
    
    // If device has been parametrized with KONNEKTING Suite, read params from eeprom
    // Otherwise contibue with global default values from sketch
    if (!Konnekting.isFactorySetting()){
        diffmillis = (int) Konnekting.getUINT16Param(PARAM_initialDelay); //blink every xxxx ms
    }
    
    lastmillis = millis();

    Debug.println(F("Toggle every %d ms."), diffmillis);
    Debug.println(F("Setup is ready. go to loop..."));
}

// ################################################
// ### LOOP
// ################################################
void loop() {
    
    // Do KNX related stuff (like sending/receiving KNX telegrams)
    // This is required in every KONNEKTING aplication sketch
    Knx.task();
    
    unsigned long currentmillis = millis();
    
    /*
     * only do measurements and other sketch related stuff if not in programming mode
     * means: only when konnekting is ready for appliction
     */
    if (Konnekting.isReadyForApplication()) {
        
        if (currentmillis - lastmillis >= diffmillis) {
            
            Debug.println(F("Actual state: %d"), laststate);
            Knx.write(COMOBJ_commObj2, laststate);
            laststate = !laststate;
            lastmillis = currentmillis;
            
            digitalWrite(TEST_LED, HIGH);
            Debug.println(F("DONE"));
            
        }
        
    }
    
}

// ################################################
// ### KNX EVENT CALLBACK
// ################################################

void knxEvents(byte index) {
    // nothing to do in this sketch
    switch (index) {

        case COMOBJ_commObj1: // object index has been updated

            if (Knx.read(COMOBJ_commObj1)) {
                digitalWrite(TEST_LED, HIGH);
                Debug.println(F("Toggle LED: on"));
            } else {
                digitalWrite(TEST_LED, LOW);
                Debug.println(F("Toggle LED: off"));
            }
            break;

        default:
            break;
    }
};

