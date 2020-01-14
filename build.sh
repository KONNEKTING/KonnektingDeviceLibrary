#!/bin/bash

rootcheck () {
    if [ $(id -u) != "0" ]
    then
    	echo "!! We require root permissions, try SUDO ?"
        echo ""
        echo ""
#        sudo "$0" "$1"  # Modified as suggested below.
        exit $?
    else
       echo "-> Ok, we have root permissions."
    fi
}

printCheckmark() {
    echo -e "\xe2\x9c\x93"
    echo ""
}

echo "========================================"
echo "Arduino CLI Build "
echo "========================================"
echo
rootcheck

WORKING_DIR="${CI_PROJECT_DIR:-/tmp/arduino-cli-build}"
# script argument #1 --> which sketch to compile
SKETCH=$1
# script argument #2 --> which output dir to use
OUT_DIR="${2:-$WORKING_DIR/bin}"
mkdir -p $OUT_DIR
OUT_FILE=`basename -a -s .ino $SKETCH`
ARDUINO_DIR=$WORKING_DIR/Arduino
ARDUINO_LIB_DIR=$ARDUINO_DIR/libraries
ARDUINO_15_DIR=$WORKING_DIR/.arduino15
ARDUINO_CLI_DIR=$WORKING_DIR/arduino-cli
PATH="$ARDUINO_CLI_DIR:$PATH"
alias arduinocli="$ARDUINO_CLI_DIR/arduino-cli --config-file $ARDUINO_CLI_DIR/arduino-cli.yaml"
# make this script expand alias, see alias usage below...
shopt -s expand_aliases

echo "Sketch: $SKETCH"
echo "Using working directory: $WORKING_DIR"
echo "Using output directory: $OUT_DIR"

# searching for lines which contains something like
#   http://librarymanager/All#xxx @ 1.2.3
# This will extract the lib name (xxx) and version (1.2.3) so that arduino CLI can install it
LIBLINES=`cat $SKETCH | gawk 'match($0, /^.* http:\/\/librarymanager\/.*#(.*) @ (.*)$/, a) {print a[1]"@"a[2]}'`
echo "Detected dependencies: "
echo $LIBLINES
echo 
for libline in $LIBLINES
do
   echo "Installing dependency: $libline"
   arduinocli lib install "$libline"
   echo "*done*"
done
echo ""

echo "Compiling $SKETCH for SAMD ..."
arduinocli compile -b arduino:samd:mzero_bl -o $OUT_DIR/${OUT_FILE}_samd.bin $SKETCH

#echo "Compiling $SKETCH for ESP8266 ..."
#- arduinocli compile -b esp8266:esp8266:generic -o $OUT_DIR/${OUT_FILE}_esp8266.bin $SKETCH

echo "Compiling $SKETCH for UNO ..."
arduinocli compile -b arduino:avr:uno -o $OUT_DIR/${OUT_FILE}_uno.bin $SKETCH

echo "Compiling $SKETCH for Leonardo ..."
arduinocli compile -b arduino:avr:leonardo -o $OUT_DIR/${OUT_FILE}_leonardo.bin $SKETCH

echo "build *done*"