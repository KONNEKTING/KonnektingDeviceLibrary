#!/bin/bash

# script argument #1 --> which sketch to compile
SKETCH=$1

echo "Compiling $SKETCH for SAMD ..."
arduinocli compile -b arduino:samd:mzero_bl $SKETCH

#echo "Compiling $SKETCH for ESP8266 ..."
#- arduinocli compile -b esp8266:esp8266:generic $SKETCH

echo "Compiling $SKETCH for UNO ..."
arduinocli compile -b arduino:avr:uno $SKETCH

echo "Compiling $SKETCH for Leonardo ..."
arduinocli compile -b arduino:avr:leonardo $SKETCH

echo "build *done*"