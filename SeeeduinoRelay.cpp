/******************************************************************************* 
 * SeeeduinoRelay, a very simple class to control the relays on a Seeedunio Relay board 
 * Copyright (C) 2011 Steven Cogswell
 * 
 * See SeeduinoRelay.h for version history 
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 ***********************************************************************************/

#include <Arduino.h>
#include "SeeeduinoRelay.h"

//
// The constructor sets up a single relay, specified by RelayNum
// Which is relay 1,2,3 or 4 as specified on the Seeeduino Relay board
// "state" is the initial state (LOW or HIGH)
// 
// Relay Truth Table:
// State     NOx    NCx 
// LOW       open   closed
// HIGH      closed open
//
// The constructor will also properly set the assigned pin to OUTPUT. 
//
SeeeduinoRelay::SeeeduinoRelay(int RelayNum, int state)
{
  if (RelayNum == 1)  relayPin=RELAY1PIN;
  if (RelayNum == 2)  relayPin=RELAY2PIN;
  if (RelayNum == 3)  relayPin=RELAY3PIN; 
  if (RelayNum == 4)  relayPin=RELAY4PIN; 
  pinMode(relayPin, OUTPUT); 

  if (state == LOW) {
    relayState=LOW; 
    off(); 
  } 
  else {
    relayState=HIGH;
    on(); 
  }
}

// Turns the relay on. 
void SeeeduinoRelay::on() 
{
  digitalWrite(relayPin, HIGH); 
  relayState=HIGH; 
}

// Turns the relay off. 
void SeeeduinoRelay::off()
{
  digitalWrite(relayPin, LOW); 
  relayState=LOW; 
}

//Toggles the state of the relay
void SeeeduinoRelay::toggle()
{
  if (relayState==HIGH) {
    off(); 
  } 
  else {
    on(); 
  }
}

// Returns the state of the relay (LOW/0 or HIGH/1)
int SeeeduinoRelay::state()
{
  return(relayState); 
}

// If the relay is on, returns true, otherwise returns false
int SeeeduinoRelay::isRelayOn()
{
  if (relayState==HIGH) 
    return true; 
  else
    return false; 
}

// If the relay is off, returns true, otherwise returns false
int SeeeduinoRelay::isRelayOff()
{
  if (relayState==LOW) 
    return true; 
  else
    return false; 
}



