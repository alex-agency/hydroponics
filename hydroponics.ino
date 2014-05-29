// Import libraries
#include <Wire.h>
#include "printf.h"
#include "settings.h"
#include "OneWire.h"
#include "DallasTemperature.h"
#include "dht11.h"
#include "SimpleMap.h"
#include "timer.h"
#include "DS1307new.h"
#include "BH1750.h"
#include "OneButton.h"
#include "LiquidCrystal_I2C.h"

// Debug info
#define DEBUG  true

// Declare settings
Storage storage;
bool storage_ok;

// Declare RTC state map keys
#define CDN  "cdn" // days after 2000-01-01
#define DTIME  "dtime" // minutes after 00:00

// Declare DHT11 sensor digital pin
#define DHT11PIN  3
// Declare DHT11 sensor state map keys
#define HUMIDITY  "humidity"
#define T_OUTSIDE  "t outside"

// Declare data wire digital pin
#define ONE_WIRE_BUS  2
// Declare DS18B20 sensors state map keys
#define T_INSIDE  "t inside"
#define T_SUBSTRATE  "t substrate"

// Declare BH1750 sensor state map key
#define LIGHT  "light"

// Declare liquid sensors analog pins
#define S1_SUBSTRATEPIN  A0
#define S2_WATERINGPIN  A1
#define S4_MISTINGPIN  A6
// Declare liquid sensors state map keys
#define S1_SUBSTRATE  "s1 substrate"
#define S2_WATERING  "s2 watering"
#define S4_MISTING  "s4 misting"

// Declare Relays pins
#define P1_MISTINGPIN  4
#define P2_WATERINGPIN  5
#define LAMPPIN  7
// Declare Relays state map keys
#define P1_MISTING  "p1 misting"
#define P2_WATERING  "p2 watering"
#define LAMP  "lamp"

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

// Declare push buttons
OneButton rightButton(A2, true);
OneButton leftButton(A3, true);

// Declare LCD1609
// Set the pins on the I2C chip used for LCD connections: 
//                    addr, en,rw,rs,d4,d5,d6,d7,bl,blpol
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
// Declare fprintf
int lcd_putc(char c, FILE *) {
  lcd.write(c);
  return c;
};
FILE lcdout = {0};
// Declare LCD panel variable
uint8_t menuItem;
bool menuEditMode;
uint8_t editCursor;
bool blink;
// Declare LCD menu items
#define HOME  0
#define WATERING_DAY  1
#define WATERING_NIGHT  2
#define WATERING_SUNRISE  3
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
timer_t fast_timer(1000);

uint32_t start_misting = 0;
uint32_t last_misting = 0;
uint32_t start_watering = 0;
uint32_t last_watering = 0;
uint16_t ONE_MINUTE = 60000;

//
// Setup
//
void setup()
{
  // Configure console
  Serial.begin(57600);
  // Initialize PSTR
  printf_begin();
  
  // Load settings
  //storage_ok = storage.load();

  // Configure LCD1609
  // Initialize the lcd for 16 chars 2 lines and turn on backlight
  lcd.begin(16, 2);
  // Initialize fprintf
  fdev_setup_stream (&lcdout, lcd_putc, NULL, _FDEV_SETUP_WRITE);

  // Configure buttons
  rightButton.attachClick( rightButtonClick );
  rightButton.attachDoubleClick( rightButtonClick );
  rightButton.attachLongPressStart( buttonsLongPress );
  leftButton.attachClick( leftButtonClick );
  leftButton.attachDoubleClick( leftButtonClick );
  leftButton.attachLongPressStart( buttonsLongPress );
  
  // check connected devices
  //system_check();
}

//
// Loop
//
void loop()
{
  if( slow_timer ) {
    //read_DHT11();
    //read_DS18B20();
    //read_BH1750();

    //doWork();

    if( storage.isChanged && storage_ok ) {
      // WARNING: EEPROM can burn!
      //storage_ok = storage.save();
    }
  }

  if( fast_timer ) {
    read_RTC();
    //read_levels();

    if( states[WARNING] != NO_WARNING )
      lcdWarning();
    else if( menuEditMode ) {
      lcdEditMenu(menuItem, editCursor);
    } else {
      lcdShowMenu(menuItem);
    }
  }

  leftButton.tick();
  rightButton.tick();

  delay(50); // not so fast
}

/****************************************************************************/

void read_RTC() {
  //RTC.setRAM(0, (uint8_t *)0x0000, sizeof(uint16_t));
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

bool read_DHT11() {
  dht11 DHT11;
  switch ( DHT11.read(DHT11PIN) ) {
    case DHTLIB_OK:
      states[HUMIDITY] = DHT11.humidity;
      states[T_OUTSIDE] = DHT11.temperature;
      if(DEBUG) 
      	printf_P(PSTR("DHT11: Info: Air humidity: %d, temperature: %dC.\n\r"), 
      	states[HUMIDITY], states[T_OUTSIDE]);
      return true;
    default: 
      printf_P(PSTR("DHT11: Error: Communication failed!\n\r"));
      return false;
  }
}

/****************************************************************************/

bool read_DS18B20() {
  OneWire oneWire(ONE_WIRE_BUS);
  DallasTemperature dallas(&oneWire);
  dallas.begin();
  dallas.requestTemperatures();

  int value = dallas.getTempCByIndex(0);
  if(value == DEVICE_DISCONNECTED_C) {
    printf_P(PSTR("DS18B20: Error: Computer sensor communication failed!\n\r"));
    return false;
  }
  if(DEBUG) printf_P(PSTR("DS18B20: Info: Computer temperature: %dC.\n\r"), value);
  states[T_INSIDE] = value;
  
  value = dallas.getTempCByIndex(1);
  if(value == DEVICE_DISCONNECTED_C) {
    printf_P(PSTR("DS18B20: Error: Substrate sensor communication failed!\n\r"));
    return false;
  }
  if(DEBUG) printf_P(PSTR("DS18B20: Info: Substrate temperature: %dC.\n\r"), value);
  states[T_SUBSTRATE] = value;
  return true;
}

/****************************************************************************/

bool read_BH1750() {
  BH1750 lightMeter;
  lightMeter.begin(BH1750_ONE_TIME_HIGH_RES_MODE_2);
  int value = lightMeter.readLightLevel();

  if(value < 0) {
    printf_P(PSTR("BH1750: Error: Light sensor communication failed!\n\r"));
    return false;
  }
  if(DEBUG) printf_P(PSTR("BH1750: Info: Light intensity: %d.\n\r"), value);
  states[LIGHT] = value;
  return true;
}

/****************************************************************************/

void read_levels() {
  pinMode(S1_SUBSTRATEPIN, INPUT_PULLUP);
  if(digitalRead(S1_SUBSTRATEPIN) == 0) {
    if(DEBUG) printf_P(PSTR("LEVELS: Info: Substrate tank is full.\n\r"));
    states[S1_SUBSTRATE] = true;
  } else {
    if(DEBUG) printf_P(PSTR("LEVELS: Info: Substrate tank is not full!\n\r"));
    states[S1_SUBSTRATE] = false;
  }
  
  pinMode(S2_WATERINGPIN, INPUT_PULLUP);
  if(digitalRead(S2_WATERINGPIN) == 0) {
    if(DEBUG) printf_P(PSTR("LEVELS: Info: Watering has done.\n\r"));
    states[S2_WATERING] = true;
  } else {
    if(DEBUG) printf_P(PSTR("LEVELS: Info: No watering.\n\r"));
    states[S2_WATERING] = false;
  }
  
  // no pull-up for A6 and A7
  pinMode(S4_MISTINGPIN, INPUT);
  if(analogRead(S4_MISTINGPIN) < 150) {
    if(DEBUG) printf_P(PSTR("LEVELS: Info: Misting water sensor active.\n\r"));
    states[S4_MISTING] = true;
  } else {
    if(DEBUG) printf_P(PSTR("LEVELS: Info: No water for misting!\n\r"));
    states[S4_MISTING] = false;
  }
}

/****************************************************************************/

void relayOn(const char* relay) {
  bool status = relays(relay, 0); // 0 is ON
  if(status) {
    if(DEBUG) printf_P(PSTR("RELAY: Info: '%s' is enabled.\n\r"), relay);
    states[relay] = true;
  }
}

void relayOff(const char* relay) {
  bool status = relays(relay, 1); // 1 is OFF
  if(status) {
    if(DEBUG) printf_P(PSTR("RELAY: Info: '%s' is disabled.\n\r"), relay);
    states[relay] = false;
  }
}

bool relays(const char* relay, uint8_t state) {
  if(strcmp(relay, P1_MISTING) == 0) {
  	pinMode(P1_MISTINGPIN, OUTPUT);
    digitalWrite(P1_MISTINGPIN, state);
    return true;
  } 
  if(strcmp(relay, P2_WATERING) == 0) {
    pinMode(P2_WATERINGPIN, OUTPUT);  	
    digitalWrite(P2_WATERINGPIN, state);
    return true;
  } 
  if(strcmp(relay, LAMP) == 0) {
  	pinMode(LAMPPIN, OUTPUT);
    digitalWrite(LAMPPIN, state);
    return true;
  }
  printf_P(PSTR("RELAY: Error: '%s' is unknown!\n\r"), relay);
  return false;
}

/****************************************************************************/

void doMisting() {
  uint16_t duration = 3000;
  
  if(start_misting == 0) {
  	relayOn(P1_MISTING);
  	start_misting = millis();
  	states[WARNING] = WARNING_MISTING;
  	if(DEBUG) printf_P(PSTR("MISTING: Info: Start.\n\r"));
  	return;
  } 

  if(start_misting > 0 && (millis() > start_misting + duration)) {
  	relayOff(P1_MISTING);
  	start_misting = -1;
  	states[WARNING] = NO_WARNING;
  	if(DEBUG) printf_P(PSTR("MISTING: Info: Stop.\n\r"));
  	return;
  }
}

/****************************************************************************/

void doWatering() {
  uint16_t duration = 60000;
  
  if(start_watering == 0) {
  	relayOn(P2_WATERING);
  	start_watering = millis();
  	states[WARNING] = WARNING_WATERING;
  	if(DEBUG) printf_P(PSTR("WATERING: Info: Start.\n\r"));
  	return;
  }

  if(start_watering > 0 && (millis() > start_watering + duration)) {
  	relayOff(P2_WATERING);
  	start_watering = 0;
  	states[WARNING] = WARNING_NO_SUBSTRATE;
  	printf_P(PSTR("MISTING: Error: No substrate!\n\r"));
  	return;
  } 
  else {
    //if(states[S2_WATERING] = SENSOR_ON)

    return;
  }

}

/****************************************************************************/

void system_check() {
  if(storage_ok == false) {
    printf_P(PSTR("EEPROM: Error!\n\r"));
    states[WARNING] = WARNING_EEPROM;
    return;
  }
  
  read_RTC();
  if(RTC.year < 2014 || RTC.year > 2018) {
    printf_P(PSTR("RTC: Wrong date!\n\r"));
    //lcdEditMenu(CLOCK, 4);
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

void doWork() {
  uint32_t now = millis();

  // day time
  if(settings.daytimeFrom <= RTC.hour && RTC.hour < settings.daytimeTo) {

    if(now > last_watering + (settings.wateringDayPeriod * ONE_MINUTE)) {
      //doWatering();
    }

    if(now > last_misting + (settings.mistingDayPeriod * ONE_MINUTE)) {
      //doMisting();
    }
  } else
  // night time 
  if(settings.nighttimeFrom <= RTC.hour || RTC.hour < settings.nighttimeTo) {

    if(now > last_watering + (settings.wateringNightPeriod * ONE_MINUTE)) {
      //doWatering();
    }

    if(now > last_misting + (settings.mistingNightPeriod * ONE_MINUTE)) {
      //doMisting();
    }
  } 
  // sunrise time
  else {

    if(now > last_watering + (settings.wateringSunrisePeriod * ONE_MINUTE)) {
      //doWatering();
    }

    if(now > last_misting + (settings.mistingSunrisePeriod * ONE_MINUTE)) {
      //doMisting();
    }
  }

  if(states[LIGHT] <= settings.lightThreshold) {
    relayOn(LAMP);
  } else {
    relayOff(LAMP);
  }

}

/****************************************************************************/

uint8_t lcdShowMenu(uint8_t _menuItem) {
   printf_P(PSTR("LCD Panel: Info: Show menu #%d.\n\r"), _menuItem);
  menuItem = _menuItem;

  lcd.setCursor(0,0);
  switch (_menuItem) {
    case HOME:
      fprintf(&lcdout, "Hydroponic %d:%d  ", RTC.hour, RTC.minute); 
      lcd.setCursor(0,1);
      fprintf(&lcdout, "tIn %dC, tOut %dC, Hum. %d, Light %d lux", 
      states[T_INSIDE], states[T_OUTSIDE], states[HUMIDITY], states[LIGHT]);
      lcdBlink(1, 13, 13);
      break;
    case WATERING_DAY:
      fprintf(&lcdout, "Watering daytime"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min     ", settings.wateringDayPeriod);
      break;
    case WATERING_NIGHT:
      fprintf(&lcdout, "Watering night  "); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min     ", settings.wateringNightPeriod);
      break;
    case WATERING_SUNRISE:
      fprintf(&lcdout, "Watering sunrise"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min     ", settings.wateringSunrisePeriod);
      break;
    case MISTING_DAY:
      fprintf(&lcdout, "Misting daytime "); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min     ", settings.mistingDayPeriod);
      break;
    case MISTING_NIGHT:
      fprintf(&lcdout, "Misting night   "); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min     ", settings.mistingNightPeriod);
      break;
    case MISTING_SUNRISE:
      fprintf(&lcdout, "Misting sunrise "); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min     ", settings.mistingSunrisePeriod);
      break;
    case DAY_TIME:
      fprintf(&lcdout, "Day time        "); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %dh to %dh   ", 
        settings.daytimeFrom, settings.daytimeTo);
      break;
    case NIGHT_TIME:
      fprintf(&lcdout, "Night time      "); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %dh to %dh   ", 
        settings.nighttimeFrom, settings.nighttimeTo);
      break;
    case LIGHT_THRESHOLD:
      fprintf(&lcdout, "Light on when   "); lcd.setCursor(0,1);
      fprintf(&lcdout, "lower %d lux     ", settings.lightThreshold);
      break;
    case LIGHT_DAY:
      fprintf(&lcdout, "Light day       "); lcd.setCursor(0,1);
      fprintf(&lcdout, "duration %dh     ", settings.lightDayDuration);
      break;
    case HUMIDITY_THRESHOLD:
      fprintf(&lcdout, "Humidity not    "); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %d     ", settings.humidThreshold);
      break;
    case T_OUTSIDE_THRESHOLD:
      fprintf(&lcdout, "Temperature not "); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %dC    ", settings.tempThreshold);
      break;
    case T_SUBSTRATE_THRESHOLD:
      fprintf(&lcdout, "Temperature not "); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %dC    ", settings.tempSubsThreshold);
      break;
    case CLOCK:
      fprintf(&lcdout, "Current time    "); lcd.setCursor(0,1);
      fprintf(&lcdout, "%d:%d %d-%d-%d       ", RTC.hour, RTC.minute, 
        RTC.day, RTC.month, RTC.year);
      break;   
    default:
      return HOME;
      break;
  }
  return _menuItem;
};

/****************************************************************************/

uint8_t lcdEditMenu(uint8_t _menuItem, uint8_t _editCursor) {
   printf_P(PSTR("LCD Panel: Info: Edit menu #%d with cursor #%d.\n\r"), 
      _menuItem, _editCursor);
  menuEditMode = true;
  menuItem = _menuItem;
  editCursor = _editCursor;

  lcd.setCursor(0,0);
  switch (_menuItem) {
    case WATERING_DAY:
      fprintf(&lcdout, "Changing period "); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min     ", settings.wateringDayPeriod);
      lcdBlink(1, 6, 8);
      return 0;
    case WATERING_NIGHT:
      fprintf(&lcdout, "Changing period "); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min     ", settings.wateringNightPeriod);
      lcdBlink(1, 6, 8);
      return 0;
    case WATERING_SUNRISE:
      fprintf(&lcdout, "Changing period "); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min     ", settings.wateringSunrisePeriod);
      lcdBlink(1, 6, 8);
      return 0;
    case MISTING_DAY:
      fprintf(&lcdout, "Changing period "); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min     ", settings.mistingDayPeriod);
      lcdBlink(1, 6, 8);
      return 0;
    case MISTING_NIGHT:
      fprintf(&lcdout, "Changing period "); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min     ", settings.mistingNightPeriod);
      lcdBlink(1, 6, 8);
      return 0;
    case MISTING_SUNRISE:
      fprintf(&lcdout, "Changing period "); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min     ", settings.mistingSunrisePeriod);
      lcdBlink(1, 6, 8);
      return 0;
    case DAY_TIME:
      fprintf(&lcdout, "Changing range  "); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %dh to %dh   ", 
        settings.daytimeFrom, settings.daytimeTo);
      if(_editCursor == 1) {
        lcdBlink(1, 12, 14);
        return 0; 
      } 
      lcdBlink(1, 5, 7);
      return 1;
    case NIGHT_TIME:
      fprintf(&lcdout, "Changing range  "); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %dh to %dh   ", 
        settings.nighttimeFrom, settings.nighttimeTo);
      if(_editCursor == 1) {
        lcdBlink(1, 12, 14);
        return 0; 
      } 
      lcdBlink(1, 5, 7);
      return 1;
    case LIGHT_THRESHOLD:
      fprintf(&lcdout, "Changing light  "); lcd.setCursor(0,1);
      fprintf(&lcdout, "lower %d lux     ", settings.lightThreshold);
      lcdBlink(1, 6, 11);
      return 0;
    case LIGHT_DAY:
      fprintf(&lcdout, "Changing light  "); lcd.setCursor(0,1);
      fprintf(&lcdout, "duration %dh     ", settings.lightDayDuration);
      lcdBlink(1, 10, 13);
      return 0;
    case HUMIDITY_THRESHOLD:
      fprintf(&lcdout, "Changing humid. "); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %d     ", settings.humidThreshold);
      lcdBlink(1, 10, 13);
      return 0;
    case T_OUTSIDE_THRESHOLD:
      fprintf(&lcdout, "Changing temp.  "); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %dC    ", settings.tempThreshold);
      lcdBlink(1, 10, 13);
      return 0;
    case T_SUBSTRATE_THRESHOLD:
      fprintf(&lcdout, "Changing temp.  "); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %02dC   ", settings.tempSubsThreshold);
      lcdBlink(1, 10, 13);
      return 0;   
    case CLOCK:
    case HOME:
      fprintf(&lcdout, "Setting time    "); lcd.setCursor(0,1);
      fprintf(&lcdout, "%02d:%02d %02d-%02d-%04d", RTC.hour, RTC.minute, 
        RTC.day, RTC.month, RTC.year);
      if(_editCursor == 4) {
        lcdBlink(1, 0, 1);
        return 0;
      } else 
      if(_editCursor == 3) {
        lcdBlink(1, 3, 4);
        return 1;
      } else 
      if(_editCursor == 2) {
        lcdBlink(1, 6, 7);
        return 2;
      } else 
      if(_editCursor == 1) {
        lcdBlink(1, 9, 10);
        return 3;
      }
      lcdBlink(1, 12, 15);
      return 4;
    default:
      return 0;
  }
};

/****************************************************************************/

void leftButtonClick() {
   
    printf_P(PSTR("LCD Panel: Info: Left click: Menu #%d, Cursor #%d.\n\r"),
      menuItem, editCursor);
  
  /*if(menuEditMode == false) {
    // move backward, previous menu
    lcdShowMenu(--menuItem);
  }

  storage.isChanged = true;
  switch (menuItem) {
    case WATERING_DAY:
      settings.wateringDayPeriod--;
      break;
    case WATERING_NIGHT:
      settings.wateringNightPeriod--;
      break;
    case WATERING_SUNRISE:
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
      if(editCursor == 1) {
        settings.daytimeFrom--;
      } else {
        settings.daytimeTo--;
      }
      break;
    case NIGHT_TIME:
      if(editCursor == 1) {
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
      if(editCursor == 4) {
        if(RTC.hour > 0)
          RTC.hour--;
      } else
      if(editCursor == 3) {
        if(RTC.minute > 0)
          RTC.minute--;
      } else
      if(editCursor == 2) {
        if(RTC.day > 1)
          RTC.day--;
      } else
      if(editCursor == 1) {
        if(RTC.month > 1)
          RTC.month--;
      } else {
        RTC.year--;
      }
  	  storage.isChanged = false;
      break;
    default:
      menuItem = HOME;
  }

  lcdEditMenu(menuItem, editCursor);*/
};

/****************************************************************************/

void rightButtonClick() {
   
    printf_P(PSTR("LCD Panel: Info: Right click: Menu #%d, Cursor #%d.\n\r"),
      menuItem, editCursor);
  
  /*if(menuEditMode == false) {
    // move forward, next menu
    lcdShowMenu(++menuItem);
  }
  
  storage.isChanged = true;
  switch (menuItem) {
    case WATERING_DAY:
      settings.wateringDayPeriod++;
      break;
    case WATERING_NIGHT:
      settings.wateringNightPeriod++;
      break;
    case WATERING_SUNRISE:
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
      if(editCursor == 1) {
        settings.daytimeFrom++;
      } else {
        settings.daytimeTo++;
      }
      break;
    case NIGHT_TIME:
      if(editCursor == 1) {
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
      if(editCursor == 4) {
        if(RTC.hour < 23)
          RTC.hour++;
      } else
      if(editCursor == 3) {
        if(RTC.minute < 59)
          RTC.minute++;
      } else
      if(editCursor == 2) {
        if(RTC.day < 31)
          RTC.day++;
      } else
      if(editCursor == 1) {
        if(RTC.month < 12)
          RTC.month++;
      } else {
        RTC.year++;
      }
  	  storage.isChanged = false;
      break;
    default:
      menuItem = HOME;
  }

  lcdEditMenu(menuItem, editCursor);*/
}

/****************************************************************************/

void buttonsLongPress() {
   printf_P(PSTR("LCD Panel: Info: LongPress callback: Menu #%d.\n\r"),
    menuItem);

  /*if(menuEditMode == false) {
    menuEditMode = true;
    editCursor = lcdEditMenu(menuItem, editCursor);
    return;
  }
  
  if(editCursor != 0) {
    // move to next edit field
    lcdEditMenu(menuItem, --editCursor);
    return;
  } 
  else {
    menuEditMode = false;
  }

  if(menuItem == CLOCK) {
    RTC.setTime();
    RTC.startClock();
  }

  lcdShowMenu(menuItem);*/
}

/****************************************************************************/

void lcdBlink(uint8_t _row, uint8_t _start, uint8_t _end) { 
  if(blink) {
    if(DEBUG) printf_P(PSTR("LCD: Info: Blink for: %d:%d-%d\n\r"),
      _row, _start, _end); 
    
    while(_start <= _end) {
      lcd.setCursor(_start, _row); lcd.print(" ");
      _start++;
    }
    blink = false;
    return;
  }
  blink = true; 
  return;
}

/****************************************************************************/

void lcdWarning() {
  if(DEBUG) printf_P(PSTR("LCD Panel: Info: Show Warning.\n\r"));

  lcd.setBacklight(false); delay(250);
  lcd.setBacklight(true); delay(250);
  lcd.setCursor(0,0);
  switch (states[WARNING]) {
    case WARNING_NO_WATER:
      fprintf(&lcdout, "No water! :(    "); lcd.setCursor(0,1);
      fprintf(&lcdout, "Can’t misting!  ");
      lcdBlink(1, 0, 13);
      return;
    case WARNING_SUBSTRATE_FULL:
      fprintf(&lcdout, "Substrate tank  "); lcd.setCursor(0,1);
      fprintf(&lcdout, "is full! :)     ");
      return;
    case WARNING_SUBSTRATE_LOW:
      fprintf(&lcdout, "Low substrate!  "); lcd.setCursor(0,1);
      fprintf(&lcdout, ":( Please add.  ");
      return;
    case WARNING_NO_SUBSTRATE:
      fprintf(&lcdout, "Can't watering! "); lcd.setCursor(0,1);
      fprintf(&lcdout, "Plants could die");
      lcdBlink(0, 0, 14);
      return;
    case WARNING_DONE:
      fprintf(&lcdout, "Watering done!  "); lcd.setCursor(0,1);
      fprintf(&lcdout, ":) wait... 10m  ");
      return;
    case WARNING_WATERING:
      fprintf(&lcdout, "Watering...     "); lcd.setCursor(0,1);
      fprintf(&lcdout, "Please wait.    ");
      return;
    case WARNING_MISTING:
      fprintf(&lcdout, "Misting...      "); lcd.setCursor(0,1);
      fprintf(&lcdout, "Please wait.    ");
      return;
    case WARNING_TEMPERATURE_COLD:
      fprintf(&lcdout, "Is too cold     "); lcd.setCursor(0,1);
      fprintf(&lcdout, "for plants! :(  ");
      lcdBlink(0, 0, 10);
      return;
    case WARNING_SUBSTRATE_COLD:
      fprintf(&lcdout, "Substrate is    "); lcd.setCursor(0,1);
      fprintf(&lcdout, "too cold! :O    ");
      lcdBlink(1, 10, 11);
      return;
    case WARNING_EEPROM:
      fprintf(&lcdout, "EEPROM ERROR!   "); lcd.setCursor(0,1);
      fprintf(&lcdout, "Settings reset! ");
      lcdBlink(1, 0, 15);
      return;
    case WARNING_DHT11:
      fprintf(&lcdout, "DHT11 ERROR!    "); lcd.setCursor(0,1);
      fprintf(&lcdout, "Check connection");
      lcdBlink(1, 0, 15);
      return;
    case WARNING_DS18B20:
      fprintf(&lcdout, "DS18B20 ERROR!  "); lcd.setCursor(0,1);
      fprintf(&lcdout, "Check connection");
      lcdBlink(1, 0, 15);
      return;
    case WARNING_BH1750:
      fprintf(&lcdout, "BH1750 ERROR!   "); lcd.setCursor(0,1);
      fprintf(&lcdout, "Check connection");
      lcdBlink(1, 0, 15);
      return;
  }  
}

/****************************************************************************/
