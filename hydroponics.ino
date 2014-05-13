// Import libraries
#include <Wire.h>
#include "LiquidCrystal_I2C.h"
#include "OneWire.h"
#include "DallasTemperature.h"
#include "printf.h"
#include "dht11.h"
#include "SimpleMap.h"
#include "timer.h"
#include "DS1307new.h"
#include "BH1750.h"
#include "SeeeduinoRelay.h"
#include "EEPROMex.h"
#include "OneButton.h"

// Debug info
#define DEBUG   true

// Declare DHT11 sensor digital pin
#define DHT11PIN  3
// Declare DHT11 sensor state map keys
#define HUMIDITY  "humidity"
#define TEMPER_OUT  "temper out"

// Declare data wire digital pin
#define ONE_WIRE_BUS  2
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature dallas(&oneWire);
// Declare DS18B20 sensor state map key
#define TEMPER_IN  "temper in"

// Declare liquid sensor state map keys
#define LOW_SUBSTRAT  "low substrat"
#define DELIVERED  "delivered"
#define LOW_WATER  "low water"

// Declare RTC state map keys
#define CDN  "cdn" // days after 2000-01-01
#define DTIME  "dtime" // minutes after 00:00

// Declare BH1750 sensor
BH1750 lightMeter;
// Declare BH1750 sensor state map key
#define LIGHT  "light"

// Declare Relays pins
#define RELAY1PIN  4
#define RELAY2PIN  5
//#define RELAY2PIN  6
#define RELAY4PIN  7
// Declare Relays
SeeeduinoRelay relay1 = SeeeduinoRelay(1,LOW); 
SeeeduinoRelay relay2 = SeeeduinoRelay(2,LOW);
//SeeeduinoRelay relay3 = SeeeduinoRelay(3,LOW); 
SeeeduinoRelay relay4 = SeeeduinoRelay(4,LOW);
// Declare Relays keys
#define PUMP_1  "pump-1"
#define PUMP_2  "pump-2"
//#define PUMP_3  "pump-3"
#define LAMP  "lamp"

// Declare buttons and its pins 
OneButton buttonLeft(A2, true);
OneButton buttonRight(A3, true);

// Declare LCD1609
// Set the pins on the I2C chip used for LCD connections: 
//                    addr, en,rw,rs,d4,d5,d6,d7,bl,blpol
LiquidCrystal_I2C lcd(0x20, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
static int lcd_putc(char c, FILE *) {
  lcd.write(c);
  return c;
};
static FILE lcdout = {0};
// Declare menu id
uint8_t lcdMenuItem = 0;
bool lcdMenuEditMode = false;
uint8_t lcdEditCursor = 0;

// Declare state map
struct comparator {
  bool operator()(const char* &str1, const char* &str2) {
    return strcmp(str1, str2) == 0;
  }
};
SimpleMap<const char*, uint8_t, 13, comparator> states;

// Declare EEPROM values
bool eeprom_ok = true;
uint16_t settingsAdress = 0;
// where to store config data
#define memoryBase 32
// ID of the settings block
#define SETTINGS_ID  "01"
// Declare settings structure
struct SettingsStruct {
  char id[3];
  uint8_t wateringDayPeriod, wateringNightPeriod, wateringSunrisePeriod;
  uint8_t mistingDayPeriod, mistingNightPeriod, mistingSunrisePeriod;
  uint8_t daytimeFrom, daytimeTo;
  uint8_t nighttimeFrom, nighttimeTo;
  uint8_t lightOnLower, lightDayDuration;
  uint8_t humidityLow, temperatureLow;
} settings = { 
  SETTINGS_ID,
  60, 180, 90,
  30, 90, 60,
  13, 16,
  21, 04,
  800, 14,
  40, 20
};
bool settingsChanged = false;

// Declare delay managers, ms
timer_t check_timer(15000);
timer_t lcd_timer(500);

//
// Setup
//
void setup()
{
  // Configure console
  Serial.begin(57600);
  printf_begin();

  // Start up Dallas Temperature library
  dallas.begin();

  // Configure EEPROM
  EEPROM.setMemPool(memoryBase, EEPROMSizeNano);
  settingsAdress = EEPROM.getAddress(sizeof(SettingsStruct)); 
  eeprom_ok = loadSettings();

  // Configure RTC
  // Shift NV-RAM address 0x08 for RTC
  //RTC.setRAM(0, (uint8_t *)0x0000, sizeof(uint16_t));
  // Set Clock
  //set_RTC(get_cdn(2014, 5, 9), get_dtime(11, 0));

  // Configure buttons
  buttonLeft.attachClick(buttonLeftClickEvent);
  buttonRight.attachClick(buttonRightClickEvent);
  buttonLeft.attachLongPressStart(buttonLeftLongPressEvent);
  buttonRight.attachLongPressStart(buttonRightLongPressEvent);

  // Configure LCD1609
  // Initialize the lcd for 16 chars 2 lines and turn on backlight
  lcd.begin(16, 2);
  fdev_setup_stream (&lcdout, lcd_putc, NULL, _FDEV_SETUP_WRITE);
  lcd.autoscroll();
  lcdShowMenuScreen();
}

//
// Loop
//
void loop()
{
  if( check_timer ) {
    // read modules
    read_DHT11();
    read_DS18B20();
    read_BH1750();
    read_RTC();
    read_relays();
    read_levels();

    // check values
    check();

    // save to EEPROM
 	if( settingsChanged && eeprom_ok )
      saveSettings();
  }

  if( lcd_timer ) {
    lcdUpdate();
  }

  // update buttons
  buttonLeft.tick();
  buttonRight.tick();

  delay(50); // not so fast
}

/****************************************************************************/

bool read_DHT11() {
  dht11 DHT11;
  uint8_t state = DHT11.read(DHT11PIN);
  switch (state) {
    case DHTLIB_OK:
      states[HUMIDITY] = DHT11.humidity;
      states[TEMPER_OUT] = DHT11.temperature;
      if(DEBUG) 
      	printf("DHT11: Info: Outside sensor values: humidity: %d, temperature: %d.\n\r", 
      	states[HUMIDITY], states[TEMPER_OUT]);
      return true;
    case DHTLIB_ERROR_CHECKSUM:
      printf("DHT11: Error: Checksum test failed!: The data may be incorrect!\n\r");
      return false;
    case DHTLIB_ERROR_TIMEOUT: 
      printf("DHT11: Error: Timeout occured!: Communication failed!\n\r");
      return false;
    default: 
      printf("DHT11: Error: Unknown error!\n\r");
      return false;
  }
}

/****************************************************************************/

void read_DS18B20() {
  dallas.requestTemperatures();
  states[TEMPER_IN] = dallas.getTempCByIndex(0);
  if(DEBUG) printf("DS18B20: Info: Temperature inside: %d.\n\r", states[TEMPER_IN]);
}

/****************************************************************************/

void read_BH1750() {
  states[LIGHT] = lightMeter.readLightLevel();
  if(DEBUG) printf("BH1750: Info: Light intensity: %d.\n\r", states[LIGHT]);
}

/****************************************************************************/

void read_RTC() {
  //RTC.getRAM(54, (uint8_t *)0xaa55, sizeof(uint16_t));
  RTC.getTime();
  states[CDN] = RTC.cdn;
  states[DTIME] = RTC.hour*60+RTC.minute;

  if(DEBUG) printf("RTC: Info: CDN: %d -> %d-%d-%d, DTIME: %d -> %d:%d:%d.\n\r", 
              states[CDN], RTC.year, RTC.month, RTC.day, 
              states[DTIME], RTC.hour, RTC.minute, RTC.second);
}

/****************************************************************************/

uint16_t get_cdn(uint8_t _day, uint8_t _month, uint16_t _year) {
  uint8_t tmp1; 
  uint16_t tmp2;
  tmp1 = 0;
  if ( _month >= 3 )
    tmp1++;
  tmp1 <<=1;
  tmp2 = ((_month+2*611)/20)+_day-91-tmp1;
  if ( tmp1 != 0 )
    tmp2 += RTC.is_leap_year(_year);
  tmp2--;
  while( _year > 2000 ) {
    _year--;
    tmp2 += 365;
    tmp2 += RTC.is_leap_year(_year);
  }
  // days after 2000-01-01
  return tmp2;
}

/****************************************************************************/

uint16_t get_dtime(uint8_t _hours, uint8_t _minutes) {
  // minutes after 00:00
  return _hours*60+_minutes;
}

/****************************************************************************/

void set_RTC(uint16_t _cdn, uint16_t _dtime) {
  //RTC.setRAM(54, (uint8_t *)0xffff, sizeof(uint16_t));
  //RTC.getRAM(54, (uint8_t *)0xffff, sizeof(uint16_t));
  RTC.stopClock();
  
  if(_cdn > 0) {
    RTC.fillByCDN(_cdn);
    printf("RTC: Warning: set new CDN: %d.\n\r", _cdn);  	
  }

  if(_dtime > 0) {
    uint8_t minutes = _dtime % 60;
    _dtime /= 60;
    uint8_t hours = _dtime % 24;
    RTC.fillByHMS(hours, minutes, 00);
    printf("RTC: Warning: set new time: %d:%d:00.\n\r", 
      hours, minutes);
  } 

  RTC.setTime();
  //RTC.setRAM(54, (uint8_t *)0xaa55, sizeof(uint16_t));
  RTC.startClock();
}

/****************************************************************************/

void read_relays() {
  states[PUMP_1] = relay1.isRelayOn();
  if(DEBUG) printf("PUMP_1: Info: Relay state: %d.\n\r", states[PUMP_1]);

  states[PUMP_2] = relay2.isRelayOn();
  if(DEBUG) printf("PUMP_2: Info: Relay state: %d.\n\r", states[PUMP_2]);

  // reserved for upgrade
  //states[PUMP_3] = relay3.isRelayOn();
  //if(DEBUG) printf("PUMP_3: Info: Relay state: %d.\n\r", states[PUMP_3]);

  states[LAMP] = relay4.isRelayOn();
  if(DEBUG) printf("LAMP: Info: Relay state: %d.\n\r", states[LAMP]);
}

/****************************************************************************/

void read_levels() {

}

/****************************************************************************/

void check() {


}

/****************************************************************************/

void alarm(uint8_t _mode) {
  if(DEBUG) printf("ALARM: Info: Alarm raised.\n\r");

}

/****************************************************************************/

bool loadSettings() {
  EEPROM.readBlock(settingsAdress, settings);
  if(settings.id == SETTINGS_ID) {
    if(DEBUG) printf("EEPROM: Info: Load settings.\n\r");
    return true;
  }
  printf("EEPROM: Error: Loading check isn't passed! Can't load settings!\n\r");
  return false;
}

/****************************************************************************/

void saveSettings() {
  EEPROM.writeBlock(settingsAdress, settings);
  if(DEBUG) printf("EEPROM: Info: Save settings.\n\r");
}

/****************************************************************************/

void buttonLeftClickEvent() {
  if(DEBUG) printf("Button LEFT: Info: Click event.\n\r");
  
  if(lcdMenuEditMode == false) {
  	lcdMenuItem--; // move backward, previous menu
    lcdShowMenuScreen();
    return;  	
  }

  settingsChanged = true;
  switch (lcdMenuItem) {
    case 1:
      settings.wateringDayPeriod--;
  	  break;
  	case 2:
      settings.wateringNightPeriod--;
      break;
  	case 3:
      settings.wateringSunrisePeriod--;
      break;
  	case 4:
      settings.mistingDayPeriod--;
      break;
  	case 5:
      settings.mistingNightPeriod--;
      break;
  	case 6:
      settings.mistingSunrisePeriod--;
      break;
  	case 7:
      if(lcdEditCursor == 1) {
        settings.daytimeFrom--;
        lcdEditCursor--;
      } else {
        settings.daytimeTo--;
      }
      break;
  	case 8:
      if(lcdEditCursor == 1) {
        settings.nighttimeFrom--;
        lcdEditCursor--;
      } else {
        settings.nighttimeTo--;
      }
      break;
  	case 9:
      settings.lightOnLower--;
      break;
  	case 10:
      settings.lightDayDuration--;
  	  break;
  	case 11:
      settings.humidityLow--;
  	  break;
  	case 12:
      settings.temperatureLow--;
  	  break;
  	case 13:
      RTC.stopClock();
      if(lcdEditCursor == 4) {
        if(RTC.hour > 0)
          RTC.hour--;
        lcdEditCursor--;
      } else
      if(lcdEditCursor == 3) {
        if(RTC.minute > 0)
          RTC.minute--;
        lcdEditCursor--;
      } else
      if(lcdEditCursor == 2) {
        if(RTC.day > 1)
          RTC.day--;
        lcdEditCursor--;
      } else
      if(lcdEditCursor == 1) {
        if(RTC.month > 1)
          RTC.month--;
        lcdEditCursor--;
      } else {
        RTC.year--;
      }
      settingsChanged = false;
  	  break;
  }
  lcdShowEditScreen();
};

/****************************************************************************/

void buttonRightClickEvent() {
  if(DEBUG) printf("Button RIGHT: Info: Click event.\n\r");

  if(lcdMenuEditMode == false) {
  	lcdMenuItem++; // move forward, next menu
    lcdShowMenuScreen();
    return;  	
  }

  settingsChanged = true;
  switch (lcdMenuItem) {
    case 1:
      settings.wateringDayPeriod++;
      break;
    case 2:
      settings.wateringNightPeriod++;
      break;
    case 3:
      settings.wateringSunrisePeriod++;
      break;
    case 4:
      settings.mistingDayPeriod++;
      break;
    case 5:
      settings.mistingNightPeriod++;
      break;
    case 6:
      settings.mistingSunrisePeriod++;
      break;
    case 7:
      if(lcdEditCursor == 1) {
        settings.daytimeFrom++;
        lcdEditCursor--;
      } else {
        settings.daytimeTo++;
      }
      break;
    case 8:
      if(lcdEditCursor == 1) {
        settings.nighttimeFrom++;
        lcdEditCursor--;
      } else {
        settings.nighttimeTo++;
      }
      break;
    case 9:
      settings.lightOnLower++;
      break;
    case 10:
      settings.lightDayDuration++;
      break;
    case 11:
      settings.humidityLow++;
      break;
    case 12:
      settings.temperatureLow++;
      break;
    case 13:
      RTC.stopClock();
      if(lcdEditCursor == 4) {
        if(RTC.hour < 23)
          RTC.hour++;
        lcdEditCursor--;
      } else
      if(lcdEditCursor == 3) {
        if(RTC.minute < 59)
          RTC.minute++;
        lcdEditCursor--;
      } else
      if(lcdEditCursor == 2) {
        if(RTC.day < 31)
          RTC.day++;
        lcdEditCursor--;
      } else
      if(lcdEditCursor == 1) {
        if(RTC.month < 12)
          RTC.month++;
        lcdEditCursor--;
      } else {
        RTC.year++;
      }
      settingsChanged = false; // nothing save to eeprom
      break;
  }
  lcdShowEditScreen();
};

/****************************************************************************/

void buttonLeftLongPressEvent() {
  if(DEBUG) printf("Button LEFT: Info: LongPress event.\n\r");

  if(lcdMenuEditMode == false) {
  	lcdMenuEditMode = true;
    lcdEditCursor = lcdShowEditScreen();
    return;
  }

  if(lcdEditCursor != 0) {
  	lcdEditCursor--; // move to next edit field
  	lcdEditCursor = lcdShowEditScreen();
  	return;
  }

  lcdMenuEditMode = false;
  if(lcdMenuItem == 13) {
    RTC.setTime();
    RTC.startClock();
  }
  lcdShowMenuScreen();	
  return;
};

/****************************************************************************/

void buttonRightLongPressEvent() {
  if(DEBUG) printf("Button RIGHT: Info: LongPress event.\n\r");

  buttonLeftLongPressEvent(); 
};

/****************************************************************************/

void lcdShowMenuScreen() {
  if(DEBUG) printf("LCD1609: Info: Show menu screen #%d.\n\r", lcdMenuItem);
  lcd.clear();
  switch (lcdMenuItem) {
  case 0:
      fprintf(&lcdout, "Hydroponic %d:%d", RTC.hour, RTC.minute); 
      lcd.setCursor(0,1);
      fprintf(&lcdout, "tIn %dC, tOut %dC, Hum. %d%, Light %d lux", 
        states[TEMPER_IN], states[TEMPER_OUT], states[HUMIDITY], states[LIGHT]);
      lcdBlink(1, 13, 13);
    return;
  case 1:
      fprintf(&lcdout, "Watering daytime"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.wateringDayPeriod);
	  return;
	case 2:
      fprintf(&lcdout, "Watering night"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.wateringNightPeriod);
	  return;
	case 3:
      fprintf(&lcdout, "Watering sunrise"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.wateringSunrisePeriod);
	  return;
	case 4:
      fprintf(&lcdout, "Misting daytime"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.mistingDayPeriod);
	  return;
	case 5:
      fprintf(&lcdout, "Misting night"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.mistingNightPeriod);
	  return;
	case 6:
      fprintf(&lcdout, "Misting sunrise"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.mistingSunrisePeriod);
	  return;
	case 7:
      fprintf(&lcdout, "Day time"); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %dh to %dh", 
        settings.daytimeFrom, settings.daytimeTo);
	  return;
	case 8:
      fprintf(&lcdout, "Night time"); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %dh to %dh", 
        settings.nighttimeFrom, settings.nighttimeTo);
	  return;
	case 9:
      fprintf(&lcdout, "Light on when"); lcd.setCursor(0,1);
      fprintf(&lcdout, "lower %d lux", settings.lightOnLower);
	  return;
	case 10:
      fprintf(&lcdout, "Light day"); lcd.setCursor(0,1);
      fprintf(&lcdout, "duration %dh", settings.lightDayDuration);
	  return;
	case 11:
      fprintf(&lcdout, "Humidity not"); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %d%", settings.humidityLow);
	  return;
	case 12:
      fprintf(&lcdout, "Temperature not"); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %dC", settings.temperatureLow);
	  return;
	case 13:
  	  fprintf(&lcdout, "Current time"); lcd.setCursor(0,1);
      fprintf(&lcdout, "%d:%d %d-%d-%d", RTC.hour, RTC.minute, 
      	RTC.day, RTC.month, RTC.year);
	  return;	  
    default: 
      lcdMenuItem = 0; lcdShowMenuScreen();
      return;
  }
};

/****************************************************************************/

uint8_t lcdShowEditScreen() {
  if(DEBUG) printf("LCD1609: Info: Show edit screen #%d with cursor: %d.\n\r", 
    lcdMenuItem, lcdEditCursor);
  lcd.clear();
  switch (lcdMenuItem) {
    case 1:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.wateringDayPeriod);
      lcdBlink(1, 6, 8);
	  return 0;
	case 2:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.wateringNightPeriod);
      lcdBlink(1, 6, 8);
	  return 0;
	case 3:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.wateringSunrisePeriod);
      lcdBlink(1, 6, 8);
	  return 0;
	case 4:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.mistingDayPeriod);
      lcdBlink(1, 6, 8);
	  return 0;
	case 5:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.mistingNightPeriod);
      lcdBlink(1, 6, 8);
	  return 0;
	case 6:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.mistingSunrisePeriod);
      lcdBlink(1, 6, 8);
	  return 0;
	case 7:
      fprintf(&lcdout, "Changing range"); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %dh to %dh", 
        settings.daytimeFrom, settings.daytimeTo);
      if(lcdEditCursor == 1) {
      	lcdBlink(1, 12, 14);
      	return 0;	
      }	
      lcdBlink(1, 5, 7);
	  return 1;
	case 8:
      fprintf(&lcdout, "Changing range"); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %dh to %dh", 
        settings.nighttimeFrom, settings.nighttimeTo);
      if(lcdEditCursor == 1) {
      	lcdBlink(1, 12, 14);
      	return 0;	
      }	
      lcdBlink(1, 5, 7);
	  return 1;
	case 9:
      fprintf(&lcdout, "Changing light"); lcd.setCursor(0,1);
      fprintf(&lcdout, "lower %d lux", settings.lightOnLower);
      lcdBlink(1, 6, 11);
	  return 0;
	case 10:
      fprintf(&lcdout, "Changing light"); lcd.setCursor(0,1);
      fprintf(&lcdout, "duration %dh", settings.lightDayDuration);
      lcdBlink(1, 10, 13);
	  return 0;
	case 11:
      fprintf(&lcdout, "Changing humid."); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %d%", settings.humidityLow);
      lcdBlink(1, 10, 13);
      return 0;
	case 12:
      fprintf(&lcdout, "Changing temp."); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %dC", settings.temperatureLow);
      lcdBlink(1, 10, 13);
	  return 0;
	case 13:
    case 0:
  	  fprintf(&lcdout, "Setting time"); lcd.setCursor(0,1);
      fprintf(&lcdout, "%d:%d %d-%d-%d", RTC.hour, RTC.minute, 
      	RTC.day, RTC.month, RTC.year);
	  if(lcdEditCursor == 4) {
	  	lcdBlink(1, 12, 13);
	  	return 0;
	  } else if(lcdEditCursor == 3) {
		lcdBlink(1, 9, 10);
		return 1;
	  } else if(lcdEditCursor == 2) {
	  	lcdBlink(1, 6, 7);
	  	return 2;
	  } else if(lcdEditCursor == 1) {
	  	lcdBlink(1, 3, 4);
	  	return 3;
	  }
	  lcdBlink(1, 0, 1);
	  return 4;
    default:
      lcdMenuItem = 0; lcdShowMenuScreen();
      return 0;
  }
};

/****************************************************************************/

void lcdBlink(uint8_t _row, uint8_t _start, uint8_t _end) { 
  if(lcdBlink) {
    if(DEBUG) printf("LCD1609: Info: Blink for: %d\n\r", _end-_start+1); 
    
    while(_start <= _end) {
      lcd.setCursor(_row, _start); lcd.print(" ");
      _start++;
    }
    lcdBlink = false;
    return;
  }
  lcdBlink = true;
};

/****************************************************************************/

void lcdUpdate() {
  if(lcdMenuEditMode) {
  	lcdShowEditScreen();
  } else {
  	lcdShowMenuScreen();
  }
};

/****************************************************************************/


