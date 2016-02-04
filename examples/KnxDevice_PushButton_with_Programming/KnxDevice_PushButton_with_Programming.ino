// comment following line to disable DEBUG mode
#define DEBUG debugSerial

// no need to comment, you can leave it as it is as long you do not change the "#define DEBUG debugSerial" line
#ifdef DEBUG
#include <SoftwareSerial.h>
SoftwareSerial debugSerial(10, 11); // RX, TX
#endif

// include KnxDevice library
#include <KnxDevice.h>


// define programming-led PIN
#ifdef ESP8266
#define PROG_LED_PIN BUILTIN_LED // ESP8266 uses wrong constant. See PR: https://github.com/esp8266/Arduino/pull/1556
#else
#define PROG_LED_PIN LED_BUILTIN  // defaults to on-board LED for AVR Arduinos
#endif

// define programming-button PIN
#define PROG_BUTTON_PIN 3

// define KNX Transceiver serial port
#ifdef __AVR_ATmega328P__
#define KNX_SERIAL Serial // Nano/ProMini etc. use Serial
#else
#define KNX_SERIAL Serial1 // Leonardo/Micro etc. use Serial1
#endif


// Definition of the Communication Objects attached to the device
KnxComObject KnxDevice::_comObjectsList[] = {
    /* needs to be there, as long as Tools is used */ Tools.createProgComObject(),
    /* Index 1 : */ KnxComObject(G_ADDR(0, 0, 1), KNX_DPT_1_001, COM_OBJ_LOGIC_IN),
    /* Index 2 : */ KnxComObject(G_ADDR(0, 0, 2), KNX_DPT_1_001, COM_OBJ_SENSOR),
};
const byte KnxDevice::_numberOfComObjects = sizeof (_comObjectsList) / sizeof (KnxComObject); // do no change this code

// Definition of parameter size
byte KnxTools::_paramSizeList[] = {
    /* Param Index 0 */ PARAM_UINT8,
    /* Param Index 1 */ PARAM_INT16,
    /* Param Index 2 */ PARAM_UINT32,
    /* Param Index 3 */ PARAM_UINT8,
    /* Param Index 4 */ PARAM_UINT8,
    /* Param Index 5 */ PARAM_UINT8,
    /* Param Index 6 */ PARAM_UINT8,
    /* Param Index 7 */ PARAM_UINT8,
    /* Param Index 8 */ PARAM_UINT8,
    /* Param Index 9 */ PARAM_UINT8,
};
const byte KnxTools::_numberOfParams = sizeof (_paramSizeList); // do no change this code

bool state = false;

// Callback function to handle com objects updates
void knxEvents(byte index) {
        
    // toggle led state, just for "visual testing"
    state = !state;

    if (state) {
        digitalWrite(PROG_LED_PIN, HIGH);
        Knx.write(2, true );
    } else {
        digitalWrite(PROG_LED_PIN, LOW);
        Knx.write(2, false );
    }
    
};

void setup() {

// if debug mode is enabled, setup serial port with 9600 baud    
#ifdef DEBUG
    DEBUG.begin(9600);
#endif
    
    // set well defined state for LED pin for this special sample. Can be skipped in other sketches.
    pinMode(PROG_LED_PIN, OUTPUT);
    digitalWrite(PROG_LED_PIN, LOW);
    
    // Initialize KNX enabled Arduino Board
    Tools.init(/* KNX transceiver serial port */ KNX_SERIAL, 
            /* Prog Button Pin */ PROG_BUTTON_PIN, 
            /* Prog LED Pin */ PROG_LED_PIN, 
            /* manufacturer */ 57005, 
            /* device */ 190, 
            /* revision */175);
    
#ifdef DEBUG
    DEBUG.print("param #0: ");
    DEBUG.print(Tools.getUINT8Param(0));
    DEBUG.println("");
    
    DEBUG.print("param #1: ");
    DEBUG.print(Tools.getINT16Param(1));
    DEBUG.println("");
    
    DEBUG.print("param #2: ");
    DEBUG.print(Tools.getUINT32Param(2));
    DEBUG.println("");
#endif    

    // setup GPIOs here!
}

void loop() {
    Knx.task();
}