#include <KonnektingDevice.h>

// include device related configuration code, created by "KONNEKTING CodeGenerator"
#include "kdevice_DemoSketch.h"

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
unsigned long blinkDelay = 2500; // default value
unsigned long lastmillis = millis();
int laststate = false;


// ################################################
// ### Optional (HW dependant): Memory access
// ### Only required if HW does not support 
// ### internal eeprom, like Arduino Zero
// ################################################

// Arduino Zero has no "real" internal EEPROM, 
// so we can use an external I2C EEPROM.
// For that we need to include Arduino's Wire.h
#ifdef __SAMD21G18A__
#include <Wire.h>

// Example for 24AA02E64 I2C EEPROM
byte readMemory(int index) {
    byte rdata = 0xFF;

    Wire.beginTransmission(0x50);
    Wire.write((int) (index));
    Wire.endTransmission();

    Wire.requestFrom(0x50, 1);

    if (Wire.available()) {
        rdata = Wire.read();
    }

    return rdata;
}

void writeMemory(int index, byte val) {
    Wire.beginTransmission(0x50);
    Wire.write((int) (index));
    Wire.write(val);
    Wire.endTransmission();

    delay(5); //is it needed?!
}

void updateMemory(int index, byte val) {
    if (readMemory(index) != val) {
        writeMemory(index, val);
    }
}

void commitMemory() {
    // this is useful if using flash memory to reduce write cycles
    // EEPROM needs no commit, so this function is intentionally left blank 
}
#endif


// ################################################
// ### SETUP
// ################################################

void setup() {

    // debug related stuff
#ifdef KDEBUG

    // Start debug serial with 9600 bauds
    DEBUGSERIAL.begin(9600);

#if defined(__AVR_ATmega32U4__) || defined(__SAMD21G18A__)
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
     * If no external eeprom required, please remove all three Konnekting.setMemory* lines below
     */
#ifdef __SAMD21G18A__
    Wire.begin();
    Konnekting.setMemoryReadFunc(&readMemory);
    Konnekting.setMemoryWriteFunc(&writeMemory);
    Konnekting.setMemoryUpdateFunc(&updateMemory);
    Konnekting.setMemoryCommitFunc(&commitMemory);
#endif    

    // Initialize KNX enabled Arduino Board
    Konnekting.init(KNX_SERIAL,
            PROG_BUTTON_PIN,
            PROG_LED_PIN,
            MANUFACTURER_ID,
            DEVICE_ID,
            REVISION);

    // If device has been parametrized with KONNEKTING Suite, read params from EEPROM
    // Otherwise continue with global default values from sketch
    if (!Konnekting.isFactorySetting()) {
        blinkDelay = (int) Konnekting.getUINT16Param(PARAM_blinkDelay); //blink every xxxx ms
    }

    lastmillis = millis();

    Debug.println(F("Toggle LED every %d ms."), blinkDelay);
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

        if (currentmillis - lastmillis >= blinkDelay) {

            Debug.println(F("Actual state: %d"), laststate);
            Knx.write(COMOBJ_trigger, laststate);
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

        case COMOBJ_ledOnOff: // object index has been updated

            if (Knx.read(COMOBJ_ledOnOff)) {
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

