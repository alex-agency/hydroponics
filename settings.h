
#ifndef SETTINGS_H
#define SETTINGS_H

#include <avr/eeprom.h>

#define EEPROM_SIZE  1024 
#define EEPROM_OFFSET  false

// Declare EEPROM values
#define SETTINGS_ID  ":)"
// Declare structure and default settings
struct SettingsStruct {
  uint8_t wateringDayPeriod, wateringNightPeriod, wateringSunrisePeriod;
  uint8_t mistingDayPeriod, mistingNightPeriod, mistingSunrisePeriod;
  uint8_t daytimeFrom, daytimeTo;
  uint8_t nighttimeFrom, nighttimeTo;
  uint8_t lightThreshold, lightDayDuration;
  uint8_t humidThreshold, tempThreshold, tempSubsThreshold;
  char id[3];
} settings = { 
  60, 180, 90,
  30, 90, 60,
  13, 16,
  21, 04,
  200, 14,
  40, 20, 20,
  SETTINGS_ID
}, memory;

// Debug info
#define DEBUG  true


class EEPROM 
{
  public:
    bool changed;

    bool load() {
      // search through the EEPROM for a valid structure
      for(; address_offset < EEPROM_SIZE-sizeof(memory); ++address_offset) {    
        //read a struct sized block from the EEPROM
        readBlock(address_offset, memory);
        if (strcmp(memory.id, SETTINGS_ID) == 0) {
	        // load settings        
	        settings = memory;
          if(DEBUG)
            printf_P(PSTR("EEPROM: Info: Settings loaded from %d address.\n\r"),
              address_offset);
          return true;
	      }
      } 
      printf_P(PSTR("EEPROM: Error: Can't load settings!\n\r"));
      return false;
    }

    bool save() {
      // move on store position
      if(EEPROM_OFFSET) address_offset++;
      // if writing at offset would mean going outside the EEPROM limit
      if(address_offset > EEPROM_SIZE-sizeof(settings)) 
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
	      if(DEBUG) 
          printf_P(PSTR("EEPROM: Info: Saved settings to %d address.\n\r"),
           address_offset);
	      return true;
      }
      printf_P(PSTR("EEPROM: Error: Settings isn't saved to %d address!\n\r"), 
        address_offset);
      return false;
    }

  private:
    uint8_t address_offset;

    template <class T> void readBlock(uint8_t _address, const T _value) {
       eeprom_read_block((void*)&_value, (const void*)_address, sizeof(_value));
    }

    template <class T> uint8_t updateBlock(uint8_t _address, const T _value) {
      uint8_t writeCount, skipCount = 0;
      const byte* bytePointer = (const byte*)(void*)&_value;
      for(uint8_t i = 0; i < sizeof(_value); i++) {
        if (eeprom_read_byte((uint8_t*)_address) != *bytePointer) {
          //do the actual EEPROM writing
          eeprom_write_byte((uint8_t*)_address, (uint8_t*)&_value);
          writeCount++; 
        } else {
          skipCount++;
        }
        _address++;
        bytePointer++;
      }
      printf_P(PSTR("EEPROM: Warning: Writed %d bytes!\n\r"), writeCount);
      return writeCount + skipCount;
    }
};

extern EEPROM storage;

#endif // __SETTINGS_H__
