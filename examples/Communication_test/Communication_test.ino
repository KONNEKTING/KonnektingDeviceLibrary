#include <KonnektingDevice.h>

// ################################################
// ### KNX DEVICE CONFIGURATION
// ################################################
word individualAddress = P_ADDR(1, 1, 199);
word groupAddressInput = G_ADDR(7, 7, 7);
word groupAddressOutput = G_ADDR(7, 7, 8);

// ################################################
// ### BOARD CONFIGURATION
// ################################################

#ifdef __AVR_ATmega328P__
// Uno/Nano/ProMini
#define KNX_SERIAL Serial    // D0=RX/D1=TX
#define TEST_LED LED_BUILTIN // On board LED on pin D13
// ATmega328P has only one UART, lets use virtual one
#include <SoftwareSerial.h>
SoftwareSerial softserial(11, 10); // D11=RX/D10=TX
#define DEBUGSERIAL softserial

#elif __AVR_ATmega32U4__
// Leonardo/Micro/ProMicro
#define KNX_SERIAL Serial1   // D0=RX/D1=TX
#define TEST_LED LED_BUILTIN // On board LED on pin D13
#define DEBUGSERIAL Serial   // USB port

#elif ARDUINO_ARCH_SAMD 
// Zero/M0
#define KNX_SERIAL Serial1    // D0=RX/D1=TX
#define TEST_LED LED_BUILTIN  // On board LED on pin D13
#define DEBUGSERIAL SerialUSB // USB port

#elif ARDUINO_ARCH_STM32
// STM32 NUCLEO Boards
// Option Serial interface: "Enabled with generic Serial" should be enabled
// Create a new Serial on Pins PA10 & PA9
// Arduino-Header: D2(PA10)=RX/D8(PA9)=TX
HardwareSerial Serial1(PA10,PA9);
#define KNX_SERIAL Serial1
#define TEST_LED LED_BUILTIN // On board LED
#define DEBUGSERIAL Serial   // USB port

#elif ESP8266
// ESP8266
#define KNX_SERIAL Serial   // swaped Serial on D7(GPIO13)=RX/GPIO15(D8)=TX
#define TEST_LED 16         // External LED
#define DEBUGSERIAL Serial1 // the 2nd serial port with TX only (GPIO2/D4)

#else
// All other boards
#error "Sorry, you board is not supported"
#endif

// ################################################
// ### DEBUG Configuration
// ################################################
#define KDEBUG // comment this line to disable DEBUG mode

#ifdef KDEBUG
#include <DebugUtil.h>
#endif

// Define KONNEKTING Device related IDs
#define MANUFACTURER_ID 57005
#define DEVICE_ID 255
#define REVISION 0


// Definition of the Communication Objects attached to the device
KnxComObject KnxDevice::_comObjectsList[] = {
    /* Suite-Index 0 : */ KnxComObject(KNX_DPT_1_001, COM_OBJ_LOGIC_IN),
    /* Suite-Index 1 : */ KnxComObject(KNX_DPT_1_001, COM_OBJ_SENSOR),
};
const byte KnxDevice::_numberOfComObjects = sizeof (_comObjectsList) / sizeof (KnxComObject); // do no change this code

// Definition of parameter size
byte KonnektingDevice::_paramSizeList[] = {
    /* Param Index 0 */ PARAM_UINT16
};
const int KonnektingDevice::_numberOfParams = sizeof (_paramSizeList); // do no change this code

//we do not need a ProgLED, but this function muss be defined
void progLed (bool state){ 
    //nothing to do here
    Debug.println(F("Toggle ProgLED, actual state: %d"),state);
}

unsigned long sendDelay = 2000;
unsigned long lastmillis = millis(); 
bool lastState = false;
bool lastLedState = false;
byte virtualEEPROM[16];


// ################################################
// ### KNX EVENT CALLBACK
// ################################################

void knxEvents(byte index) {
    switch (index) {

        case 0: // object index has been updated
            Debug.println(F("ComObject 0 received: %d, let's trigger LED state"), Knx.read(0));
            lastLedState = !lastLedState;
            digitalWrite(TEST_LED,lastLedState);
            break;

        default:
            break;
    }
}

byte readMemory(int index){
    return virtualEEPROM[index];
}

void writeMemory(int index, byte val) {
    virtualEEPROM[index] = val;
}

void updateMemory(int index, byte val) {
    if (readMemory(index) != val) {
        writeMemory(index, val);
    }
}

void commitMemory() {

}

void setup() {
    memset(virtualEEPROM,0xFF,16);
    Konnekting.setMemoryReadFunc(&readMemory);
    Konnekting.setMemoryWriteFunc(&writeMemory);
    Konnekting.setMemoryUpdateFunc(&updateMemory);
    Konnekting.setMemoryCommitFunc(&commitMemory);

    // simulating allready programmed Konnekting device:
    // in this case we don't need Konnekting Suite
    // write hardcoded PA and GAs
    writeMemory(0,  0x7F);  //Set "not factory" flag
    writeMemory(1,  (byte)(individualAddress >> 8));
    writeMemory(2,  (byte)(individualAddress));
    writeMemory(10, (byte)(groupAddressInput >> 8));
    writeMemory(11, (byte)(groupAddressInput));
    writeMemory(12, 0x80);  //activate InputGA
    writeMemory(13, (byte)(groupAddressOutput >> 8));
    writeMemory(14, (byte)(groupAddressOutput));
    writeMemory(15, 0x80);  //activate OutputGA


    // debug related stuff
#ifdef KDEBUG

    // Start debug serial with 115200 bauds
    DEBUGSERIAL.begin(115200);

    //waiting 3 seconds, so you have enough time to start serial monitor
    delay(3000);

    // make debug serial port known to debug class
    // Means: KONNEKTING will use the same serial port for console debugging
    Debug.setPrintStream(&DEBUGSERIAL);
#endif

    // Initialize KNX enabled Arduino Board
    Konnekting.init(KNX_SERIAL,
            &progLed,
            MANUFACTURER_ID,
            DEVICE_ID,
            REVISION);

    pinMode(TEST_LED,OUTPUT);
    
    Debug.println(F("Toggle LED every %d ms."), sendDelay);
    Debug.println(F("Setup is ready. Turning LED on and going to loop..."));

}

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

        if (currentmillis - lastmillis >= sendDelay) {

            Debug.println(F("Sending: %d"), lastState);
            Knx.write(1, lastState);
            lastState = !lastState;
            lastmillis = currentmillis;
            Debug.println(F("DONE\n"));

        }

    }

}