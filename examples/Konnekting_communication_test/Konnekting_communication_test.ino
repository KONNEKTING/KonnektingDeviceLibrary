// comment following line to disable DEBUG mode
#define DEBUG debugSerial

// no need to comment, you can leave it as it is as long you do not change the "#define DEBUG debugSerial" line
#ifdef DEBUG
#include <SoftwareSerial.h>
//SoftwareSerial debugSerial(10, 16); // RX, TX for ProMicro, pls. change PINs in KnxTools.cpp Line 47 
SoftwareSerial debugSerial(10, 11); // RX, TX for all others
#endif

// include KnxDevice library
#include <KnxDevice.h>

// defaults to on-board LED for AVR Arduinos
#define PROG_LED_PIN 9

// defaults to on-board LED for AVR Arduinos
#define testLED PROG_LED_PIN //or change it to another pin

// define programming-button PIN
#define PROG_BUTTON_PIN 2

//(use knx_address_calculator.html to calculate your own address/GA)
//define hardcoded address 1.1.199 
byte hiAddr = 0x11;
byte loAddr = 0xc7;
//define hardcoded listen GA 0/0/1 
byte hiGA1 = 0x00;
byte loGA1 = 0x01;
//define hardcoded LED toggle GA 0/0/2
byte hiGA2 = 0x00;
byte loGA2 = 0x02;


// Define KONNEKTING Device related IDs
#define MANUFACTURER_ID 57005
#define DEVICE_ID 255
#define REVISION 0

// define KNX Transceiver serial port
#ifdef __AVR_ATmega328P__
#define KNX_SERIAL Serial // Nano/ProMini etc. use Serial
#else
#define KNX_SERIAL Serial1 // Leonardo/Micro etc. use Serial1
#endif

// Definition of the Communication Objects attached to the device
KnxComObject KnxDevice::_comObjectsList[] = {
    /* don't change this */ Tools.createProgComObject(),
                            
    // Currently, Sketch Index and Suite Index differ for ComObjects :-(
                            
    /* Sketch-Index 1, Suite-Index 0 : */ KnxComObject(KNX_DPT_1_001, COM_OBJ_LOGIC_IN),
    /* Sketch-Index 2, Suite-Index 1 : */ KnxComObject(KNX_DPT_1_001, COM_OBJ_SENSOR),
};
const byte KnxDevice::_numberOfComObjects = sizeof (_comObjectsList) / sizeof (KnxComObject); // do no change this code

// Definition of parameter size
byte KnxTools::_paramSizeList[] = {
    
    // For params, the index in Sketch and Suite is equal
    
    /* Param Index 0 */ PARAM_UINT16
};
const byte KnxTools::_numberOfParams = sizeof (_paramSizeList); // do no change this code

unsigned long diffmillis = 0;
unsigned long lastmillis = millis(); 
int laststate = false;


// Callback function to handle com objects updates

void knxEvents(byte index) {
    // nothing to do in this sketch
    switch (index)
  {
    case 1 : // object index 1 has been updaed
      if (Knx.read(1)) { 
          digitalWrite(testLED, HIGH);  
#ifdef DEBUG  
          DEBUG.println("Toggle LED: on");
#endif
      } // red led on,  
      else { 
          digitalWrite(testLED, LOW); 
#ifdef DEBUG  
          DEBUG.println("Toggle LED: off");
#endif
      } // red led off, 
          break;

    default: 
          break;      
  }
};

void setup() {

//write hardcoded PA and GAs
EEPROM.write(0, hiAddr);  //Set "not factory" flag
EEPROM.write(1, hiAddr);  //hiAddr
EEPROM.write(2, loAddr);  //loAddr
EEPROM.write(10, hiGA1); //hi GA1
EEPROM.write(11, loGA1); //lo GA1
EEPROM.write(12, 0x80);  //activate GA1
EEPROM.write(13, hiGA2); //hi GA2
EEPROM.write(14, loGA2); //lo GA2
EEPROM.write(15, 0x80);  //activate GA2


    // if debug mode is enabled, setup serial port with 9600 baud    
#ifdef DEBUG
    DEBUG.begin(9600);
#endif
    // Initialize KNX enabled Arduino Board
    Tools.init(KNX_SERIAL, PROG_BUTTON_PIN, PROG_LED_PIN, MANUFACTURER_ID, DEVICE_ID, REVISION);

diffmillis = 1000; //blink every xxxx ms

#ifdef DEBUG 
        DEBUG.print("Toggle every ");
        DEBUG.print(diffmillis);
        DEBUG.println(" ms");
        DEBUG.println("Setup is ready. go to loop...");
#endif

}

void loop() {
    Knx.task();
    unsigned long currentmillis = millis();    
    // only do measurements and other sketch related stuff if not in programming mode
    if (!Tools.getProgState()) {
        if (currentmillis - lastmillis >= diffmillis){
#ifdef DEBUG  
        DEBUG.print("Actual state: ");
        DEBUG.println(laststate);
#endif
            Knx.write(2,laststate);
            laststate = !laststate;
            lastmillis = currentmillis;
        }
    }
}
