#include <KnxDevice.h>

#ifdef ARDUINO_ARCH_ESP8266
// fake LED-BUILTIN for ESP8266
#define LED_BUILTIN 16
#endif

int greenPin = LED_BUILTIN; // onboard LED


// Definition of the Communication Objects attached to the device
KnxComObject KnxDevice::_comObjectsList[] = {
    /* needs to be there, as long as Tools is used */ Tools.createProgComObject(),
    /* Index 1 : */ KnxComObject(G_ADDR(0, 0, 1), KNX_DPT_1_001, COM_OBJ_LOGIC_IN),
    /* Index 2 : */ KnxComObject(G_ADDR(0, 0, 2), KNX_DPT_1_001, COM_OBJ_SENSOR),
};
const byte KnxDevice::_comObjectsNb = sizeof (_comObjectsList) / sizeof (KnxComObject); // do no change this code

// Definition of parameter size
byte KnxTools::_paramLenghtList[] = {
    /* Param Index 0 */ 1,
    /* Param Index 1 */ 2,
    /* Param Index 2 */ 4,
};
const byte KnxTools::_paramsNb = sizeof (_paramLenghtList); // do no change this code

bool state = false;

// Callback function to handle com objects updates
void knxEvents(byte index) {
        
    // toggle led state, just for "visual testing"
    state = !state;

    if (state) {
        digitalWrite(greenPin, HIGH);
        Knx.write(2, true );
    } else {
        digitalWrite(greenPin, LOW);
        Knx.write(2, false );
    }
};

void setup() {
    
//    Serial.begin(9600);
//    while (!Serial) {
//        ; // wait for serial port to connect. Needed for Leonardo only
//    }  
//    Serial.println("Hello Computer");
////    Serial.println("Hello Computer1");
    
    
    pinMode(greenPin, OUTPUT);
    digitalWrite(greenPin, LOW);

    // <Device ManufacturerId="57005" DeviceId="190" Revision="175">
    
    Tools.init(/* TPUART serial port */ Serial1, 
            /* Prog Button Pin */ 3, 
            /* Prog LED Pin */ LED_BUILTIN, 
            /* manufacturer */ 57005, 
            /* device */ 190, 
            /* revision */175);
    
    // get parameter value for param #1
    byte paramValue[Tools.getParamSize(1)];
    Tools.getParamValue(1, paramValue);
    // --> value is now available in 'paramValue'
//    Serial.println("Hello Computer1b");
}

void loop() {
    Knx.task();
}
