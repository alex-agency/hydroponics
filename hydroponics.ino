// Import libraries
#include <avr/pgmspace.h>
#include <Wire.h>
#include "LiquidCrystal_I2C.h"
#include "MemoryFree.h"
#include "Watchdog.h"
#include "Timer.h"
#include "SimpleMap.h"
#include "Settings.h"
#include "OneButton.h"
#include "DS1307new.h"
#include "dht11.h"
#include "OneWire.h"
#include "DallasTemperature.h"
#include "BH1750.h"

#define DEBUG
//#define DEBUG_LCD

// Declare LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);
// Declare custom LCD characters
byte celcium_char[8] = {24, 24, 3, 4, 4, 4, 3, 0};
const uint8_t celcium_c = 0;
byte heart_char[8] = {0, 10, 21, 17, 10, 4, 0, 0};
const uint8_t heart_c = 1;
byte humidity_char[8] = {4, 10, 10, 17, 17, 17, 14, 0};
const uint8_t humidity_c = 2;
byte temp_char[8] = {4, 10, 10, 14, 31, 31, 14, 0};
const uint8_t temp_c = 3;
byte flower_char[8] = {14, 27, 21, 14, 4, 12, 4, 0};
const uint8_t flower_c = 4;
byte lamp_char[8] = {14, 17, 17, 17, 14, 14, 4, 0};
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
SimpleMap<const char*, int, 14, comparator> states;

// Declare push buttons
OneButton rightButton(A2, true);
OneButton leftButton(A3, true);

// Declare variables
EEPROM storage;
bool storage_ok;
uint16_t last_lightOn = 0;
uint16_t sunrise_dtime;
uint16_t last_misting = 0;
uint16_t last_watering = 0;
uint16_t last_touch = 0;
bool menuEditMode;
uint8_t menuEditCursor;
uint8_t menuItem;
uint8_t menuHomeItem;
bool lcdBlink;
bool lcdBacklight = true;

// Declare constants
#define CDN  "cdn" // days after 2000-01-01
#define DTIME  "dtime" // minutes after 00:00
#define HUMIDITY  "humidity"
#define T_AIR  "t air"
#define T_COMPUTER  "t computer"
#define T_SUBSTRATE  "t substrate"
#define LIGHT  "light"
#define S1_SUBSTRATE  "s1 substrate"
#define S2_WATERING  "s2 watering"
#define S4_MISTING  "s4 misting"
#define P1_MISTING  "p1 misting"
#define P2_WATERING  "p2 watering"
#define LAMP  "lamp"
#define WARNING  "warning"

// Declare warning states
#define NO_WARNING  0
#define WARNING_NO_WATER  1
#define WARNING_SUBSTRATE_FULL  2
#define WARNING_SUBSTRATE_LOW  3
#define WARNING_NO_SUBSTRATE  4
#define WARNING_WATERING_DONE  5
#define WARNING_WATERING  6
#define WARNING_MISTING  7
#define WARNING_AIR_COLD  8
#define WARNING_SUBSTRATE_COLD  9
#define WARNING_EEPROM  10
#define WARNING_DHT11  11
#define WARNING_DS18B20  12
#define WARNING_BH1750  13
#define WARNING_LOW_MEMORY  14

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
#define CLOCK  14

// Declare pins
#define DHT11PIN  3
#define ONE_WIRE_BUS  2
#define S1_SUBSTRATEPIN  A0
#define S2_WATERINGPIN  A1
#define S4_MISTINGPIN  A6
#define P1_MISTINGPIN  4
#define P2_WATERINGPIN  5
#define LAMPPIN  7


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
  lcd.clear();

  // show free memory
  fprintf(&lcd_out, "%d bytes free", freeMemory());
  #ifdef DEBUG
    printf_P(PSTR("Free memory: %d bytes.\n\r"), freeMemory());
  #endif
  delay(1000);
  // restart if memory lower 512 bytes
  softResetMem(512);
  // restart after freezing
  softResetTimeout();

  // Load settings
  storage_ok = storage.load();

  // Configure buttons
  rightButton.attachClick( rightButtonClick );
  rightButton.attachDoubleClick( rightButtonClick );
  rightButton.attachLongPressStart( buttonsLongPress );
  leftButton.attachClick( leftButtonClick );
  leftButton.attachDoubleClick( leftButtonClick );
  leftButton.attachLongPressStart( buttonsLongPress );
  
  // read modules and check errors
  system_check();
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
    read_levels();
    if(menuEditMode == false) {
      // update clock
      read_RTC();
    }
    // manage LCD
    if( states[WARNING] != NO_WARNING )
      lcdWarning();
    else if( menuItem != HOME 
        && last_touch+60 < seconds() )
      lcdShowMenu(HOME);
    else if( menuEditMode )
      lcdEditMenu(menuItem, menuEditCursor);
    else
      lcdShowMenu(menuItem);
  }

  if(slow_timer) {
    // read modules and check errors
    system_check();
    // main program
    doWork();
    // save settings
    if( storage.changed && storage_ok ) {
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
  // not so fast
  delay(50);
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

bool read_DHT11() {
  dht11 DHT11;
  if( DHT11.read(DHT11PIN) == DHTLIB_OK ) {
    states[HUMIDITY] = DHT11.humidity;
    states[T_AIR] = DHT11.temperature;
    #ifdef DEBUG
      printf_P(PSTR("DHT11: Info: Air humidity: %d, temperature: %dC.\n\r"), 
        states[HUMIDITY], states[T_AIR]);
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
  states[T_COMPUTER] = value;
  
  value = dallas.getTempCByIndex(1);
  if(value == DEVICE_DISCONNECTED_C) {
    printf_P(PSTR("DS18B20: Error: Substrate sensor communication failed!\n\r"));
    return false;
  }
  #ifdef DEBUG
    printf_P(PSTR("DS18B20: Info: Substrate temperature: %dC.\n\r"), value);
  #endif
  states[T_SUBSTRATE] = value;
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

void read_levels() {
  pinMode(S1_SUBSTRATEPIN, INPUT_PULLUP);
  if(digitalRead(S1_SUBSTRATEPIN) == 1) {
    states[S1_SUBSTRATE] = true;
  } else {
    states[S1_SUBSTRATE] = false;
  }
  pinMode(S2_WATERINGPIN, INPUT_PULLUP);
  if(digitalRead(S2_WATERINGPIN) == 1) {
    states[S2_WATERING] = true;
  } else {
    states[S2_WATERING] = false;
  }
  // no pull-up for A6 and A7
  pinMode(S4_MISTINGPIN, INPUT);
  if(analogRead(S4_MISTINGPIN) < 150) {
    states[S4_MISTING] = true;
  } else {
    states[S4_MISTING] = false;
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

uint16_t seconds() {
  return millis()/1000;
}

void doLight() {
  // light enough 
  if(states[LIGHT] > settings.lightThreshold) {
    // turn off lamp
    relayOff(LAMP);
    // capture time
    if(last_lightOn == 0) {
      last_lightOn = seconds();
      return;
    }
    // watch 30 minutes
    if(seconds() > last_lightOn + 30*60 && 
        RTC.hour > 2 && RTC.hour <= 8) {
      // sunrise in minutes
      sunrise_dtime = states[DTIME]-30;
      #ifdef DEBUG
        printf_P(PSTR("Light: Info: New sunrise at: %02d:%02d.\n\r"), 
          sunrise_dtime/60, sunrise_dtime%60);
      #endif
      return;
    }
    // default sunrise at 5 o'clock
    sunrise_dtime = 5*60; // minutes
    #ifdef DEBUG
      printf_P(PSTR("Light: Info: Set default sunrise time.\n\r"));
    #endif
    return;
  }
  
  uint16_t lightOff = sunrise_dtime+(settings.lightDayDuration*60);
  if(sunrise_dtime > 0 && states[DTIME] <= lightOff) {
    #ifdef DEBUG
      printf_P(PSTR("Light: Info: Lamp On till: %02d:%02d.\n\r"), 
        lightOff/60, lightOff%60);
    #endif
    // turn on lamp
    relayOn(LAMP);
    return;
  }
  
  last_lightOn = 0;
  // turn off lamp
  relayOff(LAMP);
}

void system_check() {
  if(freeMemory() < 600) {
    printf_P(PSTR("ERROR: Low memory!!!\n\r"));
    states[WARNING] = WARNING_LOW_MEMORY;
  }
  #ifdef DEBUG
    printf_P(PSTR("Free memory: %d bytes.\n\r"), freeMemory());
  #endif

  if(storage_ok == false) {
    states[WARNING] = WARNING_EEPROM;
    return;
  }
  
  read_RTC();
  if(RTC.year < 2014 || RTC.year > 2018) {
    lcdEditMenu(CLOCK, 4);
    return;
  }
  
  if(read_DHT11() == false) {
    states[WARNING] = WARNING_DHT11;
    return;
  }
  
  if(read_DS18B20() == false) {
    //states[WARNING] = WARNING_DS18B20;
    //return;
  }
  
  if(read_BH1750() == false) {
    states[WARNING] = WARNING_BH1750;
    return;
  }

  states[WARNING] = NO_WARNING;
}

void doWork() {
  // day time
  if(settings.daytimeFrom <= RTC.hour && RTC.hour < settings.daytimeTo) {
    if(seconds() > last_watering + (settings.wateringDayPeriod*60)) {
      //doWatering();
    }
    if(seconds() > last_misting + (settings.mistingDayPeriod*60)) {
      //doMisting();
    }
    return;
  }

  // night time 
  if(settings.nighttimeFrom <= RTC.hour || RTC.hour < settings.nighttimeTo) {
    if(seconds() > last_watering + (settings.wateringNightPeriod*60)) {
      //doWatering();
    }
    if(seconds() > last_misting + (settings.mistingNightPeriod*60)) {
      //doMisting();
    }
    doLight();
    return;
  }

  // sunrise or sunset time
  if(seconds() > last_watering + (settings.wateringSunrisePeriod*60)) {
    //doWatering();
  }
  if(seconds() > last_misting + (settings.mistingSunrisePeriod*60)) {
    //doMisting();
  }
  doLight();
}

void lcdBacklightBlink(uint8_t _count) {
  if(lcdBacklight == false) lcdBacklightToggle();
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
  // save state
  menuEditMode = false;
  if(_menuItem == 255) 
    menuItem = CLOCK;  
  else 
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
    case CLOCK:
      fprintf_P(&lcd_out, PSTR("Time:   %02d:%02d:%02d"), 
        RTC.hour, RTC.minute, RTC.second); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("Date: %02d-%02d-%4d"), 
        RTC.day, RTC.month, RTC.year);
      return;
    default:
      menuItem = HOME;
  }
}

void showHomeScreen() {
  #ifdef DEBUG_LCD
    printf_P(PSTR("LCD Panel: Info: Home screen #%d.\n\r"), menuHomeItem);
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
        temp_c, states[T_AIR], celcium_c, humidity_c, states[HUMIDITY]);
      break;
    case 4:
      fprintf_P(&lcd_out, PSTR("Substrate: %c %2d%c"), 
        temp_c, states[T_SUBSTRATE], celcium_c);
      break;
    case 8:
      fprintf_P(&lcd_out, PSTR("Light: %c %3d lux"), 
        lamp_c, states[LIGHT]);
      break;
    case 12:
      fprintf_P(&lcd_out, PSTR("Computer:  %c %2d%c"), 
        temp_c, states[T_COMPUTER], celcium_c);
      break;
  }

  menuHomeItem++;
}

void lcdTextBlink(bool _row, uint8_t _start, uint8_t _end) { 
  if(lcdBlink) {
    #ifdef DEBUG_LCD 
      printf_P(PSTR("LCD Panel: Info: Blink: row #%d from %d to %d.\n\r"),
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
    printf_P(PSTR("LCD Panel: Info: Edit menu #%d with cursor #%d.\n\r"), 
      _menuItem, _editCursor);
  #endif
  // save state
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
    printf_P(PSTR("LCD Panel: Info: Show Warning #%d.\n\r"),
      states[WARNING]);
  #endif
  // backlight blink
  lcdBacklightBlink(1);
  
  lcd.home();
  switch (states[WARNING]) {
    case WARNING_NO_WATER:
      fprintf_P(&lcd_out, PSTR("Can’t misting!  ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("No water! :(    "));
      lcdTextBlink(1, 0, 12);
      return;
    case WARNING_SUBSTRATE_FULL:
      fprintf_P(&lcd_out, PSTR("Substrate tank  ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("is full! :)))   "));
      return;
    case WARNING_SUBSTRATE_LOW:
      fprintf_P(&lcd_out, PSTR("Low substrate!  ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("Please add water"));
      lcdTextBlink(1, 0, 15);
      return;
    case WARNING_NO_SUBSTRATE:
      fprintf_P(&lcd_out, PSTR("Can't watering! ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("Plants can die! "));
      lcdTextBlink(1, 0, 14);
      return;
    case WARNING_WATERING_DONE:
      fprintf_P(&lcd_out, PSTR("Watering done! :)")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("Please wait: %2d m"), 10);
      lcdTextBlink(1, 13, 14);
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
    case WARNING_EEPROM:
      fprintf_P(&lcd_out, PSTR("EEPROM ERROR!   ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("Settings reset! "));
      lcdTextBlink(1, 0, 14);
      return;
    case WARNING_DHT11:
      fprintf_P(&lcd_out, PSTR("DHT11 ERROR!    ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("Check connection"));
      lcdTextBlink(1, 0, 15);
      return;
    case WARNING_DS18B20:
      fprintf_P(&lcd_out, PSTR("DS18B20 ERROR!  ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("Check connection"));
      lcdTextBlink(1, 0, 15);
      return;
    case WARNING_BH1750:
      fprintf_P(&lcd_out, PSTR("BH1750 ERROR!   ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("Check connection"));
      lcdTextBlink(1, 0, 15);
      return;
    case WARNING_LOW_MEMORY:
      fprintf_P(&lcd_out, PSTR("MEMORY ERROR!   ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("Too low memory!"));
      lcdTextBlink(1, 0, 15);
      return;
  }  
}

void leftButtonClick() {
  #ifdef DEBUG_LCD  
    printf_P(PSTR("LCD Panel: Info: Left button click: Menu #%d, Cursor #%d.\n\r"),
      menuItem, menuEditCursor);
  #endif
  last_touch = seconds();
  // enable backlight
  if(lcdBacklight == false) {
    lcdBacklightToggle();
    return; 
  }
  // reset home screen
  if(menuHomeItem > 0) menuHomeItem = 0;
  // action for simple Menu
  if(menuEditMode == false) {
    // move backward to previous menu
    lcdShowMenu(--menuItem);
    return;
  }  
  // mark settings for save
  storage.changed = true;
  // do minus one for particular value
  switch (menuItem) {
    case WATERING_DAY:
      settings.wateringDayPeriod--;
      return;
    case WATERING_NIGHT:
      settings.wateringNightPeriod--;
      return;
    case WATERING_SUNRISE:
      settings.wateringSunrisePeriod--;
      return;
    case MISTING_DAY:
      settings.mistingDayPeriod--;
      return;
    case MISTING_NIGHT:
      settings.mistingNightPeriod--;
      return;
    case MISTING_SUNRISE:
      settings.mistingSunrisePeriod--;
      return;
    case DAY_TIME:
      if(menuEditCursor == 1)
        settings.daytimeFrom--;
      else
        settings.daytimeTo--;
      return;
    case NIGHT_TIME:
      if(menuEditCursor == 1)
        settings.nighttimeFrom--;
      else
        settings.nighttimeTo--;
      return;
    case LIGHT_THRESHOLD:
      settings.lightThreshold--;
      return;
    case LIGHT_DAY:
      settings.lightDayDuration--;
      return;
    case HUMIDITY_THRESHOLD:
      settings.humidThreshold--;
      return;
    case T_AIR_THRESHOLD:
      settings.tempThreshold--;
      return;
    case T_SUBSTRATE_THRESHOLD:
      settings.tempSubsThreshold--;
      return;
    case CLOCK:
      storage.changed = false;
      RTC.stopClock();
      if(menuEditCursor == 4) {
        if(RTC.hour > 0)
          RTC.hour--;
      } else
      if(menuEditCursor == 3) {
        if(RTC.minute > 0)
          RTC.minute--;
      } else
      if(menuEditCursor == 2) {
        if(RTC.day > 1)
          RTC.day--;
      } else
      if(menuEditCursor == 1) {
        if(RTC.month > 1)
          RTC.month--;
      } else {
        RTC.year--;
      }
      return;
    default:
      menuItem = HOME;
  }
}

void rightButtonClick() {
  #ifdef DEBUG_LCD 
    printf_P(PSTR("LCD Panel: Info: Right button click: Menu #%d, Cursor #%d.\n\r"),
      menuItem, menuEditCursor);
  #endif
  last_touch = seconds();
  // enable backlight
  if(lcdBacklight == false) {
    lcdBacklightToggle();
    return; 
  }
  // reset home screen
  if(menuHomeItem > 0) menuHomeItem = 0;
  // action for simple Menu
  if(menuEditMode == false) {
    // move forward to next menu
    lcdShowMenu(++menuItem);
    return;
  }
  // mark settings for save
  storage.changed = true;
  // do plus one for particular value
  switch (menuItem) {
    case WATERING_DAY:
      settings.wateringDayPeriod++;
      return;
    case WATERING_NIGHT:
      settings.wateringNightPeriod++;
      return;
    case WATERING_SUNRISE:
      settings.wateringSunrisePeriod++;
      return;
    case MISTING_DAY:
      settings.mistingDayPeriod++;
      return;
    case MISTING_NIGHT:
      settings.mistingNightPeriod++;
      return;
    case MISTING_SUNRISE:
      settings.mistingSunrisePeriod++;
      return;
    case DAY_TIME:
      if(menuEditCursor == 1)
        settings.daytimeFrom++;
      else
        settings.daytimeTo++;
      return;
    case NIGHT_TIME:
      if(menuEditCursor == 1)
        settings.nighttimeFrom++;
      else
        settings.nighttimeTo++;
      return;
    case LIGHT_THRESHOLD:
      settings.lightThreshold++;
      return;
    case LIGHT_DAY:
      settings.lightDayDuration++;
      return;
    case HUMIDITY_THRESHOLD:
      settings.humidThreshold++;
      return;
    case T_AIR_THRESHOLD:
      settings.tempThreshold++;
      return;
    case T_SUBSTRATE_THRESHOLD:
      settings.tempSubsThreshold++;
      return;
    case CLOCK:
      storage.changed = false;
      RTC.stopClock();
      #ifdef DEBUG_LCD
        printf_P(PSTR("LCD Panel: Info: Stop clock.\n\r")));
      #endif
      if(menuEditCursor == 4) {
        if(RTC.hour < 23)
          RTC.hour++;
      } else
      if(menuEditCursor == 3) {
        if(RTC.minute < 59)
          RTC.minute++;
      } else
      if(menuEditCursor == 2) {
        if(RTC.day < 31)
          RTC.day++;
      } else
      if(menuEditCursor == 1) {
        if(RTC.month < 12)
          RTC.month++;
      } else {
        RTC.year++;
      }
      return;
    default:
      menuItem = HOME;
  }
}

void buttonsLongPress() {
  #ifdef DEBUG_LCD 
    printf_P(PSTR("LCD Panel: Info: Button long press: Menu #%d.\n\r"),
      menuItem);
  #endif
  // action for simple Menu
  if(menuEditMode == false) {
    // enter to Edit Menu and return edit field
    if(menuItem != HOME) 
      menuEditCursor = lcdEditMenu(menuItem, menuEditCursor);
    else
      lcdBacklightToggle();
    return;
  }
  // action for Edit menu
  if(menuEditCursor != 0) {
    // move to next edit field
    lcdEditMenu(menuItem, --menuEditCursor);
    return;
  } 
  // save changed time
  if(menuItem == CLOCK) {
    #ifdef DEBUG_LCD 
      printf_P(PSTR("LCD Panel: Info: Set new time.\n\r"));
    #endif
    RTC.setTime();
    RTC.startClock();
  }
  // close Edit menu
  lcdShowMenu(menuItem);
}
