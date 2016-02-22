// comment following line to disable DEBUG mode
//#define DEBUG debugSerial

// no need to comment, you can leave it as it is as long you do not change the "#define DEBUG debugSerial" line
#ifdef DEBUG
#include <SoftwareSerial.h>
SoftwareSerial debugSerial(10, 11); // RX, TX
#endif

// include KnxDevice library
#include <KnxDevice.h>
#include <Wire.h>
#include "SparkFunHTU21D.h" //https://github.com/sparkfun/SparkFun_HTU21D_Breakout_Arduino_Library

#define PROG_LED_PIN LED_BUILTIN  // defaults to on-board LED for AVR Arduinos

// define programming-button PIN
#define PROG_BUTTON_PIN 3

// define KNX Transceiver serial port
#ifdef __AVR_ATmega328P__
#define KNX_SERIAL Serial // Nano/ProMini etc. use Serial
#else
#define KNX_SERIAL Serial1 // Leonardo/Micro etc. use Serial1
#endif
//Create an instance of the object for HTU-Sensor
HTU21D htu;

// Definition of the Communication Objects attached to the device
KnxComObject KnxDevice::_comObjectsList[] = {
    /* needs to be there, as long as Tools is used */ Tools.createProgComObject(),
    /* Index 1 : */ KnxComObject(G_ADDR(0, 0, 1), KNX_DPT_9_001, COM_OBJ_SENSOR),
    /* Index 2 : */ KnxComObject(G_ADDR(0, 0, 2), KNX_DPT_1_001, COM_OBJ_SENSOR),
    /* Index 3 : */ KnxComObject(G_ADDR(0, 0, 3), KNX_DPT_1_001, COM_OBJ_SENSOR),
    /* Index 4 : */ KnxComObject(G_ADDR(0, 0, 4), KNX_DPT_9_007, COM_OBJ_SENSOR),
    /* Index 5 : */ KnxComObject(G_ADDR(0, 0, 5), KNX_DPT_1_001, COM_OBJ_SENSOR),
    /* Index 6 : */ KnxComObject(G_ADDR(0, 0, 6), KNX_DPT_1_001, COM_OBJ_SENSOR),
};
const byte KnxDevice::_numberOfComObjects = sizeof (_comObjectsList) / sizeof (KnxComObject); // do no change this code

// Definition of parameter size
byte KnxTools::_paramSizeList[] = {
    /* Param Index 0 */ PARAM_UINT8,
    /* Param Index 1 */ PARAM_UINT8,
    /* Param Index 2 */ PARAM_UINT32,
    /* Param Index 3 */ PARAM_UINT8,
    /* Param Index 4 */ PARAM_UINT8,
    /* Param Index 5 */ PARAM_INT16,
    /* Param Index 6 */ PARAM_UINT8,
    /* Param Index 7 */ PARAM_INT16,
    /* Param Index 8 */ PARAM_UINT8,
    /* Param Index 9 */ PARAM_UINT32,
    /* Param Index 10 */ PARAM_UINT8,
    /* Param Index 11 */ PARAM_UINT8,
    /* Param Index 12 */ PARAM_INT16,
    /* Param Index 13 */ PARAM_UINT8,
    /* Param Index 14 */ PARAM_INT16
};
const byte KnxTools::_numberOfParams = sizeof (_paramSizeList); // do no change this code

unsigned long previousTimeTemp = 0;
unsigned long previousTimeHumd = 0;

float previousTemp = 0;
float previousHumd = 0;
float currentTemp = 0;
float currentHumd = 0;

int typeTemp;
long intervalTempUser; //typical temperatur polling interval (ms)
float diffTempUser; //minimal difference between previous and current temperature [°C]
uint8_t valueTempMin; 
int16_t limitTempMin;
uint8_t valueTempMax;
int16_t limitTempMax;

uint8_t typeHumd;
long intervalHumdUser; //typical temperatur polling interval (ms)
float diffHumdUser;
uint8_t valueHumdMin;
int16_t limitHumdMin;
uint8_t valueHumdMax;
int16_t limitHumdMax;

// Callback function to handle com objects updates
void knxEvents(byte index) {
   /* switch (index)
    {
    case 1 : // send temperature
      Knx.write(1,htu.readTemperature());
      break;
    case 4 : // send humidity
      Knx.write(4,htu.readHumidity());
      break;

    default: break;      
    }
  */  
};

void limitReached(float comVal,float comValMin,float comValMax,int minObj,int maxObj,int minVal, int maxVal){
     if (minVal != 255){
      if (comVal <= comValMin){
            Knx.write(minObj,minVal);
      }
     }
     if (maxVal != 255){
      if (comVal >= comValMax){
            Knx.write(maxObj,maxVal);
      }
     }
};

void setup() {

// if debug mode is enabled, setup serial port with 9600 baud    
#ifdef DEBUG
    DEBUG.begin(9600);
#endif

int startDelay = (int)Tools.getUINT8Param(0);
if (startDelay > 0){
#ifdef DEBUG  
  DEBUG.print("start delay for ");
  DEBUG.print(startDelay);
  DEBUG.println(" s");
#endif
//  delay(startDelay*1000);
#ifdef DEBUG  
  DEBUG.println("ready!");
#endif
}
    
    // Initialize KNX enabled Arduino Board
    Tools.init(/* KNX transceiver serial port */ KNX_SERIAL, 
            /* Prog Button Pin */ PROG_BUTTON_PIN, 
            /* Prog LED Pin */ PROG_LED_PIN, 
            /* manufacturer */ 57005, 
            /* device */ 1, 
            /* revision */0);


    // setup GPIOs here!
 
    htu.begin();

    
    typeTemp = (int)Tools.getUINT8Param(1);
    intervalTempUser = (long)Tools.getUINT32Param(2)*1000; //temperature polling interval (ms)
    diffTempUser = (float)Tools.getUINT8Param(3)*0.1; //minimal difference between previous and current temperature [°C]
    valueTempMin = Tools.getUINT8Param(4); 
    limitTempMin = Tools.getINT16Param(5);
    valueTempMax = Tools.getUINT8Param(6);
    limitTempMax = Tools.getINT16Param(7);

    typeHumd = Tools.getUINT8Param(8);
    intervalHumdUser = (long)Tools.getUINT32Param(9)*1000; //humidity polling interval (ms)
    diffHumdUser = (float)Tools.getUINT8Param(10)*0.1;
    valueHumdMin = Tools.getUINT8Param(11);
    limitHumdMin = Tools.getINT16Param(12);
    valueHumdMax = Tools.getUINT8Param(13);
    limitHumdMax = Tools.getINT16Param(14);
}


void loop() {
    Knx.task();
    unsigned long currentTime = millis();
  if (!Tools.getProgState()) {//check programming mode
//Temperature
    if ((currentTime - previousTimeTemp) >= intervalTempUser){
      currentTemp = htu.readTemperature();
      #ifdef DEBUG  
         DEBUG.print("currentTemp: ");
         DEBUG.println(currentTemp);
      #endif
      if (currentTemp <900){
      switch (typeTemp){
        case 0:
          Knx.write(1,currentTemp);
          break;
        case 1:
          if (abs(currentTemp*100 - previousTemp*100)>= diffTempUser*100){//"*100" => "float to int"
            Knx.write(1,currentTemp);
            previousTemp = currentTemp;
          }
          break;
        default:
          break;
      }
      limitReached(currentTemp,limitTempMin,limitTempMax,2,3,valueTempMin,valueTempMax);
      previousTimeTemp = currentTime;         
    }
    }
//Humidity
    if ((currentTime - previousTimeHumd) >= intervalHumdUser){
      currentHumd = htu.readHumidity();
      #ifdef DEBUG 
          DEBUG.print("currentHumd: ");
          DEBUG.println(currentHumd);
      #endif
      if (currentHumd <900){
      switch (typeHumd){
        case 0:
          Knx.write(4,currentHumd);
          break;
        case 1:
          if (abs(currentHumd*100 - previousHumd*100)>= diffHumdUser*100){
            Knx.write(4,currentHumd);
            previousHumd = currentHumd;
          }  
        default:
          break;
      }
      limitReached(currentHumd,limitHumdMin,limitHumdMax,5,6,valueHumdMin,valueHumdMax);
      previousTimeHumd = currentTime;
     }
    }
 }
}








