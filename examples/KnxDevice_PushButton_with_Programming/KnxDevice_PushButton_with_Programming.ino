#include <KnxDevice.h>

#ifdef ESP8266
int progLedPin = BUILTIN_LED; // ESP8266 uses wrong constant. See PR: https://github.com/esp8266/Arduino/pull/1556
#else
int progLedPin = LED_BUILTIN; 
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
};
const byte KnxTools::_numberOfParams = sizeof (_paramSizeList); // do no change this code

bool state = false;

// Callback function to handle com objects updates
void knxEvents(byte index) {
        
    // toggle led state, just for "visual testing"
    state = !state;

    if (state) {
        digitalWrite(progLedPin, HIGH);
        Knx.write(2, true );
    } else {
        digitalWrite(progLedPin, LOW);
        Knx.write(2, false );
    }
    
};

void setup() {
    
    pinMode(progLedPin, OUTPUT);
    digitalWrite(progLedPin, LOW);

    // <Device ManufacturerId="57005" DeviceId="190" Revision="175">
    
    Tools.init(/* TPUART serial port */ Serial, /* Nano/ProMini use 'Serial', Leonardo/Micro use 'Serial1'*/
            /* Prog Button Pin */ 3, 
            /* Prog LED Pin */ progLedPin, 
            /* manufacturer */ 57005, 
            /* device */ 190, 
            /* revision */175);
    
    // get parameter value for param #1
    byte paramValue[Tools.getParamSize(1)];
    Tools.getParamValue(1, paramValue);
    // --> value is now available in 'paramValue'

    // setup GPIOs here!
}

void loop() {
    Knx.task();
}