/*!
 * @file KnxTelegram.cpp
 *
 * @section author Author
 *
 * Written by Alexander Christian.
 *
 * @section license License
 *
 *    Copyright (C) 2016 Alexander Christian <info(at)root1.de>. All rights
 * reserved. This file is part of KONNEKTING Device Library.
 *
 *    The KONNEKTING Device Library is free software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "KnxTelegram.h"

KnxTelegram::KnxTelegram() { clearTelegram(); }; // Clear telegram with default values


void KnxTelegram::clearTelegram(void)
{
// clear telegram with default values :
// std FF, no repeat, normal prio, empty payload
// multicast, routing counter = 6, payload length = 1
  memset(_telegram,0,KNX_TELEGRAM_MAX_SIZE);
  _controlField = CONTROL_FIELD_DEFAULT_VALUE ; 
  _routing= ROUTING_FIELD_DEFAULT_VALUE;
}

   
void KnxTelegram::setLongPayload(const byte origin[], byte nbOfBytes) 
{
  if (nbOfBytes > KNX_TELEGRAM_PAYLOAD_MAX_SIZE-2) nbOfBytes = KNX_TELEGRAM_PAYLOAD_MAX_SIZE-2;
  for(byte i=0; i < nbOfBytes; i++) _payloadChecksum[i] = origin[i];
}


void KnxTelegram::clearLongPayload(void)
{
  memset(_payloadChecksum,0,KNX_TELEGRAM_PAYLOAD_MAX_SIZE-1);
}


void KnxTelegram::getLongPayload(byte destination[], byte nbOfBytes) const
{
  if (nbOfBytes > KNX_TELEGRAM_PAYLOAD_MAX_SIZE-2) nbOfBytes = KNX_TELEGRAM_PAYLOAD_MAX_SIZE-2 ;
  memcpy(destination, _payloadChecksum, nbOfBytes);
};
    

byte KnxTelegram::calculateChecksum(void) const
{
  byte indexChecksum, xorSum=0;  
  indexChecksum = KNX_TELEGRAM_HEADER_SIZE + getPayloadLength() + 1;
  for (byte i = 0; i < indexChecksum ; i++)   xorSum ^= _telegram[i]; // XOR Sum of all the databytes
  return (byte)(~xorSum); // Checksum equals 1's complement of databytes XOR sum
}


void KnxTelegram::updateChecksum(void)
{
  byte indexChecksum, xorSum=0; 
  indexChecksum = KNX_TELEGRAM_HEADER_SIZE + getPayloadLength() + 1;
  for (byte i = 0; i < indexChecksum ; i++)   xorSum ^= _telegram[i]; // XOR Sum of all the databytes
  _telegram[indexChecksum] = ~xorSum; // Checksum equals 1's complement of databytes XOR sum
}


void KnxTelegram::copy(KnxTelegram& dest) const
{
  byte length = getTelegramLength();
  memcpy(dest._telegram, _telegram, length);
}


void KnxTelegram::copyHeader(KnxTelegram& dest) const
{
  memcpy(dest._telegram, _telegram, KNX_TELEGRAM_HEADER_SIZE);
}


e_KnxTelegramValidity KnxTelegram::getValidity(void) const
{
  if ((_controlField & CONTROL_FIELD_PATTERN_MASK) != CONTROL_FIELD_VALID_PATTERN) return KNX_TELEGRAM_INVALID_CONTROL_FIELD; 
  if ((_controlField & CONTROL_FIELD_FRAME_FORMAT_MASK) != CONTROL_FIELD_STANDARD_FRAME_FORMAT) return KNX_TELEGRAM_UNSUPPORTED_FRAME_FORMAT; 
  if (!getPayloadLength()) return KNX_TELEGRAM_INCORRECT_PAYLOAD_LENGTH ;
  if ((_commandH & COMMAND_FIELD_PATTERN_MASK) != COMMAND_FIELD_VALID_PATTERN) return KNX_TELEGRAM_INVALID_COMMAND_FIELD;
  if ( getChecksum() != calculateChecksum()) return KNX_TELEGRAM_INCORRECT_CHECKSUM ;
  byte cmd=getCommand();
  if  (    (cmd!=KNX_COMMAND_VALUE_READ) && (cmd!=KNX_COMMAND_VALUE_RESPONSE) 
       && (cmd!=KNX_COMMAND_VALUE_WRITE) && (cmd!=KNX_COMMAND_MEMORY_WRITE)) return KNX_TELEGRAM_UNKNOWN_COMMAND;
  return  KNX_TELEGRAM_VALID;
};


void KnxTelegram::info(String& str) const
{
  byte payloadLength = getPayloadLength();

  str+="SrcAddr=" + String(getSourceAddress(),HEX);
  str+="\nTargetAddr=" + String(getTargetAddress(),HEX);
  str+="\nPayloadLgth=" + String(payloadLength,DEC);
  str+="\nCommand=";
  switch(getCommand())
  {
    case KNX_COMMAND_VALUE_READ : str+="VAL_READ"; break;
    case KNX_COMMAND_VALUE_RESPONSE : str+="VAL_RESP"; break;
    case KNX_COMMAND_VALUE_WRITE : str+="VAL_WRITE"; break;
    case KNX_COMMAND_MEMORY_WRITE : str+="MEM_WRITE"; break;
    default : str+="ERR_VAL!"; break;
  }
  str+="\nPayload=" + String(getFirstPayloadByte(),HEX)+' ';
  for (byte i = 0; i < payloadLength-1; i++) str+=String(_payloadChecksum[i], HEX)+' ';
  str+='\n';
}


void KnxTelegram::KnxTelegram::infoRaw(String& str) const
{
  for (byte i = 0; i < KNX_TELEGRAM_MAX_SIZE; i++) str+=String(_telegram[i], HEX)+' ';
  str+='\n';
}


void KnxTelegram::infoVerbose(String& str) const
{
  byte payloadLength = getPayloadLength();
  str+= "Repeat="; str+= isRepeated() ? "YES" : "NO";
  str+="\nPrio=";
  switch(getPriority())
  {
    case KNX_PRIORITY_SYSTEM_VALUE : str+="SYSTEM"; break;
    case KNX_PRIORITY_ALARM_VALUE : str+="ALARM"; break;
    case KNX_PRIORITY_HIGH_VALUE : str+="HIGH"; break;
    case KNX_PRIORITY_NORMAL_VALUE : str+="NORMAL"; break;
    default : str+="ERR_VAL!"; break;
  }
  str+="\nSrcAddr=" + String(getSourceAddress(),HEX);
  str+="\nTargetAddr=" + String(getTargetAddress(),HEX);
  str+="\nGroupAddr="; if (isMulticast()) str+= "YES"; else str+="NO";
  str+="\nRout.Counter=" + String(getRoutingCounter(),DEC);
  str+="\nPayloadLgth=" + String(payloadLength,DEC);
  str+="\nTelegramLength=" + String(getTelegramLength(),DEC);
  str+="\nCommand=";
  switch(getCommand())
  {
    case KNX_COMMAND_VALUE_READ : str+="VAL_READ"; break;
    case KNX_COMMAND_VALUE_RESPONSE : str+="VAL_RESP"; break;
    case KNX_COMMAND_VALUE_WRITE : str+="VAL_WRITE"; break;
    case KNX_COMMAND_MEMORY_WRITE : str+="MEM_WRITE"; break;
    default : str+="ERR_VAL!"; break;
  }
  str+="\nPayload=" + String(getFirstPayloadByte(),HEX)+' ';
  for (byte i = 0; i < payloadLength-1; i++) str+=String(_payloadChecksum[i], HEX)+' ';
  str+="\nValidity=";
   switch(getValidity())
  {
    case KNX_TELEGRAM_VALID : str+="VALID"; break;
    case KNX_TELEGRAM_INVALID_CONTROL_FIELD : str+="INVALID_CTRL_FIELD"; break;
    case KNX_TELEGRAM_UNSUPPORTED_FRAME_FORMAT : str+="UNSUPPORTED_FRAME_FORMAT"; break;
    case KNX_TELEGRAM_INCORRECT_PAYLOAD_LENGTH : str+="INCORRECT_PAYLOAD_LGTH"; break;
    case KNX_TELEGRAM_INVALID_COMMAND_FIELD : str+="INVALID_CMD_FIELD"; break;
    case KNX_TELEGRAM_UNKNOWN_COMMAND : str+="UNKNOWN_CMD"; break;
    case KNX_TELEGRAM_INCORRECT_CHECKSUM : str+="INCORRECT_CHKSUM"; break;
    default : str+="ERR_VAL!"; break;
  }
  str+='\n';
}

// EOF
