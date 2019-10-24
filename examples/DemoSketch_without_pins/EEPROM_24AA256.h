#include <Wire.h>

//  24AA256 I2C EEPROM
byte readMemory(int index) {
    byte data = 0xFF;
    if(index >= 0 && index < 32768){
        Wire.beginTransmission(0x50);
        Wire.write((int) (index >> 8));
        Wire.write((int) (index & 0xFF));
        Wire.endTransmission();
        Wire.requestFrom(0x50, 1);
        if (Wire.available()) {
            data = Wire.read();
        }
    }
    return data;
}

void writeMemory(int index, byte val) {
    if(index >= 0 && index < 32768 && val >= 0 && val < 256){
        Wire.beginTransmission(0x50);
        Wire.write((int) (index >> 8));
        Wire.write((int) (index & 0xFF));
        Wire.write(val);
        Wire.endTransmission();
        delay(5);
    }
}

void updateMemory(int index, byte val) {
    if (readMemory(index) != val) {
        writeMemory(index, val);
    }
}

void commitMemory() {
    // EEPROM needs no commit, so this function is intentionally left blank 
}