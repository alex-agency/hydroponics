// Import libraries
#include <avr/pgmspace.h>
#include <Wire.h>
#include "Settings.h"
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
//#define DEBUG
//#define DEBUG_LCD
//#define DEBUG_WORK

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

// Declare settings
EEPROM storage;
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
#define WARNING_WATERING_DONE  5
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
SimpleMap<const char*, int, 14, comparator> states;

// Declare push buttons
OneButton rightButton(A2, true);
OneButton leftButton(A3, true);

// Declare LCD1609
LiquidCrystal_I2C lcd(0x27, 16, 2);
// Declare LCD panel variable
uint8_t menuItem;
bool menuEditMode;
uint8_t editCursor;
uint8_t homeItem;
bool blink;
bool backlight = true;
uint32_t lastTouched = 0;
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
// Declare custom characters
const byte celcium_char[8] PROGMEM = {24, 24, 3, 4, 4, 4, 3, 0};
const uint8_t celcium_c PROGMEM = 0;
const byte heart_char[8] PROGMEM = {0, 10, 21, 17, 10, 4, 0, 0};
const uint8_t heart_c PROGMEM = 1;
const byte humidity_char[8] PROGMEM = {4, 10, 10, 17, 17, 17, 14, 0};
const uint8_t humidity_c PROGMEM = 2;
const byte temp_char[8] PROGMEM = {4, 10, 10, 14, 31, 31, 14, 0};
const uint8_t temp_c PROGMEM = 3;
const byte flower_char[8] PROGMEM = {14, 27, 21, 14, 4, 12, 4, 0};
const uint8_t flower_c PROGMEM = 4;
const byte lamp_char[8] PROGMEM = {14, 17, 17, 17, 14, 14, 4, 0};
const uint8_t lamp_c PROGMEM = 5;


// Declare delay managers, ms
timer_t slow_timer(30000);
timer_t fast_timer(1000);

// Declare variables
uint16_t start_misting = 0;
uint16_t last_misting = 0;
uint16_t start_watering = 0;
uint16_t last_watering = 0;
uint16_t last_light = 0;
uint16_t sunrise;


//
// Setup
//
void setup()
{
  // Configure output
  Serial.begin(9600);
  fdev_setup_stream(&serial_out, serial_putchar, NULL, _FDEV_SETUP_WRITE);
  fdev_setup_stream(&lcdout, lcd_putchar, NULL, _FDEV_SETUP_WRITE);
  stdout = &serial_out;
  
  // Load settings
  storage_ok = storage.load();

  // Configure LCD1609
  // Initialize the lcd
  lcd.begin();
  // create custom characters
  lcd.createChar(celcium_c, celcium_char);
  lcd.createChar(heart_c, heart_char);
  lcd.createChar(humidity_c, humidity_char);
  lcd.createChar(temp_c, temp_char);
  lcd.createChar(flower_c, flower_char);
  lcd.createChar(lamp_c, lamp_char);
  // Initialize fprintf
  
  lcd.clear();

  // Configure buttons
  rightButton.attachClick( rightButtonClick );
  rightButton.attachDoubleClick( rightButtonClick );
  rightButton.attachLongPressStart( buttonsLongPress );
  leftButton.attachClick( leftButtonClick );
  leftButton.attachDoubleClick( leftButtonClick );
  leftButton.attachLongPressStart( buttonsLongPress );
}

//
// Loop
//
void loop()
{
  if( slow_timer ) {
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
    if(backlight && 
      lastTouched+300 < seconds()) 
    {
      lcdBacklightToggle();
    }
  }

  if( fast_timer ) {
    if(menuEditMode == false) {
      // update clock
      read_RTC();
    }
    // check level sensors
    read_levels();
    // manage LCD
    if( states[WARNING] != NO_WARNING )
      lcdWarning();
    else if( menuItem != HOME 
        && lastTouched+60 < seconds() )
      lcdShowMenu(HOME);
    else if( menuEditMode )
      lcdEditMenu(menuItem, editCursor);
    else
      lcdShowMenu(menuItem);
  }
  // update push buttons
  leftButton.tick();
  rightButton.tick();
  // not so fast update
  delay(50);
}

/****************************************************************************/

void read_RTC() {
  //RTC.setRAM(0, (uint8_t *)0x0000, sizeof(uint16_t));
  //RTC.getRAM(54, (uint8_t *)0xaa55, sizeof(uint16_t));
  RTC.getTime();
  states[CDN] = RTC.cdn;
  states[DTIME] = RTC.hour*60+RTC.minute;

  #ifdef DEBUG
    printf_P(
      PSTR("RTC: Info: CDN: %d %02d-%02d-%4d, DTIME: %d %02d:%02d:%02d.\n\r"), 
        states[CDN], RTC.day, RTC.month, RTC.year, 
        states[DTIME], RTC.hour, RTC.minute, RTC.second);
  #endif
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
      #ifdef DEBUG
        printf_P(PSTR("DHT11: Info: Air humidity: %d, temperature: %dC.\n\r"), 
      	  states[HUMIDITY], states[T_OUTSIDE]));
      #endif
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
  #ifdef DEBUG
    printf_P(PSTR("DS18B20: Info: Computer temperature: %dC.\n\r"), value));
  #endif
  states[T_INSIDE] = value;
  
  value = dallas.getTempCByIndex(1);
  if(value == DEVICE_DISCONNECTED_C) {
    printf_P(PSTR("DS18B20: Error: Substrate sensor communication failed!\n\r"));
    return false;
  }
  #ifdef DEBUG
    printf_P(PSTR("DS18B20: Info: Substrate temperature: %dC.\n\r"), value));
  #endif
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
  #ifdef DEBUG
    printf_P(PSTR("BH1750: Info: Light intensity: %d.\n\r"), value));
  #endif
  states[LIGHT] = value;
  return true;
}

/****************************************************************************/

void read_levels() {
  pinMode(S1_SUBSTRATEPIN, INPUT_PULLUP);
  if(digitalRead(S1_SUBSTRATEPIN) == 1) {
    #ifdef DEBUG
      printf_P(PSTR("LEVELS: Info: Substrate tank is full.\n\r"));
    #endif
    states[S1_SUBSTRATE] = true;
  } else {
    #ifdef DEBUG
      printf_P(PSTR("LEVELS: Info: Substrate tank is not full!\n\r"));
    #endif
    states[S1_SUBSTRATE] = false;
  }
  
  pinMode(S2_WATERINGPIN, INPUT_PULLUP);
  if(digitalRead(S2_WATERINGPIN) == 1) {
    #ifdef DEBUG
      printf_P(PSTR("LEVELS: Info: Watering has done.\n\r"));
    #endif
    states[S2_WATERING] = true;
  } else {
    #ifdef DEBUG
      printf_P(PSTR("LEVELS: Info: No watering.\n\r"));
    #endif
    states[S2_WATERING] = false;
  }
  
  // no pull-up for A6 and A7
  pinMode(S4_MISTINGPIN, INPUT);
  if(analogRead(S4_MISTINGPIN) < 150) {
    #ifdef DEBUG
      printf_P(PSTR("LEVELS: Info: Misting water sensor active.\n\r"));
    #endif
    states[S4_MISTING] = true;
  } else {
    #ifdef DEBUG
      printf_P(PSTR("LEVELS: Info: No water for misting!\n\r"));
    #endif
    states[S4_MISTING] = false;
  }
}

/****************************************************************************/

void relayOn(const char* relay) {
  if(states[relay]) {
    // relay is already on
    return;
  }
  bool status = relays(relay, 0); // 0 is ON
  if(status) {
    DEBUG(printf_P(PSTR("RELAY: Info: '%s' is enabled.\n\r"), relay));
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
    DEBUG(printf_P(PSTR("RELAY: Info: '%s' is disabled.\n\r"), relay));
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

uint16_t seconds() {
  return millis()/1000;
}

/****************************************************************************/

/*void doMisting() {
  uint16_t duration = 3000;
  
  if(start_misting == 0) {
  	relayOn(P1_MISTING);
  	start_misting = seconds();
  	states[WARNING] = WARNING_MISTING;
  	if(DEBUG) printf_P(PSTR("MISTING: Info: Start.\n\r"));
  	return;
  } 

  if(start_misting > 0 && (seconds() > start_misting + duration)) {
  	relayOff(P1_MISTING);
  	start_misting = -1;
  	states[WARNING] = NO_WARNING;
  	if(DEBUG) printf_P(PSTR("MISTING: Info: Stop.\n\r"));
  	return;
  }
}*/

/****************************************************************************/

/*void doWatering() {
  uint16_t duration = 60000;
  
  if(start_watering == 0) {
  	relayOn(P2_WATERING);
  	start_watering = seconds();
  	states[WARNING] = WARNING_WATERING;
  	if(DEBUG) printf_P(PSTR("WATERING: Info: Start.\n\r"));
  	return;
  }

  if(start_watering > 0 && (seconds() > start_watering + duration)) {
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

}*/

/****************************************************************************/

void doLight() {
  DEBUG_WORK(printf_P(PSTR("Light: Info: Light intensity: %d lux.\n\r"),
      states[LIGHT]));

  // light enough 
  if(states[LIGHT] > settings.lightThreshold) {
    // turn off lamp
    relayOff(LAMP);
    // capture time
    if(last_light == 0) {
      last_light = seconds();
      return;
    }
    // watch 30 minutes
    if(seconds() > last_light + 1800 && 
        RTC.hour > 2 && RTC.hour <= 8) 
    {
      sunrise = states[DTIME]-30;
      DEBUG_WORK(printf_P(PSTR("Light: Info: New sunrise hour is: %d\n\r"), 
        sunrise/60));
      return;
    }
    // default sunrise at 5 o'clock
    sunrise = 300; // minutes
    DEBUG_WORK(printf_P(PSTR("Light: Info: Set default sunrise time.\n\r")));
    return;
  }
  
  uint16_t light_off_threshold = sunrise + (settings.lightDayDuration*60);
  if(sunrise > 0 && (uint8_t)states[DTIME] <= light_off_threshold) {
    DEBUG_WORK(printf_P(PSTR("Light: Info: Lamp on till %d hour.\n\r"), 
        light_off_threshold/60));
    // turn on lamp
    relayOn(LAMP);
    return;
  }
  
  last_light = 0;
  // turn off lamp
  relayOff(LAMP);
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
    lcdEditMenu(CLOCK, 4);
    return;
  }
  
  if(read_DHT11() == false) {
    printf_P(PSTR("DHT11: Error!\n\r"));
    states[WARNING] = WARNING_DHT11;
    return;
  }
  
  if(read_DS18B20() == false) {
    printf_P(PSTR("DS18B20: Error!\n\r"));
    //states[WARNING] = WARNING_DS18B20;
    //return;
  }
  
  if(read_BH1750() == false) {
    printf_P(PSTR("BH1750: Error!\n\r"));
    states[WARNING] = WARNING_BH1750;
    return;
  }
}

/****************************************************************************/

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

/****************************************************************************/

void lcdShowMenu(uint8_t _menuItem) {
  DEBUG_LCD(printf_P(PSTR("LCD Panel: Info: Show menu #%d.\n\r"), _menuItem));
  // save state
  menuEditMode = false;
  if(_menuItem == 255) menuItem = CLOCK;  
  else menuItem = _menuItem;

  lcd.home();
  switch (menuItem) {
    case HOME:
      showHomeScreen();
      return;
    case WATERING_DAY:
      fprintf(&lcdout, "Watering daytime"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %3d min   ", settings.wateringDayPeriod);
      return;
    case WATERING_NIGHT:
      fprintf(&lcdout, "Watering night  "); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %3d min   ", settings.wateringNightPeriod);
      return;
    case WATERING_SUNRISE:
      fprintf(&lcdout, "Watering sunrise"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %3d min   ", settings.wateringSunrisePeriod);
      return;
    case MISTING_DAY:
      fprintf(&lcdout, "Misting daytime "); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %3d min   ", settings.mistingDayPeriod);
      return;
    case MISTING_NIGHT:
      fprintf(&lcdout, "Misting night   "); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %3d min   ", settings.mistingNightPeriod);
      return;
    case MISTING_SUNRISE:
      fprintf(&lcdout, "Misting sunrise "); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %3d min   ", settings.mistingSunrisePeriod);
      return;
    case DAY_TIME:
      fprintf(&lcdout, "Day time        "); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %2dh to %2dh ", 
        settings.daytimeFrom, settings.daytimeTo);
      return;
    case NIGHT_TIME:
      fprintf(&lcdout, "Night time      "); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %2dh to %2dh ", 
        settings.nighttimeFrom, settings.nighttimeTo);
      return;
    case LIGHT_THRESHOLD:
      fprintf(&lcdout, "Light on when   "); lcd.setCursor(0,1);
      fprintf(&lcdout, "lower %3d lux   ", settings.lightThreshold);
      return;
    case LIGHT_DAY:
      fprintf(&lcdout, "Light day       "); lcd.setCursor(0,1);
      fprintf(&lcdout, "duration %2dh    ", settings.lightDayDuration);
      return;
    case HUMIDITY_THRESHOLD:
      fprintf(&lcdout, "Humidity not    "); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %2d%%   ", settings.humidThreshold);
      return;
    case T_OUTSIDE_THRESHOLD:
      fprintf(&lcdout, "Temp. air not   "); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %2d%c   ", 
        settings.tempThreshold, celcium_c);
      return;
    case T_SUBSTRATE_THRESHOLD:
      fprintf(&lcdout, "Substrate temp. "); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %2d%c   ", 
        settings.tempSubsThreshold, celcium_c);
      return;
    case CLOCK:
      fprintf(&lcdout, "Time:   %02d:%02d:%02d", 
        RTC.hour, RTC.minute, RTC.second); lcd.setCursor(0,1);
      fprintf(&lcdout, "Date: %02d-%02d-%4d", RTC.day, RTC.month, RTC.year);
      return;
    default:
      menuItem = HOME;
  }
}

void showHomeScreen() {
  DEBUG_LCD(printf_P(PSTR("LCD Panel: Info: Home screen #%d.\n\r"), homeItem));

  fprintf(&lcdout, "%c%c%c%c%c%c%c    %02d:%02d", flower_c, flower_c, 
    flower_c, flower_c, heart_c, flower_c, heart_c, RTC.hour, RTC.minute);
  lcdTextBlink(0, 13, 13);
  lcd.setCursor(0,1);

  if(homeItem >= 16)
    homeItem = 0;
  
  switch (homeItem) {
    case 0:
      fprintf(&lcdout, "Air: %c %2d%c %c %2d%%", temp_c, states[T_OUTSIDE],
        celcium_c, humidity_c, states[HUMIDITY]);
      break;
    case 4:
      fprintf(&lcdout, "Substrate: %c %2d%c", temp_c,
        states[T_SUBSTRATE], celcium_c);
      break;
    case 8:
      fprintf(&lcdout, "Light: %c %3d lux", lamp_c, states[LIGHT]);
      break;
    case 12:
      fprintf(&lcdout, "Computer:  %c %2d%c", temp_c, 
        states[T_INSIDE], celcium_c);
      break;
  }

  homeItem++;
}

/****************************************************************************/

uint8_t lcdEditMenu(uint8_t _menuItem, uint8_t _editCursor) {
  DEBUG_LCD(
    printf_P(PSTR("LCD Panel: Info: Edit menu #%d with cursor #%d.\n\r"), 
      _menuItem, _editCursor));
  // save state
  menuEditMode = true;
  menuItem = _menuItem;
  editCursor = _editCursor;

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
      fprintf(&lcdout, "Changing light  "); lcd.setCursor(0,1);
      fprintf(&lcdout, "activity %3d lux", settings.lightThreshold);
      lcdTextBlink(1, 9, 11);
      return 0;
    case LIGHT_DAY:
      fprintf(&lcdout, "Changing light  "); lcd.setCursor(0,1);
      fprintf(&lcdout, "duration %2dh    ", settings.lightDayDuration);
      lcdTextBlink(1, 9, 10);
      return 0;
    case HUMIDITY_THRESHOLD:
      fprintf(&lcdout, "Changing humid. "); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %2d%%   ", settings.humidThreshold);
      lcdTextBlink(1, 10, 11);
      return 0;
    case T_OUTSIDE_THRESHOLD:
      fprintf(&lcdout, "Changing temp.  "); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %2d%c   ", 
        settings.tempThreshold, celcium_c);
      lcdTextBlink(1, 10, 11);
      return 0;
    case T_SUBSTRATE_THRESHOLD:
      fprintf(&lcdout, "Changing temp.  "); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %2d%c   ", 
        settings.tempSubsThreshold, celcium_c);
      lcdTextBlink(1, 10, 11);
      return 0; 
    case CLOCK:
      fprintf(&lcdout, "Setting time    "); lcd.setCursor(0,1);
      fprintf(&lcdout, "%02d:%02d %02d-%02d-%4d", RTC.hour, RTC.minute, 
        RTC.day, RTC.month, RTC.year);
      if(editCursor == 4) {
        lcdTextBlink(1, 0, 1);
      } else 
      if(editCursor == 3) {
        lcdTextBlink(1, 3, 4);
      } else 
      if(editCursor == 2) {
        lcdTextBlink(1, 6, 7);
      } else 
      if(editCursor == 1) {
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
  fprintf(&lcdout, "Changing period "); lcd.setCursor(0,1);
  fprintf(&lcdout, "every %3d min.    ", _period);
  lcdTextBlink(1, 6, 8);
  return 0;
}

bool lcdEditMenuChangingRange(uint8_t _from, uint8_t _to) {
  fprintf(&lcdout, "Changing range  "); lcd.setCursor(0,1);
  fprintf(&lcdout, "from %2dh to %2dh ", _from, _to);
  if(editCursor == 1) {
    lcdTextBlink(1, 5, 6);
  } else {
    lcdTextBlink(1, 12, 13);   
  }
  return 1;
}

/****************************************************************************/

void lcdWarning() {
  DEBUG_LCD(printf_P(PSTR("LCD Panel: Info: Show Warning #%d.\n\r"),
    states[WARNING]));
  // backlight blink
  lcdBacklightBlink(1);
  
  lcd.home();
  switch (states[WARNING]) {
    case WARNING_NO_WATER:
      fprintf(&lcdout, "Can’t misting!  "); lcd.setCursor(0,1);
      fprintf(&lcdout, "No water! :(    ");
      lcdTextBlink(1, 0, 12);
      return;
    case WARNING_SUBSTRATE_FULL:
      fprintf(&lcdout, "Substrate tank  "); lcd.setCursor(0,1);
      fprintf(&lcdout, "is full! :)))   ");
      return;
    case WARNING_SUBSTRATE_LOW:
      fprintf(&lcdout, "Low substrate!  "); lcd.setCursor(0,1);
      fprintf(&lcdout, "Please add water");
      lcdTextBlink(1, 0, 15);
      return;
    case WARNING_NO_SUBSTRATE:
      fprintf(&lcdout, "Can't watering! "); lcd.setCursor(0,1);
      fprintf(&lcdout, "Plants can die! ");
      lcdTextBlink(1, 0, 14);
      return;
    case WARNING_WATERING_DONE:
      fprintf(&lcdout, "Watering done! :)"); lcd.setCursor(0,1);
      fprintf(&lcdout, "Please wait: %2d m", 10);
      lcdTextBlink(1, 13, 14);
      return;
    case WARNING_WATERING:
      fprintf(&lcdout, "Watering...     "); lcd.setCursor(0,1);
      fprintf(&lcdout, "Please wait.    ");
      lcdTextBlink(1, 0, 11);
      return;
    case WARNING_MISTING:
      fprintf(&lcdout, "Misting...      "); lcd.setCursor(0,1);
      fprintf(&lcdout, "Please wait.    ");
      lcdTextBlink(1, 0, 11);
      return;
    case WARNING_TEMPERATURE_COLD:
      fprintf(&lcdout, "Air is too cold "); lcd.setCursor(0,1);
      fprintf(&lcdout, "for plants! :(  ");
      lcdTextBlink(1, 12, 13);
      return;
    case WARNING_SUBSTRATE_COLD:
      fprintf(&lcdout, "Substrate is too"); lcd.setCursor(0,1);
      fprintf(&lcdout, "cold! :(        ");
      lcdTextBlink(1, 6, 7);
      return;
    case WARNING_EEPROM:
      fprintf(&lcdout, "EEPROM ERROR!   "); lcd.setCursor(0,1);
      fprintf(&lcdout, "Settings reset! ");
      lcdTextBlink(1, 0, 14);
      return;
    case WARNING_DHT11:
      fprintf(&lcdout, "DHT11 ERROR!    "); lcd.setCursor(0,1);
      fprintf(&lcdout, "Check connection");
      lcdTextBlink(1, 0, 15);
      return;
    case WARNING_DS18B20:
      fprintf(&lcdout, "DS18B20 ERROR!  "); lcd.setCursor(0,1);
      fprintf(&lcdout, "Check connection");
      lcdTextBlink(1, 0, 15);
      return;
    case WARNING_BH1750:
      fprintf(&lcdout, "BH1750 ERROR!   "); lcd.setCursor(0,1);
      fprintf(&lcdout, "Check connection");
      lcdTextBlink(1, 0, 15);
      return;
  }  
}

/****************************************************************************/

void lcdBacklightBlink(uint8_t _count) {
  if(backlight == false) lcdBacklightToggle();
  for(uint8_t i=0; i<_count; i++) {
    lcdBacklightToggle(); delay(250);
    lcdBacklightToggle(); delay(250);
  }
}

/****************************************************************************/

void lcdBacklightToggle() {
  lcd.setBacklight(!backlight);
  backlight = !backlight;
} 

/****************************************************************************/

void lcdTextBlink(bool _row, uint8_t _start, uint8_t _end) { 
  if(blink) {
    DEBUG_LCD( 
      printf_P(PSTR("LCD Panel: Info: Blink: row #%d from %d to %d.\n\r"),
        _row, _start, _end)); 
    
    while(_start <= _end) {
      lcd.setCursor(_start, _row); lcd.print(" ");
      _start++;
    }
    blink = false;
  } else {
    blink = true;  	
  }
}

/****************************************************************************/

void leftButtonClick() {
  DEBUG_LCD(   
    printf_P(PSTR("LCD Panel: Info: Left button click: Menu #%d, Cursor #%d.\n\r"),
      menuItem, editCursor));
  lastTouched = seconds();
  // enable backlight
  if(backlight == false) {
    lcdBacklightToggle();
    return; 
  }
  // reset home screen
  if(homeItem > 0) homeItem = 0;
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
      if(editCursor == 1)
        settings.daytimeFrom--;
      else
        settings.daytimeTo--;
      return;
    case NIGHT_TIME:
      if(editCursor == 1)
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
    case T_OUTSIDE_THRESHOLD:
      settings.tempThreshold--;
      return;
    case T_SUBSTRATE_THRESHOLD:
      settings.tempSubsThreshold--;
      return;
    case CLOCK:
      storage.changed = false;
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
      return;
    default:
      menuItem = HOME;
  }
}

/****************************************************************************/

void rightButtonClick() {
  DEBUG_LCD(
    printf_P(PSTR("LCD Panel: Info: Right button click: Menu #%d, Cursor #%d.\n\r"),
      menuItem, editCursor));
  lastTouched = seconds();
  // enable backlight
  if(backlight == false) {
    lcdBacklightToggle();
    return; 
  }
  // reset home screen
  if(homeItem > 0) homeItem = 0;
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
      if(editCursor == 1)
        settings.daytimeFrom++;
      else
        settings.daytimeTo++;
      return;
    case NIGHT_TIME:
      if(editCursor == 1)
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
    case T_OUTSIDE_THRESHOLD:
      settings.tempThreshold++;
      return;
    case T_SUBSTRATE_THRESHOLD:
      settings.tempSubsThreshold++;
      return;
    case CLOCK:
      storage.changed = false;
      RTC.stopClock();
      DEBUG_LCD(printf_P(PSTR("LCD Panel: Info: Stop clock.\n\r")));
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
      return;
    default:
      menuItem = HOME;
  }
}

/****************************************************************************/

void buttonsLongPress() {
  DEBUG_LCD( 
   	printf_P(PSTR("LCD Panel: Info: Button long press: Menu #%d.\n\r"),
      menuItem));
  // action for simple Menu
  if(menuEditMode == false) {
    // enter to Edit Menu and return edit field
    if(menuItem != HOME) 
      editCursor = lcdEditMenu(menuItem, editCursor);
    else
      lcdBacklightToggle();
    return;
  }
  // action for Edit menu
  if(editCursor != 0) {
    // move to next edit field
    lcdEditMenu(menuItem, --editCursor);
    return;
  } 
  // save changed time
  if(menuItem == CLOCK) {
    DEBUG_LCD(printf_P(PSTR("LCD Panel: Info: Set new time.\n\r")));
    RTC.setTime();
    RTC.startClock();
  }
  // close Edit menu
  lcdShowMenu(menuItem);
}

/****************************************************************************/
