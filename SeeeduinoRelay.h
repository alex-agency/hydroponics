/******************************************************************************* 
 * SeeeduinoRelay, a very simple class to control the relays on a Seeedunio Relay board 
 * Copyright (C) 2011 Steven Cogswell
 * 
 * Version 20110712A
 * 
 * Version History:
 *    July 11, 2011: Initial version
 * 
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

// Hardware note:
// Note the relay shield needs the 9V power supply on the board hooked up in order to 
// actually be able to fire the relays.  This 9V on the sheild does not power the UNO board.
// If you power the Arduino board from it's power connector, then the shield is powered
// that way, so you don't need two power connectors.  i.e. - you can't run the relays 
// with just USB power. 
//
// Although this library was written with the Seeedstudio relay shield in mind, it will
// work perfectly line with generic relays. Just change the "RELAYxPIN" definitions below. 
//
// Shield details: 
// http://www.seeedstudio.com/depot/relay-shield-p-693.html?cPath=132_134
// http://garden.seeedstudio.com/index.php?title=Relay_Shield
//
// Software Note: Seeeduino has THREE e's, not two as you might think.  Check that
// first when trying to find unresolved references. 
// 

#ifndef SEEEDUINORELAY_H
#define SEEEDUINORELAY_H

#include <Arduino.h>

// These are the pins that control the relays on the Seeeduino board.  
// If you are modifying this to work with different relay configurations, 
// these are the pins you need to change. 
#define RELAY1PIN 7
#define RELAY2PIN 6
#define RELAY3PIN 5
#define RELAY4PIN 4

// Class Definition. 
class SeeeduinoRelay
{
public: 
  SeeeduinoRelay(int RelayNum, int state);    // Constructor 
  void on();    // Turns relay on 
  void off();   // Turns relay off
  void toggle();  // Toggles state of relay between on and off
  int state();    // returns state of relay (LOW/0/off or HIGH/1/on) 
  int isRelayOn();   // Returns TRUE if the relay is on , false otherwise 
  int isRelayOff();  // Returns TRUE if the relay is off, false otherwise 

private: 
  int relayState;     // Variables holds on/off state of the relay
  int relayPin;       // Variable holds which Arduino pin connected to relay. 
};

#endif   // SEEEDUINORELAY_H

