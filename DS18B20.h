
#ifndef DS18B20_H
#define DS18B20_H

#include "OneWire.h"

#define DS_DISCONNECTED  -100
#define DS_CRC_ERROR  -101
#define NOT_DS_DEVICE  -102

class DS18B20
{
public:
  uint8_t pin;
  DS18B20(uint8_t _pin) {
    pin = _pin;
  };

  int read(uint8_t _index) {
    OneWire oneWire(pin);
    
    byte present = 0;
    byte type_s;
    byte data[12];
    byte addr[8];
    
    for(uint8_t i = 0; i <= _index; i++) {
      if ( !oneWire.search(addr)) {
        oneWire.reset_search();
        delay(250);
        //No more addresses.
        return DS_DISCONNECTED;
      }
    }

    if (OneWire::crc8(addr, 7) != addr[7]) {
      // CRC is not valid!
      return DS_CRC_ERROR;
    }

    switch(addr[0]) {
      case 0x10:
        // Chip DS18S20 or old DS1820
        type_s = 1;
        break;
      case 0x28:
        // Chip DS18B20
        type_s = 0;
        break;
      case 0x22:
        // Chip DS1822
        type_s = 0;
        break;
      default:
        // Device is not a DS18x20 family device.
        return NOT_DS_DEVICE;
    }

    oneWire.reset();
    oneWire.select(addr);
    oneWire.write(0x44, 1);        // start conversion, with parasite power on at the end
    
    delay(800);     // maybe 750ms is enough, maybe not
    // we might do a oneWire.depower() here, but the reset will take care of it.
    
    present = oneWire.reset();
    oneWire.select(addr);    
    oneWire.write(0xBE);         // Read Scratchpad

    for( byte i = 0; i < 9; i++) {           // we need 9 bytes
      data[i] = oneWire.read();
    }

    // Convert the data to actual temperature
    // because the result is a 16 bit signed integer, it should
    // be stored to an "int16_t" type, which is always 16 bits
    // even when compiled on a 32 bit processor.
    int16_t raw = (data[1] << 8) | data[0];
    if(type_s) {
      raw = raw << 3; // 9 bit resolution default
      if (data[7] == 0x10) {
        // "count remain" gives full 12 bit resolution
        raw = (raw & 0xFFF0) + 12 - data[6];
      }
    } else {
      byte cfg = (data[4] & 0x60);
      // at lower res, the low bits are undefined, so let's zero them
      if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
      else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
      else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
      //// default is 12 bit resolution, 750 ms conversion time     
    }
    return (float)raw / 16.0;
  };
};

#endif // __DS18B20_H__
