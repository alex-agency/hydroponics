// Import libraries
#include <Wire.h>
#include "printf.h"
#include "OneWire.h"
#include "DallasTemperature.h"
#include "dht11.h"
#include "SimpleMap.h"
#include "timer.h"
#include "DS1307new.h"
#include "BH1750.h"
#include "EEPROMex.h"
#include "LCDPanel.h"

// Debug info
#define DEBUG  true

// Declare DHT11 sensor digital pin
#define DHT11PIN  3
// Declare DHT11 sensor state map keys
#define HUMIDITY  "humidity"
#define T_OUTSIDE  "t outside"

// Declare data wire digital pin
#define ONE_WIRE_BUS  2
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature dallas(&oneWire);
// Declare DS18B20 sensor state map key
#define T_INSIDE  "t inside"
#define T_SUBSTRATE  "t substrate"

// Declare sensors analog pins
#define FULL_TANKPIN  A0
#define DONEPIN  A1
#define WATER_ENOUGHPIN  A6
// Declare liquid sensors state map keys
#define FULL_TANK  "full tank"
#define DONE  "done"
#define WATER_ENOUGH  "water enough"

// Declare RTC state map keys
#define CDN  "cdn" // days after 2000-01-01
#define DTIME  "dtime" // minutes after 00:00

// Declare BH1750 sensor
BH1750 lightMeter;
// Declare BH1750 sensor state map key
#define LIGHT  "light"

// Declare Relays pins
#define PUMP_1PIN  4
#define PUMP_2PIN  5
//#define PUMP_3PIN  6
#define LAMPPIN  7
// Declare Relays state map keys
#define PUMP_1  "pump-1"
#define PUMP_2  "pump-2"
//#define PUMP_3  "pump-3"
#define LAMP  "lamp"
// Declare Relays state map values
#define RELAY_OFF  1
#define RELAY_ON  0

// Declare Warning status state map key
#define WARNING  "warning"
// Declare Warning status state map values
#define NO_WARNING  0
#define WARNING_NO_WATER  1
#define WARNING_SUBSTRATE_FULL  2
#define WARNING_SUBSTRATE_LOW  3
#define WARNING_NO_SUBSTRATE  4
#define WARNING_DONE  5
#define WARNING_WATERING  6
#define WARNING_MISTING  7
#define WARNING_TEMPERATURE_COLD  8
#define WARNING_SUBSTRATE_COLD  9
#define WARNING_EEPROM  10
#define WARNING_DHT11  11
#define WARNING_DS18B20  12
#define WARNING_BH1750  13

// Declare state map
struct comparator {
  bool operator()(const char* &str1, const char* &str2) {
    return strcmp(str1, str2) == 0;
  }
};
SimpleMap<const char*, uint8_t, 14, comparator> states;

// Declare EEPROM values
#define SETTINGS_ID  "hs"
bool eeprom_ok = false;
uint16_t eeprom_offset = 0;
uint8_t eeprom_max_writes = 50;
bool settings_changed = false;
// Declare settings structure
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
  800, 14,
  40, 20, 20,
  SETTINGS_ID
}, memory;

// Declare LCD panel with buttons pins
LCDPanel lcdPanel(A4, A5);
// Declare LCD menu items
#define HOME  0
#define WATTERING_DAY  1
#define WATTERING_NIGHT  2
#define WATTERING_SUNRISE  3
#define MISTING_DAY  4
#define MISTING_NIGHT  5
#define MISTING_SUNRISE  6
#define DAY_TIME  7
#define NIGHT_TIME  8
#define LIGHT_THRESHOLD  9
#define LIGHT_DAY  10
#define HUMIDITY_THRESHOLD  11
#define T_OUTSIDE_THRESHOLD  12
#define T_SUBSTRATE_THRESHOLD  13
#define CLOCK  14

// Declare delay managers, ms
timer_t slow_timer(15000);
timer_t fast_timer(10000);

//
// Setup
//
void setup()
{
  // Configure console
  Serial.begin(57600);
  printf_begin();

  // Configure EEPROM
  // prevent to burn EEPROM
  EEPROM.setMaxAllowedWrites(eeprom_max_writes);
  // load settings
  eeprom_ok = loadSettings();

  // Configure DS18B20
  dallas.begin();

  // Configure BH1750
  lightMeter.begin(BH1750_CONTINUOUS_HIGH_RES_MODE);

  // Configure RTC RAM
  // Shift NV-RAM address 0x08 for RTC
  //RTC.setRAM(0, (uint8_t *)0x0000, sizeof(uint16_t));

  // Initialize sensors pins with internal pull-up
  pinMode(FULL_TANKPIN, INPUT_PULLUP);
  pinMode(DONEPIN, INPUT_PULLUP);
  pinMode(WATER_ENOUGHPIN, INPUT); // no pull-up for A6 and A7

  // attach to events
  lcdPanel.attachLeftClick(lcdLeftButtonClick);
  lcdPanel.attachRightClick(lcdRightButtonClick);
  lcdPanel.attachLongPress(lcdButtonLongPress);
  lcdPanel.attachEditMenu(lcdEditMenu);
  lcdPanel.attachShowMenu(lcdShowMenu);

  // check connected devices
  system_check();
}

//
// Loop
//
void loop()
{
  if( slow_timer ) {
    read_DHT11();
    read_DS18B20();
    read_BH1750();

    // check values
    check();

    // save to EEPROM
    if( settings_changed && eeprom_ok ) {
      saveSettings();
    }
  }

  if( fast_timer ) {
    read_RTC();
    read_sensors();
  }

  if( states[WARNING] == NO_WARNING ) {
    // update LCD panel
    lcdPanel.update();    
  } else {
    lcdWarning();
  } 

  delay(100); // not so fast
}

/****************************************************************************/

bool read_DHT11() {
  dht11 DHT11;
  uint8_t state = DHT11.read(DHT11PIN);
  switch (state) {
    case DHTLIB_OK:
      states[HUMIDITY] = DHT11.humidity;
      states[T_OUTSIDE] = DHT11.temperature;
      if(DEBUG) 
      	printf_P(PSTR("DHT11: Info: Outside sensor values: humidity: %d, temperature: %dC.\n\r"), 
      	states[HUMIDITY], states[T_OUTSIDE]);
      return true;
    case DHTLIB_ERROR_CHECKSUM:
      printf_P(PSTR("DHT11: Error: Checksum test failed!: The data may be incorrect!\n\r"));
      return false;
    case DHTLIB_ERROR_TIMEOUT: 
      printf_P(PSTR("DHT11: Error: Timeout occured!: Communication failed!\n\r"));
      return false;
    default: 
      printf_P(PSTR("DHT11: Error: Communication failed!\n\r"));
      return false;
  }
}

/****************************************************************************/

bool read_DS18B20() {
  dallas.requestTemperatures();
  int value = dallas.getTempCByIndex(0);
  if(value == DEVICE_DISCONNECTED_C) {
    printf_P(PSTR("DS18B20: Error: Inside sensor communication failed!\n\r"));
    return false;
  }
  states[T_INSIDE] = value;
  if(DEBUG) printf_P(PSTR("DS18B20: Info: Temperature inside: %dC.\n\r"), 
        states[T_INSIDE]);
  
  value = dallas.getTempCByIndex(1);
  if(value == DEVICE_DISCONNECTED_C) {
    printf_P(PSTR("DS18B20: Error: Substrate sensor communication failed!\n\r"));
    return false;
  }
  states[T_SUBSTRATE] = value;
  if(DEBUG) printf_P(PSTR("DS18B20: Info: Temperature substrate: %dC.\n\r"), 
  	states[T_SUBSTRATE]);
  return true;
}

/****************************************************************************/

bool read_BH1750() {
  uint16_t value = lightMeter.readLightLevel();
  if(value == 54612) {
    printf_P(PSTR("BH1750: Error: Light sensor communication failed!\n\r"));
    return false;
  }
  states[LIGHT] = value;
  if(DEBUG) printf_P(PSTR("BH1750: Info: Light intensity: %d.\n\r"), states[LIGHT]);
  return true;
}

/****************************************************************************/

void read_RTC() {
  //RTC.getRAM(54, (uint8_t *)0xaa55, sizeof(uint16_t));
  RTC.getTime();
  states[CDN] = RTC.cdn;
  states[DTIME] = RTC.hour*60+RTC.minute;

  if(DEBUG) printf_P(PSTR("RTC: Info: CDN: %d -> %d-%d-%d, DTIME: %d -> %d:%d:%d.\n\r"), 
              states[CDN], RTC.year, RTC.month, RTC.day, 
              states[DTIME], RTC.hour, RTC.minute, RTC.second);
}

/****************************************************************************/

void set_RTC(uint16_t _cdn, uint16_t _dtime) {
  //RTC.setRAM(54, (uint8_t *)0xffff, sizeof(uint16_t));
  //RTC.getRAM(54, (uint8_t *)0xffff, sizeof(uint16_t));
  RTC.stopClock();
  
  if(_cdn > 0) {
    RTC.fillByCDN(_cdn);
    printf_P(PSTR("RTC: Warning: set new CDN: %d.\n\r"), _cdn);
  }

  if(_dtime > 0) {
    uint8_t minutes = _dtime % 60;
    _dtime /= 60;
    uint8_t hours = _dtime % 24;
    RTC.fillByHMS(hours, minutes, 00);
    printf_P(PSTR("RTC: Warning: set new time: %d:%d:00.\n\r"), 
      hours, minutes);
  } 

  RTC.setTime();
  //RTC.setRAM(54, (uint8_t *)0xaa55, sizeof(uint16_t));
  RTC.startClock();
}

/****************************************************************************/

void relay(const char* relay, int state) {
  // initialize relays pin
  pinMode(PUMP_1PIN, OUTPUT);
  pinMode(PUMP_2PIN, OUTPUT);
  //pinMode(PUMP_3PIN, OUTPUT);
  pinMode(LAMPPIN, OUTPUT);
  // turn on/off
  if(strcmp(relay, PUMP_1) == 0) {
    digitalWrite(PUMP_1PIN, state);
  } 
  else if(strcmp(relay, PUMP_2) == 0) {
    digitalWrite(PUMP_2PIN, state);
  } 
  //else if(strcmp(relay, PUMP_3) == 0) {
  //  digitalWrite(PUMP_3PIN, state);
  //}
  else if(strcmp(relay, LAMP) == 0) {
    digitalWrite(LAMPPIN, state);
  }
  else {
    printf("RELAY: Error: '%s' is unknown!\n\r", relay);
    return;
  }
  // save states
  if(state == RELAY_ON) {
    if(DEBUG) printf("RELAY: Info: '%s' is enabled.\n\r", relay);
    states[relay] = true;
  } 
  else if(state == RELAY_OFF) {
    if(DEBUG) printf("RELAY: Info: '%s' is disabled.\n\r", relay);
    states[relay] = false;
  }
}

/****************************************************************************/

void read_sensors() {
  states[FULL_TANK] = digitalRead(FULL_TANKPIN);
  states[DONE] = digitalRead(DONEPIN);

  if(analogRead(WATER_ENOUGHPIN) > 150)
    states[WATER_ENOUGH] =  1;
  else
    states[WATER_ENOUGH] =  0;
  if(DEBUG) 
    printf_P(PSTR("SENSORS: Info: Full tank: %d, Wattering done: %d, Water enough: %d.\n\r"), 
      states[FULL_TANK], states[DONE], states[WATER_ENOUGH]);
}

/****************************************************************************/

bool loadSettings() {
  // search through the EEPROM for a valid structure
  for ( ; eeprom_offset < EEPROMSizeATmega328-sizeof(memory) ; ++eeprom_offset) {    
    //read a struct sized block from the EEPROM
    EEPROM.readBlock(eeprom_offset, memory);
    if (strcmp(memory.id, SETTINGS_ID) == 0) {
      // load settings        
      settings = memory;
      if(DEBUG) printf_P(PSTR("EEPROM: Info: Settings loaded.\n\r"));
      return true;
    }
  }
  printf_P(PSTR("EEPROM: Error: Can't load settings!\n\r"));
  return false;
}

/****************************************************************************/

bool saveSettings() {
  // move on one position
  ++eeprom_offset;
  // if writing at offset would mean going outside the EEPROM limit
  if(eeprom_offset > EEPROMSizeATmega328-sizeof(settings)) 
    eeprom_offset = 0;

  int writeCount = EEPROM.updateBlock(eeprom_offset, settings);
  
  if(writeCount > 0) {
    if(DEBUG) printf_P(PSTR("EEPROM: Info: Saved settings at address %d with size %d.\n\r"),
    eeprom_offset, writeCount);
      return true;
  }
  printf_P(PSTR("EEPROM: Error: Can't save settings! Stored %d of %d at address %d.\n\r"),
    writeCount, sizeof(settings), eeprom_offset);
  return false;
}

/****************************************************************************/

void system_check() {
  if(eeprom_ok == false) {
    printf_P(PSTR("EEPROM: Error!\n\r"));
    states[WARNING] = WARNING_EEPROM;
    return;
  }
  
  read_RTC();
  if(RTC.year < 2014 || RTC.year > 2018) {
    printf_P(PSTR("RTC: Wrong date!\n\r"));
    lcdPanel.editMenu(CLOCK, 0);
    return;
  }
  
  if(read_DHT11() == false) {
    printf_P(PSTR("DHT11: Error!\n\r"));
    states[WARNING] = WARNING_DHT11;
    return;
  }
  
  if(read_DS18B20() == false) {
    printf_P(PSTR("DS18B20: Error!\n\r"));
    states[WARNING] = WARNING_DS18B20;
    return;
  }
  
  if(read_BH1750() == false) {
    printf_P(PSTR("BH1750: Error!\n\r"));
    states[WARNING] = WARNING_BH1750;
    return;
  }

}

/****************************************************************************/

void check() {


}

/****************************************************************************/

void alarm(uint8_t _mode) {
  if(DEBUG) printf_P(PSTR("ALARM: Info: Alarm raised.\n\r"));

}

/****************************************************************************/

uint8_t lcdShowMenu(uint8_t _menuItem) {
  if(DEBUG) 
    printf_P(PSTR("LCD Panel: Info: Show menu calback: Menu #%d.\n\r"), _menuItem);

  switch (_menuItem) {
    case HOME:
      fprintf(&lcdout, "Hydroponic %d:%d", RTC.hour, RTC.minute); 
      lcd.setCursor(0,1);
      fprintf(&lcdout, "tIn %dC, tOut %dC, Hum. %d%, Light %d lux", 
      states[T_INSIDE], states[T_OUTSIDE], states[HUMIDITY], states[LIGHT]);
      lcdPanel.doBlink(1, 13, 13);
      break;
    case WATTERING_DAY:
      fprintf(&lcdout, "Watering daytime"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.wateringDayPeriod);
      break;
    case WATTERING_NIGHT:
      fprintf(&lcdout, "Watering night"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.wateringNightPeriod);
      break;
    case WATTERING_SUNRISE:
      fprintf(&lcdout, "Watering sunrise"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.wateringSunrisePeriod);
      break;
    case MISTING_DAY:
      fprintf(&lcdout, "Misting daytime"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.mistingDayPeriod);
      break;
    case MISTING_NIGHT:
      fprintf(&lcdout, "Misting night"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.mistingNightPeriod);
      break;
    case MISTING_SUNRISE:
      fprintf(&lcdout, "Misting sunrise"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.mistingSunrisePeriod);
      break;
    case DAY_TIME:
      fprintf(&lcdout, "Day time"); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %dh to %dh", 
        settings.daytimeFrom, settings.daytimeTo);
      break;
    case NIGHT_TIME:
      fprintf(&lcdout, "Night time"); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %dh to %dh", 
        settings.nighttimeFrom, settings.nighttimeTo);
      break;
    case LIGHT_THRESHOLD:
      fprintf(&lcdout, "Light on when"); lcd.setCursor(0,1);
      fprintf(&lcdout, "lower %d lux", settings.lightThreshold);
      break;
    case LIGHT_DAY:
      fprintf(&lcdout, "Light day"); lcd.setCursor(0,1);
      fprintf(&lcdout, "duration %dh", settings.lightDayDuration);
      break;
    case HUMIDITY_THRESHOLD:
      fprintf(&lcdout, "Humidity not"); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %d%", settings.humidThreshold);
      break;
    case T_OUTSIDE_THRESHOLD:
      fprintf(&lcdout, "Temperature not"); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %dC", settings.tempThreshold);
      break;
    case T_SUBSTRATE_THRESHOLD:
      fprintf(&lcdout, "Temperature not"); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %dC", settings.tempSubsThreshold);
      break;
    case CLOCK:
      fprintf(&lcdout, "Current time"); lcd.setCursor(0,1);
      fprintf(&lcdout, "%d:%d %d-%d-%d", RTC.hour, RTC.minute, 
        RTC.day, RTC.month, RTC.year);
      break;   
    default:
      lcdPanel.showMenu(HOME);
      break;
  }
  return _menuItem;
};

/****************************************************************************/

uint8_t lcdEditMenu(uint8_t _menuItem, uint8_t _editCursor) {
  if(DEBUG) 
    printf_P(PSTR("LCD Panel: Info: Edit menu calback: Menu #%d, Cursor #%d.\n\r"), 
      _menuItem, _editCursor);

  switch (_menuItem) {
    case WATTERING_DAY:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.wateringDayPeriod);
      lcdPanel.doBlink(1, 6, 8);
      return 0;
    case WATTERING_NIGHT:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.wateringNightPeriod);
      lcdPanel.doBlink(1, 6, 8);
      return 0;
    case WATTERING_SUNRISE:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.wateringSunrisePeriod);
      lcdPanel.doBlink(1, 6, 8);
      return 0;
    case MISTING_DAY:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.mistingDayPeriod);
      lcdPanel.doBlink(1, 6, 8);
      return 0;
    case MISTING_NIGHT:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.mistingNightPeriod);
      lcdPanel.doBlink(1, 6, 8);
      return 0;
    case MISTING_SUNRISE:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.mistingSunrisePeriod);
      lcdPanel.doBlink(1, 6, 8);
      return 0;
    case DAY_TIME:
      fprintf(&lcdout, "Changing range"); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %dh to %dh", 
        settings.daytimeFrom, settings.daytimeTo);
      if(_editCursor == 1) {
        lcdPanel.doBlink(1, 12, 14);
        return 0; 
      } 
      lcdPanel.doBlink(1, 5, 7);
      return 1;
    case NIGHT_TIME:
      fprintf(&lcdout, "Changing range"); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %dh to %dh", 
        settings.nighttimeFrom, settings.nighttimeTo);
      if(_editCursor == 1) {
        lcdPanel.doBlink(1, 12, 14);
        return 0; 
      } 
      lcdPanel.doBlink(1, 5, 7);
      return 1;
    case LIGHT_THRESHOLD:
      fprintf(&lcdout, "Changing light"); lcd.setCursor(0,1);
      fprintf(&lcdout, "lower %d lux", settings.lightThreshold);
      lcdPanel.doBlink(1, 6, 11);
      return 0;
    case LIGHT_DAY:
      fprintf(&lcdout, "Changing light"); lcd.setCursor(0,1);
      fprintf(&lcdout, "duration %dh", settings.lightDayDuration);
      lcdPanel.doBlink(1, 10, 13);
      return 0;
    case HUMIDITY_THRESHOLD:
      fprintf(&lcdout, "Changing humid."); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %d%", settings.humidThreshold);
      lcdPanel.doBlink(1, 10, 13);
      return 0;
    case T_OUTSIDE_THRESHOLD:
      fprintf(&lcdout, "Changing temp."); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %dC", settings.tempThreshold);
      lcdPanel.doBlink(1, 10, 13);
      return 0;
    case T_SUBSTRATE_THRESHOLD:
      fprintf(&lcdout, "Changing temp."); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %dC", settings.tempSubsThreshold);
      lcdPanel.doBlink(1, 10, 13);
      return 0;   
    case CLOCK:
    case HOME:
      fprintf(&lcdout, "Setting time"); lcd.setCursor(0,1);
      fprintf(&lcdout, "%d:%d %d-%d-%d", RTC.hour, RTC.minute, 
        RTC.day, RTC.month, RTC.year);
      if(_editCursor == 4) {
        lcdPanel.doBlink(1, 12, 13);
        return 0;
      } else 
      if(_editCursor == 3) {
        lcdPanel.doBlink(1, 9, 10);
        return 1;
      } else 
      if(_editCursor == 2) {
        lcdPanel.doBlink(1, 6, 7);
        return 2;
      } else 
      if(_editCursor == 1) {
        lcdPanel.doBlink(1, 3, 4);
        return 3;
      }
      lcdPanel.doBlink(1, 0, 1);
      return 4;
    default:
      lcdPanel.showMenu(HOME);
      return 0;
  }
};

/****************************************************************************/

uint8_t lcdLeftButtonClick(uint8_t _menuItem, uint8_t _editCursor) {
  if(DEBUG) 
    printf_P(PSTR("LCD Panel: Info: Left click callback: Menu #%d, Cursor #%d.\n\r"),
      _menuItem, _editCursor);

  settings_changed = true;
  switch (_menuItem) {
    case WATTERING_DAY:
      settings.wateringDayPeriod--;
      break;
    case WATTERING_NIGHT:
      settings.wateringNightPeriod--;
      break;
    case WATTERING_SUNRISE:
      settings.wateringSunrisePeriod--;
      break;
    case MISTING_DAY:
      settings.mistingDayPeriod--;
      break;
    case MISTING_NIGHT:
      settings.mistingNightPeriod--;
      break;
    case MISTING_SUNRISE:
      settings.mistingSunrisePeriod--;
      break;
    case DAY_TIME:
      if(_editCursor == 1) {
        settings.daytimeFrom--;
      } else {
        settings.daytimeTo--;
      }
      break;
    case NIGHT_TIME:
      if(_editCursor == 1) {
        settings.nighttimeFrom--;
      } else {
        settings.nighttimeTo--;
      }
      break;
    case LIGHT_THRESHOLD:
      settings.lightThreshold--;
      break;
    case LIGHT_DAY:
      settings.lightDayDuration--;
      break;
    case HUMIDITY_THRESHOLD:
      settings.humidThreshold--;
      break;
    case T_OUTSIDE_THRESHOLD:
      settings.tempThreshold--;
      break;
    case T_SUBSTRATE_THRESHOLD:
      settings.tempSubsThreshold--;
      break;
    case CLOCK:
      RTC.stopClock();
      if(_editCursor == 4) {
        if(RTC.hour > 0)
          RTC.hour--;
      } else
      if(_editCursor == 3) {
        if(RTC.minute > 0)
          RTC.minute--;
      } else
      if(_editCursor == 2) {
        if(RTC.day > 1)
          RTC.day--;
      } else
      if(_editCursor == 1) {
        if(RTC.month > 1)
          RTC.month--;
      } else {
        RTC.year--;
      }
      settings_changed = false;
      break;
    default:
      _menuItem = HOME;
  }
  return _menuItem;
};

/****************************************************************************/

uint8_t lcdRightButtonClick(uint8_t _menuItem, uint8_t _editCursor) {
  if(DEBUG) 
    printf_P(PSTR("LCD Panel: Info: Right click callback: Menu #%d, Cursor #%d.\n\r"),
      _menuItem, _editCursor);
  
  settings_changed = true;
  switch (_menuItem) {
    case WATTERING_DAY:
      settings.wateringDayPeriod++;
      break;
    case WATTERING_NIGHT:
      settings.wateringNightPeriod++;
      break;
    case WATTERING_SUNRISE:
      settings.wateringSunrisePeriod++;
      break;
    case MISTING_DAY:
      settings.mistingDayPeriod++;
      break;
    case MISTING_NIGHT:
      settings.mistingNightPeriod++;
      break;
    case MISTING_SUNRISE:
      settings.mistingSunrisePeriod++;
      break;
    case DAY_TIME:
      if(_editCursor == 1) {
        settings.daytimeFrom++;
      } else {
        settings.daytimeTo++;
      }
      break;
    case NIGHT_TIME:
      if(_editCursor == 1) {
        settings.nighttimeFrom++;
      } else {
        settings.nighttimeTo++;
      }
      break;
    case LIGHT_THRESHOLD:
      settings.lightThreshold++;
      break;
    case LIGHT_DAY:
      settings.lightDayDuration++;
      break;
    case HUMIDITY_THRESHOLD:
      settings.humidThreshold++;
      break;
    case T_OUTSIDE_THRESHOLD:
      settings.tempThreshold++;
      break;
    case T_SUBSTRATE_THRESHOLD:
      settings.tempSubsThreshold++;
      break;
    case CLOCK:
      RTC.stopClock();
      if(_editCursor == 4) {
        if(RTC.hour < 23)
          RTC.hour++;
      } else
      if(_editCursor == 3) {
        if(RTC.minute < 59)
          RTC.minute++;
      } else
      if(_editCursor == 2) {
        if(RTC.day < 31)
          RTC.day++;
      } else
      if(_editCursor == 1) {
        if(RTC.month < 12)
          RTC.month++;
      } else {
        RTC.year++;
      }
      // nothing save to eeprom
      settings_changed = false;
      break;
    default:
      _menuItem = HOME;
  }
  return _menuItem;
}

/****************************************************************************/

uint8_t lcdButtonLongPress(uint8_t _menuItem) {
  if(DEBUG) printf_P(PSTR("LCD Panel: Info: LongPress callback: Menu #%d.\n\r"),
    _menuItem);

  if(_menuItem == CLOCK) {
    RTC.setTime();
    RTC.startClock();
  }
  return _menuItem;
}

/****************************************************************************/

void lcdWarning() {
  if(DEBUG) printf_P(PSTR("LCD Panel: Info: Show Warning.\n\r"));

  // not fast update lcd
  delay(200);

  switch (states[WARNING]) {
    case WARNING_NO_WATER:
      fprintf(&lcdout, "No water! :("); lcd.setCursor(0,1);
      fprintf(&lcdout, "Can’t misting!");
      lcdPanel.doBlink(1, 0, 13);
      return;
    case WARNING_SUBSTRATE_FULL:
      fprintf(&lcdout, "Substrate tank"); lcd.setCursor(0,1);
      fprintf(&lcdout, "is full! :)");
      return;
    case WARNING_SUBSTRATE_LOW:
      fprintf(&lcdout, "Low substrate!"); lcd.setCursor(0,1);
      fprintf(&lcdout, ":( Please add.");
      return;
    case WARNING_NO_SUBSTRATE:
      fprintf(&lcdout, "Can't watering!"); lcd.setCursor(0,1);
      fprintf(&lcdout, "Plants could die");
      lcdPanel.doBlink(0, 0, 14);
      return;
    case WARNING_DONE:
      fprintf(&lcdout, "Watering done!"); lcd.setCursor(0,1);
      fprintf(&lcdout, ":) wait... 10m");
      return;
    case WARNING_WATERING:
      fprintf(&lcdout, "Watering..."); lcd.setCursor(0,1);
      fprintf(&lcdout, "Please wait.");
      return;
    case WARNING_MISTING:
      fprintf(&lcdout, "Misting..."); lcd.setCursor(0,1);
      fprintf(&lcdout, "Please wait.");
      return;
    case WARNING_TEMPERATURE_COLD:
      fprintf(&lcdout, "Is too cold"); lcd.setCursor(0,1);
      fprintf(&lcdout, "for plants! :(");
      lcdPanel.doBlink(0, 0, 10);
      return;
    case WARNING_SUBSTRATE_COLD:
      fprintf(&lcdout, "Substrate is"); lcd.setCursor(0,1);
      fprintf(&lcdout, "too cold! :O");
      lcdPanel.doBlink(1, 10, 11);
      return;
    case WARNING_EEPROM:
      fprintf(&lcdout, "EEPROM ERROR!"); lcd.setCursor(0,1);
      fprintf(&lcdout, "Settings reset!");
      lcdPanel.doBlink(1, 0, 15);
      return;
    case WARNING_DHT11:
      fprintf(&lcdout, "DHT11 ERROR!"); lcd.setCursor(0,1);
      fprintf(&lcdout, "Check connection");
      lcdPanel.doBlink(1, 0, 15);
      return;
    case WARNING_DS18B20:
      fprintf(&lcdout, "DS18B20 ERROR!"); lcd.setCursor(0,1);
      fprintf(&lcdout, "Check connection");
      lcdPanel.doBlink(1, 0, 15);
      return;
    case WARNING_BH1750:
      fprintf(&lcdout, "BH1750 ERROR!"); lcd.setCursor(0,1);
      fprintf(&lcdout, "Check connection");
      lcdPanel.doBlink(1, 0, 15);
      return;
  }  
}

/****************************************************************************/
