#include <KnxDevice.h>

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
    
    pinMode(greenPin, OUTPUT);
    digitalWrite(greenPin, LOW);

    Tools.init(/* TPUART serial port */ Serial1, 
            /* Prog Button Pin */ 3, 
            /* Prog LED Pin */ LED_BUILTIN, 
            /* manufacturer */ 0x0000, 
            /* device */ 0x00, 
            /* revision */0x00);
    
    // get parameter value for param #1
    byte paramValue[Tools.getParamSize(1)];
    Tools.getParamValue(1, paramValue);
    // --> value is now available in 'paramValue'
    
}

void loop() {
    Knx.task();
}
