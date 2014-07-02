
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
#define MAX_WRITES  20

// Declare EEPROM values
#define SETTINGS_ID "r1"
// Declare structure and default settings
struct SettingsStruct {
  uint8_t wateringDuration, wateringSunnyPeriod, wateringNightPeriod, wateringOtherPeriod;
  uint8_t mistingDuration, mistingSunnyPeriod, mistingNightPeriod, mistingOtherPeriod;
  uint16_t lightMinimum, lightDayStart; uint8_t lightDayDuration;
  uint8_t humidMinimum, humidMaximum; 
  uint8_t airTempMinimum, airTempMaximum, subsTempMinimum;
  char id[3];
} settings = {
  15, 90, 0, 120,
  3, 120, 0, 60,
  700, 300, 14,
  50, 75,
  17, 35, 15,
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
      // prevent to burn EEPROM
      if(writes_count >= MAX_WRITES) {
        printf_P(PSTR("EEPROM: Error: Reached limit %d writes!\n\r"), 
          MAX_WRITES);
        return false;
      }
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
    uint16_t writes_count;

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
      writes_count += writeCount;
      printf_P(PSTR("EEPROM: Warning: Writed %d bytes!\n\r"), 
        writeCount);
      #ifdef DEBUG
        printf_P(PSTR("EEPROM: Info: %d bytes not changed!\n\r"), skipCount);
      #endif
      return writeCount + skipCount;
    }
};

#endif // __SETTINGS_H__
