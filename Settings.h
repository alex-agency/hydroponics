
#ifndef SETTINGS_H
#define SETTINGS_H

#include <avr/eeprom.h>

//#define DEBUG_EEPROM

static const uint8_t EEPROM_SIZE = 255;
//#define EEPROM_OFFSET
// prevent burn memory
static const uint8_t MAX_WRITES = 20;
// Declare EEPROM values
#define SETTINGS_ID  "@"
// Declare structure and default settings
struct SettingsStruct {
  uint8_t wateringDuration, wateringSunnyPeriod, wateringPeriod;
  uint8_t mistingDuration, mistingSunnyPeriod, mistingPeriod;
  uint16_t lightMinimum, lightDayStart; uint8_t lightDayDuration;
  uint8_t humidMinimum, humidMaximum; 
  uint8_t airTempMinimum, airTempMaximum, subsTempMinimum;
  uint8_t silentEvening, silentMorning;
  char id[2];
  uint8_t c_celcium[8], c_heart[8], c_humidity[8];
  uint8_t c_temp[8], c_flower[8], c_lamp[8];
  uint8_t c_up[8], c_down[8];
} settings = {
  15, 80, 100,
  3, 120, 60,
  1000, 360, 14,
  45, 75,
  18, 30, 16,
  22, 8,
  SETTINGS_ID,
  {24, 24, 3, 4, 4, 4, 3, 0}, {0, 10, 21, 17, 10, 4, 0, 0}, 
  {4, 10, 10, 17, 17, 17, 14, 0}, {4, 10, 10, 14, 31, 31, 14, 0},
  {14, 27, 21, 14, 4, 12, 4, 0}, {14, 17, 17, 17, 14, 14, 4, 0},
  {4, 14, 21, 4, 4, 4, 4, 0}, {4, 4, 4, 4, 21, 14, 4, 0}
}, test;


class EEPROM 
{
  public:
    bool changed;
    bool ok;

    void load() {
      address_offset = 0;
      // search through the EEPROM for a valid structure
      while(address_offset < (EEPROM_SIZE-sizeof(settings))) { 
        SettingsStruct memory;
        //read a struct sized block from the EEPROM
        readBlock(address_offset, memory);
        if (strcmp(memory.id, SETTINGS_ID) == 0) {
          // load settings
          settings = memory;
          #ifdef DEBUG_EEPROM
            printf_P(PSTR("EEPROM: Info: Settings loaded from address: %d.\n\r"),
            address_offset);
          #endif
          ok = true;
          return;
	      }
        address_offset++;
      }
      #ifdef DEBUG_EEPROM
        printf_P(PSTR("EEPROM: Error: Can't load settings!\n\r"));
      #endif
      // load default settings to EEPROM
      address_offset = 0; 
      changed = true; 
      save();
      ok = false;
    }

    void save() {
      // prevent to burn EEPROM
      if(writes_count >= MAX_WRITES) {
        #ifdef DEBUG_EEPROM
          printf_P(PSTR("EEPROM: Error: Reached limit %d writes!\n\r"), 
            MAX_WRITES);
        #endif
        ok = false;
        return;
      }
      // move on store position
      #ifdef EEPROM_OFFSET 
        address_offset++;
      #endif
      // if writing at offset would mean going outside the EEPROM limit
      if(address_offset > (EEPROM_SIZE-sizeof(settings))) {
        address_offset = 0;
      }
      uint8_t updateCount = 0;
      if(changed) {
        #ifdef DEBUG_EEPROM
          printf_P(PSTR("EEPROM: Warning: Write to EEPROM! Do this not so often!\n\r"));
        #endif
        updateCount = updateBlock(address_offset, settings);
        changed = false;
      } else {
        ok = true;
        return;
      }

      if(updateCount == sizeof(settings)) {
        #ifdef DEBUG_EEPROM
          printf_P(PSTR("EEPROM: Info: Saved settings at address: %d.\n\r"),
            address_offset);
        #endif
        ok = true;
        return;
      }
      #ifdef DEBUG_EEPROM
        printf_P(PSTR("EEPROM: Error: Settings isn't saved at %d address!\n\r"), 
          address_offset);
      #endif
      ok = false;
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
          #ifdef DEBUG_EEPROM
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
      #ifdef DEBUG_EEPROM
        printf_P(PSTR("EEPROM: Warning: Writed %d bytes and %d bytes not changed!\n\r"), 
          writeCount, skipCount);
      #endif
      return writeCount + skipCount;
    }
};

#endif // __SETTINGS_H__
