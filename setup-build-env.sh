#!/bin/bash
# enable following two line to make it run line by line --> for debugging only
#set -x
#trap read debug

# Check if we're root and re-execute if we're not.
rootcheck () {
    if [ $(id -u) != "0" ]
    then
    	echo "!! We require root permissions, restarting with SUDO ..."
        echo ""
        echo ""
        sudo "$0" "$@"  # Modified as suggested below.
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
echo "Setup Arduino Build Environment "
echo "========================================"
echo
#rootcheck

# make this script expand alias, see alias usage below...
shopt -s expand_aliases

WORKING_DIR="${CI_PROJECT_DIR:-./arduino-cli-build}"
#if [ -e $WORKING_DIR ]; then
#  echo "existing setup in $WORKING_DIR detected. Nothing to install."
#  exit;
#fi
ARDUINO_DIR=$WORKING_DIR/Arduino
ARDUINO_LIB_DIR=$ARDUINO_DIR/libraries
ARDUINO_15_DIR=$WORKING_DIR/.arduino15
ARDUINO_CLI_DIR=$WORKING_DIR/arduino-cli
PATH="$ARDUINO_CLI_DIR:$PATH"

#echo -n "-> Cleanup old stuff "
#rm -Rf $WORKING_DIR
#printCheckmark

echo "-> Using working directory: $WORKING_DIR"

# setup the build environment: 
#  * install Arduino CLI
#  * install boards
#  * install required libs (may be something more than we really need, but this does not hurt)

# install CURL command to be able to install Arduino CLI
if [ -z $(which rsync) ] && [ -z $(which curl) ]; then
  echo -n "-> APT update and install curl+rsync "
  if [ $(id -u) != "0" ]; then
    # not root, need sudo
    sudo apt-get -q -q update
    sudo apt-get -y -q -q install curl rsync
  else 
    apt-get -q -q update
    apt-get -y -q -q install curl rsync
  fi 
else
  echo -n "-> curl+rsync detected "  
fi
printCheckmark

# install Arduino CLI
echo "-> Install Arduino CLI ..."
mkdir -p $ARDUINO_CLI_DIR
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=$ARDUINO_CLI_DIR sh
echo -n "-> Install Arduino CLI "; printCheckmark;

# make local arduino folders and preferences, preferences will be used by ALIAS command, see 'before_script' above
echo -n "-> Configure Arduino CLI "
mkdir -p $ARDUINO_DIR
mkdir -p $ARDUINO_15_DIR
# write preference file
cat >$ARDUINO_CLI_DIR/arduino-cli.yaml <<EOF
board_manager:
  additional_urls: []
daemon:
  port: "50051"
directories:
  data: ${ARDUINO_15_DIR}
  downloads: ${ARDUINO_15_DIR}/staging
  user: ${ARDUINO_DIR}
logging:
  file: ""
  format: text
  level: info
EOF
# create an alias to not apply config file argument every time
alias arduinocli="$ARDUINO_CLI_DIR/arduino-cli --config-file $ARDUINO_CLI_DIR/arduino-cli.yaml"
printCheckmark;

# make our project available as library for Arduino
if [ ! -z $1 ]; then
  echo -n "-> Copy $1 to sketchbook libraries "
  mkdir -p $ARDUINO_LIB_DIR/MYLIBRARY
  rsync -avzq --exclude 'Arduino' --exclude 'arduino-cli-build' --exclude 'bin' --exclude '.arduino15' $1 $ARDUINO_LIB_DIR/MYLIBRARY
  printCheckmark;
fi

# show config
echo "-> Arduino CLI config dump: "
arduinocli config dump 

# update core board index
echo "-> Update Core index"
arduinocli core update-index

# install UNO/Leonardo/...
echo "-> Install Arduino AVR core"
arduinocli core install arduino:avr

# install SAMD core
echo "-> Install SAMD core"
arduinocli core install arduino:samd

# install ESP8266 core
echo "-> Install Arduino ESP8266 core"
arduinocli core update-index --additional-urls "http://arduino.esp8266.com/stable/package_esp8266com_index.json"
arduinocli core install esp8266:esp8266 --additional-urls "https://arduino.esp8266.com/stable/package_esp8266com_index.json"

# list installed cores and boards
echo "-> Installed cores: "
arduinocli core list
echo ""

echo "-> Installed boards: "
arduinocli board listall
echo ""

# install libs
echo "-> Installing libs ..."
arduinocli lib install "OneWire"
arduinocli lib install "DallasTemperature"
arduinocli lib install "SparkFun HTU21D Humidity and Temperature Sensor Breakout"
arduinocli lib install "FlashStorage"
arduinocli lib install "CRC32"
echo -n "-> Installing libs "; printCheckmark;

# list libs
echo "-> List of installed libs: "
arduinocli lib list
echo ""

echo -n "-> Setup Arduino Build Environment "
printCheckmark
