#define KONNEKTING_SYSTEM_TYPE_SIMPLE
#include <KonnektingDevice.h>

// include device related configuration code, created by "KONNEKTING CodeGenerator"
#include "kdevice_DemoSketch.h"

// ################################################
// ### BOARD CONFIGURATION
// ################################################

#ifdef __AVR_ATmega328P__
// Uno/Nano/ProMini
#define KNX_SERIAL Serial    // D0=RX/D1=TX
#define PROG_LED_PIN 11      // External LED
#define TEST_LED LED_BUILTIN // On board LED on pin D13
#define PROG_BUTTON_PIN 3    // pin with interrupt
// ATmega328P has only one UART, lets use virtual one
#include <SoftwareSerial.h>
SoftwareSerial softserial(11, 10); // D11=RX/D10=TX
#define DEBUGSERIAL softserial

#elif __AVR_ATmega32U4__
// Leonardo/Micro/ProMicro
#define KNX_SERIAL Serial1   // D0=RX/D1=TX
#define PROG_LED_PIN 11      // External LED
#define TEST_LED LED_BUILTIN // On board LED on pin D13
#define PROG_BUTTON_PIN 3    // pin with interrupt
#define DEBUGSERIAL Serial   // USB port

#elif ARDUINO_ARCH_SAMD
// Zero/M0
#define KNX_SERIAL Serial1    // D0=RX/D1=TX
#define PROG_LED_PIN 11       // External LED
#define TEST_LED LED_BUILTIN  // On board LED on pin D13
#define PROG_BUTTON_PIN 3     // pin with interrupt
#define DEBUGSERIAL SerialUSB // USB port
// Arduino Zero has no "real" internal EEPROM,
// so we can use an external I2C EEPROM.
#include "EEPROM_24AA256.h"   // external EEPROM

#elif ARDUINO_ARCH_STM32
// STM32 NUCLEO Boards
// Option Serial interface: "Enabled with generic Serial" should be enabled
// Create a new Serial on Pins PA10 & PA9
// Arduino-Header: D2(PA10)=RX/D8(PA9)=TX
HardwareSerial Serial1(PA10, PA9);
#define KNX_SERIAL Serial1
#define PROG_LED_PIN 11          // External LED
#define TEST_LED LED_BUILTIN     // On board LED on pin D13
#define PROG_BUTTON_PIN USER_BTN // On board button
#define DEBUGSERIAL Serial       // USB port

#elif ESP8266
// ESP8266
#define KNX_SERIAL Serial   // swaped Serial on D7(GPIO13)=RX/GPIO15(D8)=TX
#define PROG_LED_PIN 14     // External LED
#define TEST_LED 16         // External LED
#define PROG_BUTTON_PIN 0   // Flash/IO0 button
#define DEBUGSERIAL Serial1 // the 2nd serial port with TX only (GPIO2/D4)

#else
// All other boards
#error "Sorry, you board is not supported"
#endif

// ################################################
// ### DEBUG CONFIGURATION
// ################################################
//#define KDEBUG // comment this line to disable DEBUG mode
#ifdef KDEBUG
#include <DebugUtil.h>
#endif

// ################################################
// ### Global variables, sketch related
// ################################################
unsigned long blinkDelay = 2500; // default value
unsigned long lastmillis = millis();
int laststate = false;

// ################################################
// ### SETUP
// ################################################

void setup()
{

    // debug related stuff
#ifdef KDEBUG

    // Start debug serial with 115200 bauds
    DEBUGSERIAL.begin(115200);

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
    if (!Konnekting.isFactorySetting())
    {
        blinkDelay = (int)Konnekting.getUINT16Param(PARAM_blinkDelay); //blink every xxxx ms
    }

    lastmillis = millis();

    Debug.println(F("Toggle LED every %d ms."), blinkDelay);
    Debug.println(F("Setup is ready. go to loop..."));
}

// ################################################
// ### LOOP
// ################################################

void loop()
{

    // Do KNX related stuff (like sending/receiving KNX telegrams)
    // This is required in every KONNEKTING aplication sketch
    Knx.task();

    unsigned long currentmillis = millis();

    /*
     * only do measurements and other sketch related stuff if not in programming mode
     * means: only when konnekting is ready for appliction
     */
    if (Konnekting.isReadyForApplication())
    {

        if (currentmillis - lastmillis >= blinkDelay)
        {

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

void knxEvents(byte index)
{
    switch (index)
    {

    case COMOBJ_ledOnOff: // object index has been updated

        if (Knx.read(COMOBJ_ledOnOff))
        {
            digitalWrite(TEST_LED, HIGH);
            Debug.println(F("Toggle LED: on"));
        }
        else
        {
            digitalWrite(TEST_LED, LOW);
            Debug.println(F("Toggle LED: off"));
        }
        break;

    default:
        break;
    }
};
