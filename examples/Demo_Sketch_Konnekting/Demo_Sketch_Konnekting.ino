#include <KnxDevice.h>

// ################################################
// ### DEBUG CONFIGURATION
// ################################################
#define DEBUG
#include <KonnektingDebug.h>
// -> Hardware Debug Serial: For Leonardo/Micro/...
//#define DEBUGCONFIG_DEVICE serial
// -> Software Debug Serial
#define DEBUGCONFIG_SOFTWARESERIAL
#define DEBUGCONFIG_RX_PIN 11
#define DEBUGCONFIG_TX_PIN 10
#define DEBUGCONFIG_BAUDS 9600



// ################################################
// ### KONNEKTING Configuration
// ################################################
#ifdef __AVR_ATmega328P__
    #define KNX_SERIAL Serial // Nano/ProMini etc. use Serial
#else
    #define KNX_SERIAL Serial1 // Leonardo/Micro etc. use Serial1
#endif
#define MANUFACTURER_ID 57005
#define DEVICE_ID 255
#define REVISION 0
#define PROG_LED_PIN 9
#define PROG_BUTTON_PIN 2



// ################################################
// ### KONNEKTING ComObjects and Parameters
// ################################################
KnxComObject KnxDevice::_comObjectsList[] = {
    /* don't change this */ Konnekting.createProgComObject(),
                            
    // Currently, Sketch Index and Suite Index differ for ComObjects :-(
                            
    /* Sketch-Index 1, Suite-Index 0 : */ KnxComObject(KNX_DPT_1_001, COM_OBJ_LOGIC_IN),
    /* Sketch-Index 2, Suite-Index 1 : */ KnxComObject(KNX_DPT_1_001, COM_OBJ_SENSOR),
};
const byte KnxDevice::_numberOfComObjects = sizeof (_comObjectsList) / sizeof (KnxComObject); // do no change this code

// Definition of parameter size
byte KonnektingDevice::_paramSizeList[] = {
    
    // For params, the index in Sketch and Suite is equal
    
    /* Param Index 0 */ PARAM_UINT16
};
const byte KonnektingDevice::_numberOfParams = sizeof (_paramSizeList); // do no change this code





// ################################################
// ### Sketch Configuration & Variables
// ################################################
#define TEST_LED PROG_LED_PIN //or change it to another pin

unsigned long diffmillis = 0;
unsigned long lastmillis = millis();
int laststate = false;



// ################################################
// ### SETUP
// ################################################
void setup() {

    //DebugInit();
    __DEBUG_SERIAL = Serial;
    
    __DEBUG_SERIAL.println("huhu");
    DEBUG_PRINTLN("HUHU");

    // Initialize KNX enabled Arduino Board
    Konnekting.init(KNX_SERIAL, 
                    PROG_BUTTON_PIN, 
                    PROG_LED_PIN, 
                    MANUFACTURER_ID, 
                    DEVICE_ID, 
                    REVISION);

    diffmillis = (int) Konnekting.getUINT16Param(0); //blink every xxxx ms

    DEBUG_PRINT("Toggle every ");
    DEBUG_PRINT(diffmillis);
    DEBUG_PRINTLN(" ms");
    DEBUG_PRINTLN("Setup is ready. go to loop...");
}

// ################################################
// ### LOOP
// ################################################
void loop() {
    Knx.task();
    unsigned long currentmillis = millis();
    // only do measurements and other sketch related stuff if not in programming mode
    if (Konnekting.isReadyForApplication()) {
        if (currentmillis - lastmillis >= diffmillis) {
            DEBUG_PRINT("Actual state: ");
            DEBUG_PRINTLN(laststate);
            Knx.write(2, laststate);
            laststate = !laststate;
            lastmillis = currentmillis;
        }
    }
}

// ################################################
// ### KNX EVENT CALLBACK
// ################################################
void knxEvents(byte index) {
    // nothing to do in this sketch
    switch (index) {
        
        case 1: // object index 1 has been updated
            if (Knx.read(1)) {
                digitalWrite(TEST_LED, HIGH);
                DEBUG_PRINTLN("Toggle LED: on");
            }// led on,  
            else {
                digitalWrite(TEST_LED, LOW);
                DEBUG_PRINTLN("Toggle LED: off");
            } // led off, 
            break;

        default:
            break;
    }
};

