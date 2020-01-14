#!/bin/sh

# setup the build environment: 
#  * install Arduino CLI
#  * install boards
#  * install required libs (may be something more than we really need, but this does not hurt)

# install CURL command to be able to install Arduino CLI
apt-get -q -q update
apt-get -y -q -q install curl rsync

# install Arduino CLI
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh

# make local arduino folders and preferences, preferences will be used by ALIAS command, see 'before_script' above
mkdir -p $CI_PROJECT_DIR/Arduino
mkdir -p $CI_PROJECT_DIR/.arduino15
echo "sketchbook_path: $CI_PROJECT_DIR/Arduino" > preferences.txt
echo "arduino_data: $CI_PROJECT_DIR/.arduino15" >> preferences.txt
cat preferences.txt

# make our project available as library for Arduino
mkdir -p $CI_PROJECT_DIR/Arduino/libraries/KONNEKTING
rsync -avz --exclude 'Arduino' --exclude 'bin' --exclude '.arduino15' * Arduino/libraries/KONNEKTING

# show config
arduinocli config dump 

# update core board index
arduinocli core update-index

# install SAMD core
arduinocli core install arduino:samd

# install UNO/Leonardo/...
arduinocli core install arduino:avr

# install ESP8266 core
arduinocli core update-index --additional-urls "http://arduino.esp8266.com/stable/package_esp8266com_index.json"
arduinocli core install esp8266:esp8266 --additional-urls "https://arduino.esp8266.com/stable/package_esp8266com_index.json"

# list installed cores and boards
arduinocli core list
arduinocli board listall

# install libs
arduinocli lib install "OneWire"
arduinocli lib install "DallasTemperature"
arduinocli lib install "SparkFun HTU21D Humidity and Temperature Sensor Breakout"
arduinocli lib install "FlashStorage"
arduinocli lib install "CRC32"

# list libs
arduinocli lib list