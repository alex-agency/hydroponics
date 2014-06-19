
#ifndef SETTINGS_H
#define SETTINGS_H

#include <avr/eeprom.h>

//#define DEBUG

// Avoid spurious warnings
#if ! defined( NATIVE ) && defined( ARDUINO )
#undef PROGMEM
#define PROGMEM __attribute__(( section(".progmem.data") ))
#undef PSTR
#define PSTR(s) (__extension__({static const char __c[] PROGMEM = (s); &__c[0];}))
#endif

#define EEPROM_SIZE  256 
//#define EEPROM_OFFSET

// Declare EEPROM values
#define SETTINGS_ID "s8"
// Declare structure and default settings
struct SettingsStruct {
  uint8_t wateringDayPeriod, wateringNightPeriod, wateringSunrisePeriod;
  uint8_t mistingDayPeriod, mistingNightPeriod, mistingSunrisePeriod;
  uint8_t daytimeFrom, daytimeTo;
  uint8_t nighttimeFrom, nighttimeTo;
  uint16_t lightThreshold, sunrise; uint8_t lightDayDuration;
  uint8_t humidThreshold, tempThreshold, tempSubsThreshold;
  char id[3];
} settings = {
  45, 90, 60,
  60, 90, 30,
  11, 15,
  21, 05,
  200, 300, 14,
  50, 15, 15,
  SETTINGS_ID
}, test;


class EEPROM 
{
  public:
    bool changed;

    bool load() {
      address_offset = 0;
      // search through the EEPROM for a valid structure
      while(address_offset < (EEPROM_SIZE-sizeof(settings))) { 
        SettingsStruct memory;
        //read a struct sized block from the EEPROM
        readBlock(address_offset, memory);
        if (strcmp(memory.id, SETTINGS_ID) == 0) {
          // load settings
          settings = memory;
          #ifdef DEBUG
            printf_P(PSTR("EEPROM: Info: Settings loaded from address: %d.\n\r"),
            address_offset);
          #endif
          return true;
	}
        address_offset++;
      } 
      printf_P(PSTR("EEPROM: Error: Can't load settings!\n\r"));
      // initial default settings to EEPROM
      address_offset = 0; changed = true; save();
      return false;
    }

    bool save() {
      // move on store position
      #ifdef EEPROM_OFFSET 
        address_offset++;
      #endif
      // if writing at offset would mean going outside the EEPROM limit
      if(address_offset > (EEPROM_SIZE-sizeof(settings))) 
        address_offset = 0;
      
      uint8_t updateCount = 0;
      if(changed) {
        printf_P(PSTR("EEPROM: Warning: Write to EEPROM! Do this not so often!\n\r"));
        updateCount = updateBlock(address_offset, settings);
        changed = false;
      } else {
        return true;
      }

      if(updateCount == sizeof(settings)) {
        #ifdef DEBUG
          printf_P(PSTR("EEPROM: Info: Saved settings at address: %d.\n\r"),
            address_offset);
        #endif
        return true;
      }
      printf_P(PSTR("EEPROM: Error: Settings isn't saved at %d address!\n\r"), 
        address_offset);
      return false;
    }

  private:
    uint8_t address_offset;

    template <class T> void readBlock(uint16_t _address, const T& _value) {
       eeprom_read_block((void*)&_value, (const void*)_address, sizeof(_value));
    }

    template <class T> uint8_t updateBlock(uint16_t _address, T& _value) {
      uint8_t writeCount = 0, skipCount = 0;
      const uint8_t* bytePointer = (const uint8_t*)(void*)&_value;
      for(uint8_t i = 0; i < sizeof(_value); i++) {
        if (eeprom_read_byte((uint8_t*)_address) != *bytePointer) {
          #ifdef DEBUG
            printf_P(PSTR("writing: %d\n\r"), *bytePointer);
          #endif
          //do the actual EEPROM writing
          eeprom_write_byte((uint8_t*)_address, *bytePointer);
          writeCount++; 
        } else {
          skipCount++;
        }
        _address++;
        bytePointer++;
      }
      printf_P(PSTR("EEPROM: Warning: Writed %d bytes!\n\r"), 
        writeCount);
      #ifdef DEBUG
        printf_P(PSTR("EEPROM: Info: %d bytes not changed!\n\r"), skipCount);
      #endif
      return writeCount + skipCount;
    }
};

#endif // __SETTINGS_H__
