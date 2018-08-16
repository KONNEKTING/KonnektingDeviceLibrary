#include "ArrayList.h"

ArrayList<word> list;

extern "C" char *sbrk(int i);

int freeMemory() {
#ifdef __SAMD21G18A__
  char stack_dummy = 0;
  return &stack_dummy - sbrk(0);
#else
return -1
#endif
}

#ifdef __SAMD21G18A__
#define Serial SerialUSB
#endif


void fillAndClear() {

  Serial.print(F("before fill: "));
  Serial.println(freeMemory());

  for (word i = 0; i < 128; i++) {
    list.add(i);
//    Serial.print(F("after add  : "));
//    Serial.println(freeMemory());
//    list.log();
  }

  Serial.print(F("after fill : "));
  Serial.println(freeMemory());

  list.clear();

  Serial.print(F("after clear: "));
  Serial.println(freeMemory());

}

void setup() {

  Serial.begin(115200);
  while (!Serial) {}
  Serial.println(F("Starting"));


  Serial.print(F("start:"));
  Serial.println(freeMemory());

  for (int i = 0; i < 3; i++) {
    Serial.print(F("\ni="));
    Serial.println(i);
    fillAndClear();
    Serial.println(F("----------------\n"));
  }

  list.add(0x0000);
  list.add(0x0001);
  list.add(0x1001);
  Serial.println(freeMemory());
  word w;
  list.get(2, w);
  Serial.print(F("w="));
  Serial.println(w);
  list.log();
  list.clear();

  Serial.print(F("...end: "));
  Serial.println(freeMemory());

}

void loop() {
}
