#!/bin/bash

# -e exits on error, -u errors on undefined variables, and -o (for option) pipefail exits on command pipe failures.
# set -euo pipefail

function rootcheck () {
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

function printCheckmark() {
    echo -e "\xe2\x9c\x93"
    echo ""
}

function printHelp() {
    echo "Usage: "
    echo "  sudo ./build.sh <sketch> <fqbn> <outputfolder>"
    echo "  -> sketch: Path and folder to sketch.ino file"
    echo "  -> fqbn (optional): full qualified board name. Supported (* = default if none specified): "
    echo "     esp8266:esp8266:generic"
    echo "     arduino:avr:uno"
    echo "     arduino:avr:leonardo"
    echo "     arduino:samd:mzero_bl *"
    echo "  -> outputfolder (optional): folder to put the compiled binary to. (./bin if none specified)"
    echo 
    echo "Example: "
    echo "  sudo ./build.sh exampleFolder/ExampleSketch.ino arduino:samd:mzero_bl ./bin"
    echo
}


echo "========================================"
echo "Arduino CLI Build "
echo "========================================"
echo
if [ -z $1 ]; then
    printHelp
    exit 0
fi

#rootcheck

WORKING_DIR="${CI_PROJECT_DIR:-./arduino-cli-build}"
# script argument #1 --> which sketch to compile
SKETCH="$1"
# script argument #2 --> which board to use
BOARD="${2:-arduino:samd:mzero_bl}"
BOARD_SHORT=`echo $BOARD | perl -n -e 'm/^.*:(.*)$/ && print $1'`
# script argument #3 --> which output dir to use
OUT_DIR="${3:-./bin}"

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
echo "Board: $BOARD"
echo "Extra: $EXTRA"
echo "Board short: $BOARD_SHORT"
echo "Using working directory: $WORKING_DIR"
echo "Using output directory: $OUT_DIR"

# searching for lines which contains something like
#   http://librarymanager/All#xxx @ 1.2.3
# This will extract the lib name (xxx) and version (1.2.3) so that arduino CLI can install it
LIBLINES=`cat $SKETCH | perl -n -e 'm/^.* http:\/\/librarymanager\/.*#(.*) @ (.*)$/ && print "${1}\@${2}"'`

if [ -z $LIBLINES ]; then
    echo "Detected dependencies: <none>"
else
    echo "Detected dependencies: "
    echo $LIBLINES
    echo 
    for libline in $LIBLINES
    do
    echo "Installing dependency: $libline"
    arduinocli lib install "$libline"
    echo "*done*"
    done
fi
echo ""

echo "Compiling $SKETCH for $BOARD ..."

# WITHOUT DEBUG
arduinocli compile -b $BOARD --output-dir $OUT_DIR/${OUT_FILE}_${BOARD_SHORT}.bin $SKETCH || { echo "build *failed*"; exit 1; }

# WITH DEBUG
# arduinocli compile --build-property compiler.cpp.extra_flags="-DDEBUG" -b $BOARD --output-dir $OUT_DIR/${OUT_FILE}_${BOARD_SHORT}.bin $SKETCH || { echo "build *failed*"; exit 1; }

echo "build *done*"