// Import libraries
//#include <avr/pgmspace.h>
#include <Wire.h>
#include "LiquidCrystal_I2C.h"
#include "MemoryFree.h"
#include "Watchdog.h"
#include "Timer.h"
#include "SimpleMap.h"
#include "Settings.h"
#include "OneButton.h"
#include "DS1307new.h"
#include "DHT11.h"
#include "OneWire.h"
#include "DallasTemperature.h"
#include "BH1750.h"
#include "Melody.h"

// debug console
#define DEBUG
//#define DEBUG_LCD
//#define DEBUG_RTC

// Declare LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);
// Declare custom LCD characters
uint8_t celcium_char[8] = {24, 24, 3, 4, 4, 4, 3, 0};
const uint8_t celcium_c = 0;
uint8_t heart_char[8] = {0, 10, 21, 17, 10, 4, 0, 0};
const uint8_t heart_c = 1;
uint8_t humidity_char[8] = {4, 10, 10, 17, 17, 17, 14, 0};
const uint8_t humidity_c = 2;
uint8_t temp_char[8] = {4, 10, 10, 14, 31, 31, 14, 0};
const uint8_t temp_c = 3;
uint8_t flower_char[8] = {14, 27, 21, 14, 4, 12, 4, 0};
const uint8_t flower_c = 4;
uint8_t lamp_char[8] = {14, 17, 17, 17, 14, 14, 4, 0};
const uint8_t lamp_c = 5;

// Declare output functions
static int serial_putchar(char c, FILE *) {
  Serial.write(c);
  return 0;
};
FILE serial_out = {0};
static int lcd_putchar(char c, FILE *) {
  lcd.write(c);
  return 0;
};
FILE lcd_out = {0};
// Avoid spurious warnings
#if ! defined( NATIVE ) && defined( ARDUINO )
#undef PROGMEM
#define PROGMEM __attribute__(( section(".progmem.data") ))
#undef PSTR
#define PSTR(s) (__extension__({static const char __c[] PROGMEM = (s); &__c[0];}))
#endif

// Declare delay managers
timer_t fast_timer(1000);
timer_t slow_timer(30000);

// Declare state map
struct comparator {
  bool operator()(const char* &str1, const char* &str2) {
    return strcmp(str1, str2) == 0;
  }
};
SimpleMap<const char*, int, 15, comparator> states;

// Declare push buttons
OneButton rightButton(A2, true);
OneButton leftButton(A3, true);

// Set up Speaker digital pin
Melody melody(8);

// Declare constants
#define CDN  "cdn" // days after 2000-01-01
#define DTIME  "dtime" // minutes after 00:00
#define HUMIDITY  "humidity" // air humidity
#define AIR_TEMP  "air temp"
#define COMPUTER_TEMP  "comp. temp" // temperature inside
#define SUBSTRATE_TEMP  "subs. temp"
#define LIGHT  "light" // light intensivity
#define PUMP_MISTING  "misting"
#define PUMP_WATERING  "watering"
#define LAMP  "lamp"
#define WARNING  "warning"
#define ERROR  "error"

// Declare warning states
#define NO_WARNING  0
#define INFO_SUBSTRATE_FULL  1
#define WARNING_SUBSTRATE_LOW  2
#define INFO_SUBSTRATE_DELIVERED  3
#define WARNING_WATERING  4
#define WARNING_MISTING  5
#define WARNING_AIR_COLD  6
#define WARNING_SUBSTRATE_COLD  7
#define WARNING_NO_WATER  8

// Declare error states
#define NO_ERROR  0
#define ERROR_LOW_MEMORY  10
#define ERROR_EEPROM  11
#define ERROR_DHT11  12
#define ERROR_BH1750  13
#define ERROR_DS18B20  14
#define ERROR_NO_SUBSTRATE  15

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
#define T_AIR_THRESHOLD  12
#define T_SUBSTRATE_THRESHOLD  13
#define TEST 14
#define CLOCK  15

// Declare pins
#define DHT11PIN  3
#define ONE_WIRE_BUS  2
#define SUBSTRATE_FULLPIN  A0
#define SUBSTRATE_DELIVEREDPIN  A1
#define WATER_LEVELPIN  A6
#define SUBSTRATE_LEVELPIN  A7
#define PUMP_WATERINGPIN  4
#define PUMP_MISTINGPIN  5
#define LAMPPIN  7

// Declare variables
EEPROM storage;
bool storage_ok;
uint16_t last_misting = 0;
uint16_t last_watering = 0;
uint16_t last_touch = 0;
uint8_t misting_duration = 0;
uint16_t start_watering = 0;
bool menuEditMode;
uint8_t menuEditCursor;
uint8_t menuItem;
uint8_t menuHomeItem;
bool lcdBlink;
bool lcdBacklight = true;
bool substrate_full;

/****************************************************************************/

//
// Setup
//
void setup()
{
  // Configure output
  Serial.begin(9600);
  fdev_setup_stream(&serial_out, serial_putchar, NULL, _FDEV_SETUP_WRITE);
  fdev_setup_stream(&lcd_out, lcd_putchar, NULL, _FDEV_SETUP_WRITE);
  stdout = stderr = &serial_out;

  // Configure lcd
  lcd.begin();
  // load custom characters
  lcd.createChar(celcium_c, celcium_char);
  lcd.createChar(heart_c, heart_char);
  lcd.createChar(humidity_c, humidity_char);
  lcd.createChar(temp_c, temp_char);
  lcd.createChar(flower_c, flower_char);
  lcd.createChar(lamp_c, lamp_char);
  lcd.home();
  
  #ifdef DEBUG
    printf_P(PSTR("Free memory: %d bytes.\n\r"), freeMemory());
  #endif
  // prevent continiously restart
  delay(500);
  // restart if memory lower 512 bytes
  softResetMem(512);
  // restart after freezing for 8 sec
  softResetTimeout();

  // Load settings
  storage_ok = storage.load();

  // Configure buttons
  rightButton.attachClick( rightButtonClick );
  rightButton.attachLongPressStart( buttonsLongPress );
  leftButton.attachClick( leftButtonClick );
  leftButton.attachLongPressStart( buttonsLongPress );

  // "R2D2" melody
  melody.play(R2D2); 
}

//
// Loop
//
void loop()
{
  // watchdog
  heartbeat();

  if(fast_timer) {
    // check level sensors
    check_levels();
    // update watering
    watering();
    // update misting
    misting();
    // prevent redundant update
    if(menuEditMode == false) {
      // update clock
      read_RTC();
    }
    // manage LCD
    if(states[ERROR] != NO_ERROR)
      lcdAlert();
    else
    if(states[WARNING] != NO_WARNING)
      lcdWarning();
    else
    if(menuItem != HOME && 
        last_touch+60 < seconds() &&
        menuItem != TEST)
      lcdShowMenu(HOME);
    else
    if(menuEditMode)
      lcdEditMenu(menuItem, menuEditCursor);
    else
      lcdShowMenu(menuItem);
  }

  if(slow_timer) {
    // system check
    doCheck();
    // manage light
    doLight();
    // manage misting and watering
    doWork();
    // save settings
    if(storage.changed && storage_ok) {
      // WARNING: EEPROM can burn!
      storage_ok = storage.save();
      storage.changed = false;
    }
    // LCD sleeping after 5 min
    if(lcdBacklight && last_touch + 5*60 < seconds()) {
      // switch off backlight
      lcdBacklightToggle();
    }
  }
  
  // update push buttons
  leftButton.tick();
  rightButton.tick();
  // update melody
  melody.update();
}

/****************************************************************************/

void read_RTC() {
  //RTC.setRAM(0, (uint8_t *)0x0000, sizeof(uint16_t));
  //RTC.getRAM(54, (uint8_t *)0xaa55, sizeof(uint16_t));
  RTC.getTime();
  states[CDN] = RTC.cdn;
  states[DTIME] = RTC.hour*60+RTC.minute;

  #ifdef DEBUG_RTC
    printf_P(
      PSTR("RTC: Info: CDN: %d %02d-%02d-%4d, DTIME: %d %02d:%02d:%02d.\n\r"), 
        states[CDN], RTC.day, RTC.month, RTC.year, 
        states[DTIME], RTC.hour, RTC.minute, RTC.second);
  #endif
}

/*void set_RTC(uint16_t _cdn, uint16_t _dtime) {
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
}*/

bool read_DHT11() {
  dht11 DHT11;
  if( DHT11.read(DHT11PIN) == DHTLIB_OK ) {
    states[HUMIDITY] = DHT11.humidity;
    states[AIR_TEMP] = DHT11.temperature;
    #ifdef DEBUG
      printf_P(PSTR("DHT11: Info: Air humidity: %d, temperature: %dC.\n\r"), 
        states[HUMIDITY], states[AIR_TEMP]);
    #endif
    return true;
  }
  printf_P(PSTR("DHT11: Error: Communication failed!\n\r"));
  return false;
}

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
  #ifdef DEBUG
    printf_P(PSTR("DS18B20: Info: Computer temperature: %dC.\n\r"), value);
  #endif
  states[COMPUTER_TEMP] = value;
  
  value = dallas.getTempCByIndex(1);
  if(value == DEVICE_DISCONNECTED_C) {
    printf_P(PSTR("DS18B20: Error: Substrate sensor communication failed!\n\r"));
    return false;
  }
  #ifdef DEBUG
    printf_P(PSTR("DS18B20: Info: Substrate temperature: %dC.\n\r"), value);
  #endif
  states[SUBSTRATE_TEMP] = value;
  return true;
}

bool read_BH1750() {
  BH1750 lightMeter;
  lightMeter.begin(BH1750_ONE_TIME_HIGH_RES_MODE_2);
  uint16_t value = lightMeter.readLightLevel();

  if(value < 0) {
    printf_P(PSTR("BH1750: Error: Light sensor communication failed!\n\r"));
    return false;
  }
  #ifdef DEBUG
    printf_P(PSTR("BH1750: Info: Light intensity: %d.\n\r"), value);
  #endif
  states[LIGHT] = value;
  return true;
}

void check_levels() {
  // no pull-up for A6 and A7
  pinMode(SUBSTRATE_LEVELPIN, INPUT);
  if(analogRead(SUBSTRATE_LEVELPIN) > 700) {
    states[ERROR] = ERROR_NO_SUBSTRATE;
    return;
  }
  pinMode(SUBSTRATE_DELIVEREDPIN, INPUT_PULLUP);
  if(digitalRead(SUBSTRATE_DELIVEREDPIN) == 1) {
    states[WARNING] = INFO_SUBSTRATE_DELIVERED;
    return;
  }
  // no pull-up for A6 and A7
  pinMode(WATER_LEVELPIN, INPUT);
  if(analogRead(WATER_LEVELPIN) > 700) {
    states[WARNING] = WARNING_NO_WATER;
    return;
  }
  pinMode(SUBSTRATE_FULLPIN, INPUT_PULLUP);
  if(digitalRead(SUBSTRATE_FULLPIN) == 1) { 	  
  	if(substrate_full == false) {
      substrate_full = true;
      states[WARNING] = INFO_SUBSTRATE_FULL;
      return;
    }
  } else {
  	substrate_full = false;
  }
}

void relayOn(const char* relay) {
  if(states[relay]) {
    // relay is already on
    return;
  }
  bool status = relays(relay, 0); // 0 is ON
  if(status) {
    #ifdef DEBUG
      printf_P(PSTR("RELAY: Info: '%s' is enabled.\n\r"), relay);
    #endif
    states[relay] = true;
  }
}

void relayOff(const char* relay) {
  if(states[relay] == false) {
    // relay is already off
    return;
  }
  bool status = relays(relay, 1); // 1 is OFF
  if(status) {
    #ifdef DEBUG
      printf_P(PSTR("RELAY: Info: '%s' is disabled.\n\r"), relay);
    #endif
    states[relay] = false;
  }
}

bool relays(const char* relay, uint8_t state) {
  if(strcmp(relay, PUMP_MISTING) == 0) {
    pinMode(PUMP_MISTINGPIN, OUTPUT);
    digitalWrite(PUMP_MISTINGPIN, state);
    return true;
  } 
  if(strcmp(relay, PUMP_WATERING) == 0) {
    pinMode(PUMP_WATERINGPIN, OUTPUT);    
    digitalWrite(PUMP_WATERINGPIN, state);
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

void doCheck() {
  #ifdef DEBUG
    printf_P(PSTR("Free memory: %d bytes.\n\r"), freeMemory());
  #endif
  // check memory
  if(freeMemory() < 600) {
    printf_P(PSTR("ERROR: Low memory!!!\n\r"));
    states[ERROR] = ERROR_LOW_MEMORY;
    return;
  }
  // check EEPROM
  if(storage_ok == false) {
    states[ERROR] = ERROR_EEPROM;
    return;
  }
  // read and check DHT11 sensor
  if(read_DHT11() == false) {
    states[ERROR] = ERROR_DHT11;
    return;
  }
  // read and check BH1750 sensor
  if(read_BH1750() == false) {
    states[ERROR] = ERROR_BH1750;
    return;
  }
  // read and check DS18B20 sensor
  if(read_DS18B20() == false) {
    states[ERROR] = ERROR_DS18B20;
    return;
  }
  // reset error
  states[ERROR] = NO_ERROR;

  // check substrate temperature
  if(states[SUBSTRATE_TEMP] <= settings.tempSubsThreshold) {
    states[WARNING] = WARNING_SUBSTRATE_COLD;
    return;
  }
  // check air temperature
  if(states[AIR_TEMP] <= settings.tempThreshold) {
    states[WARNING] = WARNING_AIR_COLD;
    return;
  }
  // check humidity
  if(states[HUMIDITY] <= settings.humidThreshold) {
    misting_duration = 3; // do for 3 sec
  }
  // reset warning
  states[WARNING] = NO_WARNING;
}

void doTest(bool start) { 
  if(start && settings.lightThreshold != 3000) {
    #ifdef DEBUG
      printf_P(PSTR("Test: Info: enable.\n\r"));
    #endif
    // save previous settings
    test = settings;
    // change settings for test
    settings.lightThreshold = 3000;
    settings.sunrise = states[DTIME];
    settings.mistingDayPeriod = 1;
    settings.mistingNightPeriod = 1;
    settings.mistingSunrisePeriod = 1;
    settings.wateringDayPeriod = 3;
    settings.wateringNightPeriod = 3;
    settings.wateringSunrisePeriod = 3;
    misting_duration = 15;
    // manage test settings
    doWork();
    return;
  }
  if(start == false &&
      settings.lightThreshold == 3000) {
    #ifdef DEBUG
      printf_P(PSTR("Test: Info: disable.\n\r"));
    #endif    
    // restore previous settings
    settings = test;
    // turn all off
    start_misting = 0;
    relayOff(PUMP_WATERING);
    relayOff(LAMP);
    return;
  }
}

void doWork() {
  // don't do any work while error
  if(states[ERROR] != NO_ERROR) {
    return;
  }
  // day time
  if(settings.daytimeFrom <= RTC.hour && RTC.hour < settings.daytimeTo) {
    #ifdef DEBUG
      printf_P(PSTR("Work: Info: Day time.\n\r"));
    #endif
    if(seconds() > last_watering + (settings.wateringDayPeriod*60)) {
      start_watering = seconds();
    }
    if(seconds() > last_misting + (settings.mistingDayPeriod*60)) {
      misting_duration = 3; // do for 3 sec
    }
    return;
  }
  // night time 
  if(settings.nighttimeFrom <= RTC.hour || RTC.hour < settings.nighttimeTo) {
    #ifdef DEBUG
      printf_P(PSTR("Work: Info: Night time.\n\r"));
    #endif
    if(seconds() > last_watering + (settings.wateringNightPeriod*60)) {
      start_watering = seconds();
    }
    if(seconds() > last_misting + (settings.mistingNightPeriod*60)) {
      misting_duration = 3; // do for 3 sec
    }
    return;
  }
  #ifdef DEBUG
    printf_P(PSTR("Work: Info: Between day and night time.\n\r"));
  #endif
  // sunrise or sunset time
  if(seconds() > last_watering + (settings.wateringSunrisePeriod*60)) {
    start_watering = seconds();
  }
  if(seconds() > last_misting + (settings.mistingSunrisePeriod*60)) {
    misting_duration = 3; // do for 3 sec
  }
}

void doLight() {
  // try to up temperature
  if(states[AIR_TEMP] <= settings.tempThreshold) {
    // turn on lamp
    relayOn(LAMP);
    return;
  }
  // light enough
  if(states[LIGHT] > settings.lightThreshold) {
    // turn off lamp
    relayOff(LAMP);
    // capture time
    if(last_light == 0) {
      last_light = seconds();
      return;
    }
    // watch for 30 minutes
    if(RTC.hour <= 8 && RTC.hour > 3 && 
        seconds() > last_light + 30*60) {
      last_light = seconds();
      // sunrise in minutes
      settings.sunrise = states[DTIME]-30;
  	  // mark for save to EEPROM
  	  storage.changed = true;
      #ifdef DEBUG
        printf_P(PSTR("Light: Info: New sunrise at: %02d:%02d.\n\r"), 
          settings.sunrise/60, settings.sunrise%60);
      #endif
      return;
    }
    return;
  }
  #ifdef DEBUG
    printf_P(PSTR("Light: Info: Today sunrise at: %02d:%02d.\n\r"),
      settings.sunrise/60, settings.sunrise%60);
  #endif
  // keep duration of light day
  uint16_t lampOn = settings.sunrise+(settings.lightDayDuration*60);
  if(settings.sunrise > 0 && states[DTIME] <= lampOn) {
    #ifdef DEBUG
      printf_P(PSTR("Light: Info: Lamp On till: %02d:%02d.\n\r"), 
        lampOn/60, lampOn%60);
    #endif
    // turn on lamp
    relayOn(LAMP);
    return;
  }
  // turn off lamp
  relayOff(LAMP);
}

void misting() {
  if(misting_duration == 0) {
    // stop misting
    if(states[PUMP_MISTING]) {
      #ifdef DEBUG
        printf_P(PSTR("Misting: Info: Stop misting.\n\r"));
      #endif
      relayOff(PUMP_MISTING);
      if(states[WARNING] == WARNING_MISTING)
        states[WARNING] = NO_WARNING;
    }
    return;
  }
  #ifdef DEBUG
    printf_P(PSTR("Misting: Info: Misting...\n\r"));
  #endif
  states[WARNING] = WARNING_MISTING;
  misting_duration--;
  last_misting = seconds();
  relayOn(PUMP_MISTING);
}

void watering() {
  if(start_watering == 0 && 
  	  states[PUMP_WATERING] == false) {
    return;
  }
  // emergency stop
  if(states[WARNING] == ERROR_NO_SUBSTRATE ||
  	  start_watering +180 <= seconds()) {
    relayOff(PUMP_WATERING);
    states[WARNING] = WARNING_SUBSTRATE_LOW;
    start_watering = 0;
    printf_P(PSTR("Watering: Error: Emergency stop watering.\n\r"));
    return;
  }
  // stop watering
  if(states[WARNING] == INFO_SUBSTRATE_DELIVERED) {
    #ifdef DEBUG
      printf_P(PSTR("Watering: Info: Stop watering.\n\r"));
    #endif
    relayOff(PUMP_WATERING);
    start_watering = 0;
    if(states[WARNING] == WARNING_WATERING)
      states[WARNING] = NO_WARNING;
    return;
  }
  // pause for clean pump for 15 sec
  if(start_watering +15 <= seconds() &&
      start_watering +15+15 > seconds()) {
    #ifdef DEBUG
      printf_P(PSTR("Watering: Info: Pause for clean up.\n\r"));
    #endif
    relayOff(PUMP_WATERING);
    return;
  }
  #ifdef DEBUG
    printf_P(PSTR("Misting: Info: Start or Resume watering.\n\r"));
  #endif
  states[WARNING] = WARNING_WATERING;
  last_watering = seconds();
  relayOn(PUMP_WATERING);
}

uint16_t seconds() {
  return millis()/1000;
}

void lcdBacklightBlink(uint8_t _count) {
  lcdBacklight = true;
  lcd.setBacklight(true);
  for(uint8_t i=0; i<_count; i++) {
    lcdBacklightToggle(); delay(250);
    lcdBacklightToggle(); delay(250);
  }
}

void lcdBacklightToggle() {
  lcd.setBacklight(!lcdBacklight);
  lcdBacklight = !lcdBacklight;
}

void lcdShowMenu(uint8_t _menuItem) {
  #ifdef DEBUG_LCD
    printf_P(PSTR("LCD Panel: Info: Show menu #%d.\n\r"), _menuItem);
  #endif
  menuEditMode = false;
  menuItem = _menuItem;

  lcd.home();
  switch (menuItem) {
    case HOME:
      showHomeScreen();
      return;
    case WATERING_DAY:
      fprintf_P(&lcd_out, PSTR("Watering daytime")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("every %3d min   "), 
        settings.wateringDayPeriod);
      return;
    case WATERING_NIGHT:
      fprintf_P(&lcd_out, PSTR("Watering night  ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("every %3d min   "), 
        settings.wateringNightPeriod);
      return;
    case WATERING_SUNRISE:
      fprintf_P(&lcd_out, PSTR("Watering sunrise")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("every %3d min   "), 
        settings.wateringSunrisePeriod);
      return;
    case MISTING_DAY:
      fprintf_P(&lcd_out, PSTR("Misting daytime ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("every %3d min   "),
        settings.mistingDayPeriod);
      return;
    case MISTING_NIGHT:
      fprintf_P(&lcd_out, PSTR("Misting night   ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("every %3d min   "), 
        settings.mistingNightPeriod);
      return;
    case MISTING_SUNRISE:
      fprintf_P(&lcd_out, PSTR("Misting sunrise ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("every %3d min   "), 
        settings.mistingSunrisePeriod);
      return;
    case DAY_TIME:
      fprintf_P(&lcd_out, PSTR("Day time        ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("from %2dh to %2dh "), 
        settings.daytimeFrom, settings.daytimeTo);
      return;
    case NIGHT_TIME:
      fprintf_P(&lcd_out, PSTR("Night time      ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("from %2dh to %2dh "), 
        settings.nighttimeFrom, settings.nighttimeTo);
      return;
    case LIGHT_THRESHOLD:
      fprintf_P(&lcd_out, PSTR("Light on when   ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("lower %3d lux   "), 
        settings.lightThreshold);
      return;
    case LIGHT_DAY:
      fprintf_P(&lcd_out, PSTR("Light day       ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("duration %2dh    "), 
        settings.lightDayDuration);
      return;
    case HUMIDITY_THRESHOLD:
      fprintf_P(&lcd_out, PSTR("Humidity not    ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("less than %2d%%   "), 
        settings.humidThreshold);
      return;
    case T_AIR_THRESHOLD:
      fprintf_P(&lcd_out, PSTR("Temp. air not   ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("less than %2d%c   "), 
        settings.tempThreshold, celcium_c);
      return;
    case T_SUBSTRATE_THRESHOLD:
      fprintf_P(&lcd_out, PSTR("Substrate temp. ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("less than %2d%c   "), 
        settings.tempSubsThreshold, celcium_c);
      return;
    case TEST:
      fprintf_P(&lcd_out, PSTR("Test all systems")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("       -> Start?"));
      lcdTextBlink(1, 10, 15);
      doTest(false);
      return;
    case 255:  
      menuItem = CLOCK;
    case CLOCK:
      fprintf_P(&lcd_out, PSTR("Time:   %02d:%02d:%02d"), 
        RTC.hour, RTC.minute, RTC.second); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("Date: %02d-%02d-%4d"), 
        RTC.day, RTC.month, RTC.year);
      return;
    default:
      menuItem = HOME;
      menuHomeItem = 0;
  }
}

void showHomeScreen() {
  #ifdef DEBUG_LCD
    printf_P(PSTR("LCD: Info: Home screen #%d.\n\r"), menuHomeItem);
  #endif
  fprintf_P(&lcd_out, PSTR("%c%c%c%c%c%c%c    %02d:%02d"), 
    flower_c, flower_c, flower_c, flower_c, heart_c, 
    flower_c, heart_c, RTC.hour, RTC.minute);
  lcdTextBlink(0, 13, 13);
  lcd.setCursor(0,1);

  if(menuHomeItem >= 16)
    menuHomeItem = 0;
  switch (menuHomeItem) {
    case 0:
      fprintf_P(&lcd_out, PSTR("Air: %c %2d%c %c %2d%%"), 
        temp_c, states[AIR_TEMP], celcium_c, humidity_c, states[HUMIDITY]);
      break;
    case 4:
      fprintf_P(&lcd_out, PSTR("Substrate: %c %2d%c"), 
        temp_c, states[SUBSTRATE_TEMP], celcium_c);
      break;
    case 8:
      fprintf_P(&lcd_out, PSTR("Light: %c %3d lux"), 
        lamp_c, states[LIGHT]);
      break;
    case 12:
      fprintf_P(&lcd_out, PSTR("Computer:  %c %2d%c"), 
        temp_c, states[COMPUTER_TEMP], celcium_c);
      break;
  }
  menuHomeItem++;
}

void lcdTextBlink(bool _row, uint8_t _start, uint8_t _end) { 
  if(lcdBlink) {
    #ifdef DEBUG_LCD 
      printf_P(PSTR("LCD: Info: Blink: row #%d from %d to %d.\n\r"),
        _row, _start, _end);
    #endif 
    while(_start <= _end) {
      lcd.setCursor(_start, _row); fprintf_P(&lcd_out, PSTR(" "));
      _start++;
    }
    lcdBlink = false;
  } else {
    lcdBlink = true;   
  }
}

uint8_t lcdEditMenu(uint8_t _menuItem, uint8_t _editCursor) {
  #ifdef DEBUG_LCD
    printf_P(PSTR("LCD: Info: Edit menu #%d with cursor #%d.\n\r"), 
      _menuItem, _editCursor);
  #endif
  // save states
  menuEditMode = true;
  menuItem = _menuItem;
  menuEditCursor = _editCursor;

  lcd.home();
  switch (menuItem) {
    case WATERING_DAY:
      return lcdEditMenuChangingPeriod(settings.wateringDayPeriod);
    case WATERING_NIGHT:
      return lcdEditMenuChangingPeriod(settings.wateringNightPeriod);  
    case WATERING_SUNRISE:
      return lcdEditMenuChangingPeriod(settings.wateringSunrisePeriod);
    case MISTING_DAY:
      return lcdEditMenuChangingPeriod(settings.mistingDayPeriod);
    case MISTING_NIGHT:
      return lcdEditMenuChangingPeriod(settings.mistingNightPeriod);
    case MISTING_SUNRISE:
      return lcdEditMenuChangingPeriod(settings.mistingSunrisePeriod);
    case DAY_TIME:
      return lcdEditMenuChangingRange(settings.daytimeFrom, 
        settings.daytimeTo);
    case NIGHT_TIME:
      return lcdEditMenuChangingRange(settings.nighttimeFrom, 
        settings.nighttimeTo);
    case LIGHT_THRESHOLD:
      fprintf_P(&lcd_out, PSTR("Changing light  ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("activity %3d lux"), 
        settings.lightThreshold);
      lcdTextBlink(1, 9, 11);
      return 0;
    case LIGHT_DAY:
      fprintf_P(&lcd_out, PSTR("Changing light  ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("duration %2dh    "), 
        settings.lightDayDuration);
      lcdTextBlink(1, 9, 10);
      return 0;
    case HUMIDITY_THRESHOLD:
      fprintf_P(&lcd_out, PSTR("Changing humid. ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("less than %2d%%   "), 
        settings.humidThreshold);
      lcdTextBlink(1, 10, 11);
      return 0;
    case T_AIR_THRESHOLD:
      fprintf_P(&lcd_out, PSTR("Changing temp.  ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("less than %2d%c   "), 
        settings.tempThreshold, celcium_c);
      lcdTextBlink(1, 10, 11);
      return 0;
    case T_SUBSTRATE_THRESHOLD:
      fprintf_P(&lcd_out, PSTR("Changing temp.  ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("less than %2d%c   "), 
        settings.tempSubsThreshold, celcium_c);
      lcdTextBlink(1, 10, 11);
      return 0;
    case TEST:
      fprintf_P(&lcd_out, PSTR("Testing.....    ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("        -> Stop?"));
      lcdTextBlink(1, 11, 15);
      // testing
      doTest(true);
      return 0;
    case CLOCK:
      fprintf_P(&lcd_out, PSTR("Setting time    ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("%02d:%02d %02d-%02d-%4d"), 
        RTC.hour, RTC.minute, RTC.day, RTC.month, RTC.year);
      if(menuEditCursor == 4) {
        lcdTextBlink(1, 0, 1);
      } else 
      if(menuEditCursor == 3) {
        lcdTextBlink(1, 3, 4);
      } else 
      if(menuEditCursor == 2) {
        lcdTextBlink(1, 6, 7);
      } else 
      if(menuEditCursor == 1) {
        lcdTextBlink(1, 9, 10);
      } else {
        lcdTextBlink(1, 12, 15);       
      }
      return 4;
    default:
      menuItem = HOME;
      menuHomeItem = 0;
      return 0;
  }
}

bool lcdEditMenuChangingPeriod(uint8_t _period) {
  fprintf_P(&lcd_out, PSTR("Changing period ")); lcd.setCursor(0,1);
  fprintf_P(&lcd_out, PSTR("every %3d min.    "), _period);
  lcdTextBlink(1, 6, 8);
  return 0;
}

bool lcdEditMenuChangingRange(uint8_t _from, uint8_t _to) {
  fprintf_P(&lcd_out, PSTR("Changing range  ")); lcd.setCursor(0,1);
  fprintf_P(&lcd_out, PSTR("from %2dh to %2dh "), _from, _to);
  if(menuEditCursor == 1) {
    lcdTextBlink(1, 5, 6);
  } else {
    lcdTextBlink(1, 12, 13);   
  }
  return 1;
}

void lcdWarning() {
  #ifdef DEBUG_LCD
    printf_P(PSTR("LCD: Info: Show Warning #%d.\n\r"), states[WARNING]);
  #endif
  last_touch = seconds();
  lcd.setBacklight(true);
  
  lcd.home();
  switch (states[WARNING]) { 
    case WARNING_SUBSTRATE_LOW:
      fprintf_P(&lcd_out, PSTR("Low substrate!  ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("Please add water"));
      lcdTextBlink(1, 0, 15);
      return;
    case INFO_SUBSTRATE_FULL:
      fprintf_P(&lcd_out, PSTR("Substrate tank  ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("is full! :)))   "));
      melody.beep(1);
      return;
    case INFO_SUBSTRATE_DELIVERED:
      fprintf_P(&lcd_out, PSTR("Substrate was   ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("delivered! :))) "));
      return;
    case WARNING_WATERING:
      fprintf_P(&lcd_out, PSTR("Watering...     ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("Please wait.    "));
      lcdTextBlink(1, 0, 11);
      return;
    case WARNING_MISTING:
      fprintf_P(&lcd_out, PSTR("Misting...      ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("Please wait.    "));
      lcdTextBlink(1, 0, 11);
      return;
    case WARNING_AIR_COLD:
      fprintf_P(&lcd_out, PSTR("Air is too cold ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("for plants! :(  "));
      lcdTextBlink(1, 12, 13);
      return;
    case WARNING_SUBSTRATE_COLD:
      fprintf_P(&lcd_out, PSTR("Substrate is too")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("cold! :(        "));
      lcdTextBlink(1, 6, 7);
      return;
    case WARNING_NO_WATER:
      fprintf_P(&lcd_out, PSTR("Misting error!  ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("No water! :(    "));
      lcdTextBlink(1, 0, 12);
      return;
  }  
}

void lcdAlert() {
  #ifdef DEBUG_LCD
    printf_P(PSTR("LCD: Info: Show Alert #%d.\n\r"), states[ERROR]);
  #endif
  melody.beep(5);
  last_touch = seconds();
  lcdBacklightBlink(1);

  lcd.home();
  switch (states[ERROR]) {
    case ERROR_LOW_MEMORY:
      fprintf_P(&lcd_out, PSTR("MEMORY ERROR!   ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("Low memory!     "));
      lcdTextBlink(1, 0, 10);
      return;
    case ERROR_EEPROM:
      fprintf_P(&lcd_out, PSTR("EEPROM ERROR!   ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("Settings reset! "));
      lcdTextBlink(1, 0, 14);
      return;
    case ERROR_DHT11:
      fprintf_P(&lcd_out, PSTR("DHT11 ERROR!    ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("Check connection"));
      lcdTextBlink(1, 0, 15);
      return;
    case ERROR_BH1750:
      fprintf_P(&lcd_out, PSTR("BH1750 ERROR!   ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("Check connection"));
      lcdTextBlink(1, 0, 15);
      return;
    case ERROR_DS18B20:
      fprintf_P(&lcd_out, PSTR("DS18B20 ERROR!  ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("Check connection"));
      lcdTextBlink(1, 0, 15);
      return;
    case ERROR_NO_SUBSTRATE:
      fprintf_P(&lcd_out, PSTR("No substrate!   ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("Plants can die!"));
      lcdTextBlink(1, 0, 14);
      return;
  }  
}

void leftButtonClick() {
  buttonsShortClick(-1);
}

void rightButtonClick() {
  buttonsShortClick(+1);
}

void buttonsShortClick(int _direction) {
  #ifdef DEBUG_LCD 
    printf_P(
      PSTR("Button: Info: Short click: Edit mode: %d, Menu: %d, Cursor: %d.\n\r"),
        menuEditMode, menuItem, menuEditCursor);
  #endif
  melody.beep(1);
  last_touch = seconds();
  lcd.setBacklight(true);
  // action for simple Menu
  if(menuEditMode == false) {
    // move forward to next menu
    lcdShowMenu(menuItem + _direction);
    return;
  }
  // mark settings for save
  storage.changed = true;
  // change settings
  switch (menuItem) {
    case WATERING_DAY:
      settings.wateringDayPeriod += _direction;
      return;
    case WATERING_NIGHT:
      settings.wateringNightPeriod += _direction;
      return;
    case WATERING_SUNRISE:
      settings.wateringSunrisePeriod += _direction;
      return;
    case MISTING_DAY:
      settings.mistingDayPeriod += _direction;
      return;
    case MISTING_NIGHT:
      settings.mistingNightPeriod += _direction;
      return;
    case MISTING_SUNRISE:
      settings.mistingSunrisePeriod += _direction;
      return;
    case DAY_TIME:
      if(menuEditCursor == 1)
        settings.daytimeFrom += _direction;
      else
        settings.daytimeTo += _direction;
      return;
    case NIGHT_TIME:
      if(menuEditCursor == 1)
        settings.nighttimeFrom += _direction;
      else
        settings.nighttimeTo += _direction;
      return;
    case LIGHT_THRESHOLD:
      settings.lightThreshold += _direction;
      return;
    case LIGHT_DAY:
      settings.lightDayDuration += _direction;
      return;
    case HUMIDITY_THRESHOLD:
      settings.humidThreshold += _direction;
      return;
    case T_AIR_THRESHOLD:
      settings.tempThreshold += _direction;
      return;
    case T_SUBSTRATE_THRESHOLD:
      settings.tempSubsThreshold += _direction;
      return;
    case TEST:
      menuEditMode = false;
      storage.changed = false;
      doTest(false);
    case CLOCK:
      settingClock(_direction);
      // clock not used storage
      storage.changed = false;
      return;
    default:
      menuItem = HOME;
      menuHomeItem = 0;
  }
}

void settingClock(int _direction) {
  #ifdef DEBUG_LCD
    printf_P(PSTR("Button: Info: Clock settings. Stop clock.\n\r"));
  #endif
  RTC.stopClock();
  switch (menuEditCursor) {
    case 4:
      if(0 < RTC.hour && RTC.hour < 23)
          RTC.hour += _direction;
      return;
    case 3:
      if(0 < RTC.minute && RTC.minute < 59)
          RTC.minute += _direction;
      return;
    case 2:
      if(1 < RTC.day && RTC.day < 31)
          RTC.day += _direction;
      return;
    case 1:
      if(1 < RTC.month && RTC.month < 12)
          RTC.month += _direction;
      return;
    case 0:
      RTC.year += _direction;
      return;
    default:
      menuEditCursor = 0;
  }
}

void buttonsLongPress() {
  #ifdef DEBUG_LCD 
    printf_P(PSTR("Button: Info: Long press: Edit mode: %d, Menu: %d.\n\r"),
      menuEditMode, menuItem);
  #endif
  melody.beep(2);
  // reset text blink state
  lcdBlink = false;
  // action for simple Menu
  if(menuItem != HOME && menuEditMode == false) {
    // enter to Edit Menu and return edit field
    menuEditCursor = lcdEditMenu(menuItem, menuEditCursor);
    return;
  }
  // action for Edit menu
  if(menuEditMode && menuEditCursor > 0) {
    // move to next edit field
    lcdEditMenu(menuItem, menuEditCursor-1);
    return;
  }
  // save changed time
  if(menuEditMode && menuItem == CLOCK) {
    #ifdef DEBUG_LCD
      printf_P(PSTR("Button: Info: Set new time and start clock.\n\r"));
    #endif
    RTC.setTime();
    RTC.startClock();
  }
  // close Edit menu
  lcdShowMenu(menuItem);
}
