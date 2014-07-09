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
#include "DS18B20.h"
#include "BH1750.h"
#include "Melody.h"
#include "MeshNet.h"
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include "RF24Layer2.h"

// debug console
//#define DEBUG
//#define DEBUG_LCD

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
#ifdef DEBUG
  static int serial_putchar(char c, FILE *) {
    Serial.write(c);
    return 0;
  };
  FILE serial_out = {0};
#endif
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

// DEVICE TYPE
const uint32_t deviceType = 001;
// DEVICE UNIQUE ID
uint32_t deviceUniqueId = 100002;
/** LAYER 2 DEPENDENT CODE **/
// Declare SPI bus pins
#define CE_PIN  9
#define CS_PIN  10
// Set up nRF24L01 radio
RF24 radio(CE_PIN, CS_PIN);
const uint8_t RF24_INTERFACE = 0;
// The number of network interfaces that this device has
const int NUM_INTERFACES = 1;
// Pass a layer3 packet to the layer2
int sendPacket(unsigned char* message, uint8_t len, uint8_t interface, uint8_t macAddress)
{  
    // Here should be called the layer2 function corresponding to interface
    if(interface == RF24_INTERFACE){
      rf24sendPacket(message, len, macAddress);
      return 1;
    }
    return 0;
}
void onCommandReceived(uint8_t command, void* data, uint8_t dataLen)
{
  #ifdef DEBUG
    printf_P(PSTR("onCommandReceived: %d, %d\n\r"), command, data);
  #endif
}

// Declare delay managers
timer_t fast_timer(1000);
timer_t slow_timer(60000);

// Declare state map
struct comparator {
  bool operator()(const char* &str1, const char* &str2) {
    return strcmp(str1, str2) == 0;
  }
};
SimpleMap<const char*, int, 12, comparator> states;

// Declare push buttons
OneButton rightButton(A2, true);
OneButton leftButton(A3, true);

// Set up Speaker digital pin
Melody melody(8);

// Declare constants
#define NAME  "hydroponics"
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
#define SUNNY_TRESHOLD  2500 // lux

// Declare warning states
#define NO_WARNING  0
#define INFO_SUBSTRATE_FULL  1
#define WARNING_SUBSTRATE_LOW  2
#define INFO_SUBSTRATE_DELIVERED  3
#define WARNING_WATERING  4
#define WARNING_MISTING  5
#define WARNING_AIR_COLD  6
#define WARNING_AIR_HOT  7
#define WARNING_SUBSTRATE_COLD  8
#define WARNING_NO_WATER  9

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
#define WATERING_DURATION  1
#define WATERING_SUNNY  2
#define WATERING_NIGHT  3
#define WATERING_OTHER  4
#define MISTING_DURATION  5
#define MISTING_SUNNY  6
#define MISTING_NIGHT  7
#define MISTING_OTHER  8
#define LIGHT_MINIMUM  9
#define LIGHT_DAY_DURATION  10
#define LIGHT_DAY_START  11
#define HUMIDITY_MINIMUM  12
#define HUMIDITY_MAXIMUM  13
#define AIR_TEMP_MINIMUM  14
#define AIR_TEMP_MAXIMUM  15
#define SUBSTRATE_TEMP_MINIMUM  16
#define TEST 17
#define CLOCK  18

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
uint16_t sunrise = 0;
uint8_t misting_duration = 0;
uint16_t start_watering = 0;
bool menuEditMode;
uint8_t menuEditCursor;
uint8_t menuItem;
uint8_t menuHomeItem;
bool lcdBlink;
bool lcdBacklight;
bool substrate_full;

/****************************************************************************/

//
// Setup
//
void setup()
{
  // Configure output
  #ifdef DEBUG
    Serial.begin(9600);
    fdev_setup_stream(&serial_out, serial_putchar, NULL, _FDEV_SETUP_WRITE);
    stdout = stderr = &serial_out;
  #endif
  fdev_setup_stream(&lcd_out, lcd_putchar, NULL, _FDEV_SETUP_WRITE);
 
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
  // touch init
  last_touch = seconds();

  // initialize radio
  rf24init();
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
    if(states[ERROR] != NO_ERROR && 
        last_touch+10 < seconds())
      lcdAlert();
    else
    if(states[WARNING] != NO_WARNING && 
        last_touch+10 < seconds())
      lcdWarning();
    else
    if(menuItem != HOME && 
        last_touch+30 < seconds() &&
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
    if(states[WARNING] == NO_WARNING &&
        lcd.isBacklight() && last_touch + 5*60 < seconds()) {
      // switch off backlight
      lcd.setBacklight(false);
    }
    // send data to base
    sendCommand(1, (void*) &states[NAME], sizeof(states[NAME]));
    sendCommand(2, (void*) &states[HUMIDITY], sizeof(states[HUMIDITY]));
    sendCommand(3, (void*) &states[AIR_TEMP], sizeof(states[AIR_TEMP]));
    sendCommand(4, (void*) &states[COMPUTER_TEMP], sizeof(states[COMPUTER_TEMP]));
    sendCommand(5, (void*) &states[SUBSTRATE_TEMP], sizeof(states[SUBSTRATE_TEMP]));
    sendCommand(6, (void*) &states[LIGHT], sizeof(states[LIGHT]));
    sendCommand(7, (void*) &states[PUMP_MISTING], sizeof(states[PUMP_MISTING]));
    sendCommand(8, (void*) &states[PUMP_WATERING], sizeof(states[PUMP_WATERING]));
    sendCommand(9, (void*) &states[LAMP], sizeof(states[LAMP]));
    sendCommand(10, (void*) &states[WARNING], sizeof(states[WARNING]));
    sendCommand(11, (void*) &states[ERROR], sizeof(states[ERROR]));
  }
  // update push buttons
  leftButton.tick();
  rightButton.tick();
  // update melody
  melody.update();
  // update network
  rf24receive();
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
    #ifdef DEBUG_DHT11
      printf_P(PSTR("DHT11: Info: Air humidity: %d, temperature: %dC.\n\r"), 
        states[HUMIDITY], states[AIR_TEMP]);
    #endif
    return true;
  }
  printf_P(PSTR("DHT11: Error: Communication failed!\n\r"));
  return false;
}

bool read_DS18B20() {
  DS18B20 ds(ONE_WIRE_BUS);
  
  int value = ds.read(0);
  if(value == DS_DISCONNECTED) {
    printf_P(PSTR("DS18B20: Error: Computer sensor communication failed!\n\r"));
    return false;
  }
  #ifdef DEBUG_DS18B20
    printf_P(PSTR("DS18B20: Info: Computer temperature: %dC.\n\r"), value);
  #endif
  states[COMPUTER_TEMP] = value;

  value = ds.read(1);
  if(value == DS_DISCONNECTED) {
    printf_P(PSTR("DS18B20: Error: Substrate sensor communication failed!\n\r"));
    return false;
  }
  #ifdef DEBUG_DS18B20
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
  #ifdef DEBUG_BH1750
    printf_P(PSTR("BH1750: Info: Light intensity: %d.\n\r"), value);
  #endif
  states[LIGHT] = value;
  return true;
}

void check_levels() {
  // no pull-up for A6 and A7
  pinMode(SUBSTRATE_LEVELPIN, INPUT);
  if(analogRead(SUBSTRATE_LEVELPIN) > 700) {
    //states[ERROR] = ERROR_NO_SUBSTRATE;
    //return;
  }
  pinMode(SUBSTRATE_DELIVEREDPIN, INPUT_PULLUP);
  if(digitalRead(SUBSTRATE_DELIVEREDPIN) == 1) {
    states[WARNING] = INFO_SUBSTRATE_DELIVERED;
    return;
  }
  // no pull-up for A6 and A7
  pinMode(WATER_LEVELPIN, INPUT);
  if(analogRead(WATER_LEVELPIN) > 700) {
    //states[WARNING] = WARNING_NO_WATER;
    //return;
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
    #ifdef DEBUG_RELAY
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
    #ifdef DEBUG_RELAY
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
  if(states[SUBSTRATE_TEMP] <= settings.subsTempMinimum) {
    states[WARNING] = WARNING_SUBSTRATE_COLD;
    return;
  }
  // check air temperature
  if(states[AIR_TEMP] <= settings.airTempMinimum && 
      RTC.hour >= 7) {
    states[WARNING] = WARNING_AIR_COLD;
    return;
  } else if(states[AIR_TEMP] >= settings.airTempMaximum) {
    states[WARNING] = WARNING_AIR_HOT;
  }
  // reset warning
  states[WARNING] = NO_WARNING;
}

void doTest(bool start) {
  if(start && settings.lightMinimum != 10000) {
    #ifdef DEBUG
      printf_P(PSTR("Test: Info: enable.\n\r"));
    #endif
    // save previous settings
    test = settings;
    // change settings for test
    settings.lightMinimum = 10000; //lux
    settings.lightDayDuration = 18; //hours
    settings.mistingSunnyPeriod = 1; //min
    settings.mistingNightPeriod = 1; //min
    settings.mistingOtherPeriod = 1; //min
    settings.wateringSunnyPeriod = 3; //min
    settings.wateringNightPeriod = 3; //min
    settings.wateringOtherPeriod = 3; //min
    settings.wateringDuration = 2; //min
    // manage test settings
    doWork();
    // long misting after start only
    misting_duration = 10; //sec
    return;
  }
  if(start == false &&
      settings.lightMinimum == 10000) {
    #ifdef DEBUG
      printf_P(PSTR("Test: Info: disable.\n\r"));
    #endif    
    // restore previous settings
    settings = test;
    // turn off immediately
    misting_duration = 0;
    start_watering = 0;
    relayOff(PUMP_WATERING);
    return;
  }
}

void doWork() {
  // don't do any work while error
  if(states[ERROR] != NO_ERROR) {
    return;
  }
  uint8_t one_minute = 60;
  // check humidity
  if(states[HUMIDITY] <= settings.humidMinimum) {
    one_minute /= 2; // do work twice often
  } else if(states[HUMIDITY] >= settings.humidMaximum) {
    one_minute *= 2; // do work twice rarely
  }
  // sunny time
  if(11 <= RTC.hour && RTC.hour < 16 && 
      states[LIGHT] >= SUNNY_TRESHOLD) {
    #ifdef DEBUG
      printf_P(PSTR("Work: Info: Sunny time.\n\r"));
    #endif
    checkWateringPeriod(settings.wateringSunnyPeriod, one_minute);
    checkMistingPeriod(settings.mistingSunnyPeriod, one_minute);
    return;
  }
  // night time 
  if((20 <= RTC.hour || RTC.hour < 8) && 
      states[LIGHT] <= settings.lightMinimum) {
    #ifdef DEBUG
      printf_P(PSTR("Work: Info: Night time.\n\r"));
    #endif
    checkWateringPeriod(settings.wateringNightPeriod, one_minute);
    checkMistingPeriod(settings.mistingNightPeriod, one_minute);
    return;
  }
  // not sunny and not night time
  checkWateringPeriod(settings.wateringOtherPeriod, one_minute);
  checkMistingPeriod(settings.mistingOtherPeriod, one_minute);
}

void checkWateringPeriod(uint8_t _period, uint8_t _time) {
  if(_period != 0 && seconds() > last_watering + (_period * _time))
    start_watering = seconds();
}

void checkMistingPeriod(uint8_t _period, uint8_t _time) {
  if(_period != 0 && seconds() > last_misting + (_period * _time))
    misting_duration = settings.mistingDuration;
}

void doLight() { 
  // try to up temperature
  if(states[AIR_TEMP] <= settings.airTempMinimum &&
      states[LIGHT] > 100) {
    // turn on lamp
    relayOn(LAMP);
    return;
  }
  // light enough
  if(states[LIGHT] > settings.lightMinimum) {
    // turn off lamp
    relayOff(LAMP);
    
    bool morning = 4 < RTC.hour && RTC.hour <= 8;
    // save sunrise time
    if(morning && sunrise == 0) {
      sunrise = states[DTIME];
    } else
    // watch for 30 min to check
    if(morning && sunrise+30 <= states[DTIME]) {
      #ifdef DEBUG
        printf_P(PSTR("Light: Info: Set new Day Start time: %02d:%02d.\n\r"), 
        sunrise/60, sunrise%60);
      #endif
      // save to EEPROM if big difference
      if(sunrise-30 > settings.lightDayStart || 
          sunrise+30 < settings.lightDayStart) {
        storage.changed = true;
      }
      settings.lightDayStart = sunrise;
      // prevent rewrite, move to out of morning
      sunrise += 300;
    }
    return;
  }
  // reset sunrise time
  sunrise = 0;
  // keep light day
  uint16_t lightDayEnd = settings.lightDayStart+(settings.lightDayDuration*60);
  if(settings.lightDayStart <= states[DTIME] && states[DTIME] <= lightDayEnd) {
    #ifdef DEBUG
      printf_P(PSTR("Light: Info: Lamp On till: %02d:%02d.\n\r"), 
        lightDayEnd/60, lightDayEnd%60);
    #endif
    // turn on lamp
    relayOn(LAMP);
    return;
  }
  // turn off lamp
  relayOff(LAMP);
}

void misting() {
  if(misting_duration == 0 || 
      states[WARNING] == WARNING_NO_WATER) {
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
  bool timeIsOver = start_watering + (settings.wateringDuration*60) <= seconds();
  // stop watering
  if(states[WARNING] == INFO_SUBSTRATE_DELIVERED && timeIsOver) {
    #ifdef DEBUG
      printf_P(PSTR("Watering: Info: Stop watering.\n\r"));
    #endif
    relayOff(PUMP_WATERING);
    start_watering = 0;
    if(states[WARNING] == WARNING_WATERING)
      states[WARNING] = NO_WARNING;
    return;
  }
  // emergency stop
  if(timeIsOver || states[ERROR] == ERROR_NO_SUBSTRATE) {
    relayOff(PUMP_WATERING);
    states[WARNING] = WARNING_SUBSTRATE_LOW;
    start_watering = 0;
    #ifdef DEBUG
      printf_P(PSTR("Watering: Error: Emergency stop watering.\n\r"));
    #endif
    return;
  }
  // set pause for cleanup pump and rest
  uint8_t pauseDuration = 5;
  if(states[WARNING] == INFO_SUBSTRATE_DELIVERED)
    pauseDuration = 10;
  // pause every 30 sec
  if((seconds()-start_watering) % 30 <= pauseDuration) {
    #ifdef DEBUG
      printf_P(PSTR("Watering: Info: Pause for clean up.\n\r"));
    #endif
    relayOff(PUMP_WATERING);
    return;
  }
  #ifdef DEBUG
    printf_P(PSTR("Misting: Info: Watering...\n\r"));
  #endif
  states[WARNING] = WARNING_WATERING;
  last_watering = seconds();
  relayOn(PUMP_WATERING);
}

uint16_t seconds() {
  return millis()/1000;
}

void lcdBacklightBlink(uint8_t _count) {
  for(uint8_t i=0; i<_count; i++) {
    lcd.setBacklight(false); delay(250);
    lcd.setBacklight(true); delay(250);
  }
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
    case WATERING_DURATION:
      fprintf_P(&lcd_out, PSTR("Watering durat. ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("for %2d min      "), 
        settings.wateringDuration);
      return;
    case WATERING_SUNNY:
      fprintf_P(&lcd_out, PSTR("Watering sunny  ")); lcd.setCursor(0,1);
      lcdShowMenuPeriodRow(settings.wateringSunnyPeriod);
      return;
    case WATERING_NIGHT:
      fprintf_P(&lcd_out, PSTR("Watering night  ")); lcd.setCursor(0,1);
      lcdShowMenuPeriodRow(settings.wateringNightPeriod);
      return;
    case WATERING_OTHER:
      fprintf_P(&lcd_out, PSTR("Watering evening")); lcd.setCursor(0,1);
      lcdShowMenuPeriodRow(settings.wateringOtherPeriod);
      return;
    case MISTING_DURATION:
      fprintf_P(&lcd_out, PSTR("Misting duration")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("for %2d sec      "), 
        settings.mistingDuration);
      return;
    case MISTING_SUNNY:
      fprintf_P(&lcd_out, PSTR("Misting sunny   ")); lcd.setCursor(0,1);
      lcdShowMenuPeriodRow(settings.mistingSunnyPeriod);
      return;
    case MISTING_NIGHT:
      fprintf_P(&lcd_out, PSTR("Misting night   ")); lcd.setCursor(0,1);
      lcdShowMenuPeriodRow(settings.mistingNightPeriod);
      return;
    case MISTING_OTHER:
      fprintf_P(&lcd_out, PSTR("Misting evening ")); lcd.setCursor(0,1);
      lcdShowMenuPeriodRow(settings.mistingOtherPeriod);
      return;
    case LIGHT_MINIMUM:
      fprintf_P(&lcd_out, PSTR("Light not less  ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("than %4d lux   "), 
        settings.lightMinimum);
      return;
    case LIGHT_DAY_DURATION:
      fprintf_P(&lcd_out, PSTR("Light day       ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("duration %2dh    "), 
        settings.lightDayDuration);
      return;
    case LIGHT_DAY_START:
      fprintf_P(&lcd_out, PSTR("Light day from  ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("%02d:%02d to %02d:%02d  "), 
        settings.lightDayStart/60, settings.lightDayStart%60,
        (settings.lightDayStart/60)+settings.lightDayDuration, 
        settings.lightDayStart%60);
      return;
    case HUMIDITY_MINIMUM:
      fprintf_P(&lcd_out, PSTR("Humidity not    ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("less than %2d%%   "), 
        settings.humidMinimum);
      return;
    case HUMIDITY_MAXIMUM:
      fprintf_P(&lcd_out, PSTR("Humidity not    ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("greater than %2d%%"), 
        settings.humidMaximum);
      return;
    case AIR_TEMP_MINIMUM:
      fprintf_P(&lcd_out, PSTR("Temp. air not   ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("less than %2d%c   "), 
        settings.airTempMinimum, celcium_c);
      return;
    case AIR_TEMP_MAXIMUM:
      fprintf_P(&lcd_out, PSTR("Temp. air not   ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("greater than %2d%c"), 
        settings.airTempMaximum, celcium_c);
      return;
    case SUBSTRATE_TEMP_MINIMUM:
      fprintf_P(&lcd_out, PSTR("Substrate temp. ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("less than %2d%c   "), 
        settings.subsTempMinimum, celcium_c);
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

void lcdShowMenuPeriodRow(uint8_t _period) {
  if(_period != 0) {
    fprintf_P(&lcd_out, PSTR("every %3d min   "), _period);
  } else {
    fprintf_P(&lcd_out, PSTR("is disable      "));
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
      fprintf_P(&lcd_out, PSTR("Light: %c %4dlux"), 
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
    case WATERING_DURATION:
      fprintf_P(&lcd_out, PSTR("Changing watering")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("duration %2d min  "), 
        settings.wateringDuration);
      lcdTextBlink(1, 9, 10);
      return 0;
    case WATERING_SUNNY:
      return lcdEditMenuChangingPeriod(settings.wateringSunnyPeriod);
    case WATERING_NIGHT:
      return lcdEditMenuChangingPeriod(settings.wateringNightPeriod);  
    case WATERING_OTHER:
      return lcdEditMenuChangingPeriod(settings.wateringOtherPeriod);
    case MISTING_DURATION:
      fprintf_P(&lcd_out, PSTR("Changing misting")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("duration %2d sec "), 
        settings.mistingDuration);
      lcdTextBlink(1, 9, 10);
      return 0;
    case MISTING_SUNNY:
      return lcdEditMenuChangingPeriod(settings.mistingSunnyPeriod);
    case MISTING_NIGHT:
      return lcdEditMenuChangingPeriod(settings.mistingNightPeriod);
    case MISTING_OTHER:
      return lcdEditMenuChangingPeriod(settings.mistingOtherPeriod);
    case LIGHT_MINIMUM:
      lcdEditMenuChengingLightRow();
      fprintf_P(&lcd_out, PSTR("minimum %3d lux "), 
        settings.lightMinimum);
      lcdTextBlink(1, 8, 10);
      return 0;
    case LIGHT_DAY_DURATION:
      lcdEditMenuChengingLightRow();
      fprintf_P(&lcd_out, PSTR("day duration %2dh"), 
        settings.lightDayDuration);
      lcdTextBlink(1, 13, 14);
      return 0;
    case LIGHT_DAY_START:
      lcdEditMenuChengingLightRow();
      fprintf_P(&lcd_out, PSTR("day start at %2dh"), 
        settings.lightDayStart/60);
      lcdTextBlink(1, 13, 14);
      return 0;
    case HUMIDITY_MINIMUM:
      fprintf_P(&lcd_out, PSTR("Changing humid. ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("less than %2d%%   "), 
        settings.humidMinimum);
      lcdTextBlink(1, 10, 11);
      return 0;
    case HUMIDITY_MAXIMUM:
      fprintf_P(&lcd_out, PSTR("Changing humid. ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("greater than %2d%%"), 
        settings.humidMaximum);
      lcdTextBlink(1, 13, 14);
      return 0;
    case AIR_TEMP_MINIMUM:
      lcdEditMenuChengingTempRow();
      fprintf_P(&lcd_out, PSTR("less than %2d%c   "), 
        settings.airTempMinimum, celcium_c);
      lcdTextBlink(1, 10, 11);
      return 0;
    case AIR_TEMP_MAXIMUM:
      lcdEditMenuChengingTempRow();
      fprintf_P(&lcd_out, PSTR("greater than %2d%c"), 
        settings.airTempMaximum, celcium_c);
      lcdTextBlink(1, 13, 14);
      return 0;
    case SUBSTRATE_TEMP_MINIMUM:
      lcdEditMenuChengingTempRow();
      fprintf_P(&lcd_out, PSTR("less than %2d%c   "), 
        settings.subsTempMinimum, celcium_c);
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
  if(_period != 0) {
    fprintf_P(&lcd_out, PSTR("every %3d min.    "), _period);
    lcdTextBlink(1, 6, 8);
  } else {
    fprintf_P(&lcd_out, PSTR("is disable      "));
    lcdTextBlink(1, 3, 9);
  }
  return 0;
}

void lcdEditMenuChengingTempRow() {
  fprintf_P(&lcd_out, PSTR("Changing temp.  ")); lcd.setCursor(0,1);
}

void lcdEditMenuChengingLightRow() {
  fprintf_P(&lcd_out, PSTR("Changing light  ")); lcd.setCursor(0,1);
}

void lcdWarning() {
  #ifdef DEBUG_LCD
    printf_P(PSTR("LCD: Info: Show Warning #%d.\n\r"), states[WARNING]);
  #endif
  lcd.setBacklight(true);
  
  lcd.home();
  switch (states[WARNING]) { 
    case WARNING_SUBSTRATE_LOW:
      fprintf_P(&lcd_out, PSTR("Low substrate!  ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("Please add some!"));
      melody.beep(1);
      lcdTextBlink(1, 0, 15);
      return;
    case INFO_SUBSTRATE_FULL:
      fprintf_P(&lcd_out, PSTR("Substrate tank  ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("is full! :)))   "));
      lcdBacklightBlink(1);
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
      return;
    case WARNING_AIR_COLD:
      fprintf_P(&lcd_out, PSTR("Air is too cold ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("for plants! :(  "));
      melody.beep(1);
      lcdTextBlink(1, 12, 13);
      return;
    case WARNING_AIR_HOT:
      fprintf_P(&lcd_out, PSTR("Air is too hot ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("for plants! :(  "));
      melody.beep(1);
      lcdTextBlink(1, 12, 13);
      return;
    case WARNING_SUBSTRATE_COLD:
      fprintf_P(&lcd_out, PSTR("Substrate is too")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("cold! :(        "));
      melody.beep(2);
      lcdTextBlink(1, 6, 7);
      return;
    case WARNING_NO_WATER:
      fprintf_P(&lcd_out, PSTR("Misting error!  ")); lcd.setCursor(0,1);
      fprintf_P(&lcd_out, PSTR("No water! :(    "));
      melody.beep(1);
      lcdTextBlink(1, 0, 12);
      return;
  }  
}

void lcdAlert() {
  #ifdef DEBUG_LCD
    printf_P(PSTR("LCD: Info: Show Alert #%d.\n\r"), states[ERROR]);
  #endif
  melody.beep(5);
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
      fprintf_P(&lcd_out, PSTR("Plants can die! "));
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
  if(lcd.isBacklight() == false) {
    lcd.setBacklight(true);
    return;
  }
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
    case WATERING_DURATION:
      settings.wateringDuration += _direction;
      return;    
    case WATERING_SUNNY:
      settings.wateringSunnyPeriod += _direction;
      return;
    case WATERING_NIGHT:
      settings.wateringNightPeriod += _direction;
      return;
    case WATERING_OTHER:
      settings.wateringOtherPeriod += _direction;
      return;
    case MISTING_DURATION:
      settings.mistingDuration += _direction;
      return;
    case MISTING_SUNNY:
      settings.mistingSunnyPeriod += _direction;
      return;
    case MISTING_NIGHT:
      settings.mistingNightPeriod += _direction;
      return;
    case MISTING_OTHER:
      settings.mistingOtherPeriod += _direction;
      return;
    case LIGHT_MINIMUM:
      settings.lightMinimum += _direction;
      return;
    case LIGHT_DAY_DURATION:
      settings.lightDayDuration += _direction;
      return;
    case LIGHT_DAY_START:
      settings.lightDayStart += _direction;
      return;
    case HUMIDITY_MINIMUM:
      settings.humidMinimum += _direction;
      return;
    case HUMIDITY_MAXIMUM:
      settings.humidMaximum += _direction;
      return;
    case AIR_TEMP_MINIMUM:
      settings.airTempMinimum += _direction;
      return;
    case AIR_TEMP_MAXIMUM:
      settings.airTempMaximum += _direction;
      return;
    case SUBSTRATE_TEMP_MINIMUM:
      settings.subsTempMinimum += _direction;
      return;
    case TEST:
      menuEditMode = false;
      storage.changed = false;
      doTest(false);
      return;
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
      else RTC.hour = 0 + _direction;
      return;
    case 3:
      if(0 < RTC.minute && RTC.minute < 59)
        RTC.minute += _direction;
      else RTC.minute = 0 + _direction;
      return;
    case 2:
      if(1 < RTC.day && RTC.day < 31)
        RTC.day += _direction;
      else RTC.day = 1 + _direction;
      return;
    case 1:
      if(1 < RTC.month && RTC.month < 12)
        RTC.month += _direction;
      else RTC.month = 1 + _direction;
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
