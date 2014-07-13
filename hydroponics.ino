// Import libraries
#include <Wire.h>
#include <SPI.h>
#include "LiquidCrystal_I2C.h"
#include "MemoryFree.h"
#include "Watchdog.h"
#include "Settings.h"
#include "OneButton.h"
#include "RTClib.h"
#include "DHT.h"
#include "DS18B20.h"
#include "BH1750.h"
#include "Melody.h"
#include "nRF24L01.h"
#include "RF24.h"
#include "RF24Layer2.h"
#include "MeshNet.h"


// Declare LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);
// Declare custom LCD characters
static const uint8_t CHAR_CELCIUM = 0;
uint8_t char_celcium[8] = {24, 24, 3, 4, 4, 4, 3, 0};
static const uint8_t CHAR_HEART = 1;
uint8_t char_heart[8] = {0, 10, 21, 17, 10, 4, 0, 0};
static const uint8_t CHAR_HUMIDITY = 2;
uint8_t char_humidity[8] = {4, 10, 10, 17, 17, 17, 14, 0};
static const uint8_t CHAR_TEMP = 3;
uint8_t char_temp[8] = {4, 10, 10, 14, 31, 31, 14, 0};
static const uint8_t CHAR_FLOWER = 4;
uint8_t char_flower[8] = {14, 27, 21, 14, 4, 12, 4, 0};
static const uint8_t CHAR_LAMP = 5;
uint8_t char_lamp[8] = {14, 17, 17, 17, 14, 14, 4, 0};

// Declare outputs
static int serial_putchar(char c, FILE *) {
  Serial.write(c);
  return 0;
};
FILE serial_out = {0};
static int lcd_putchar(char c, FILE *) {
  if(c == '\n')
    lcd.setCursor(0,1);
  lcd.write(c);
  return 0;
};
FILE lcd_out = {0};

// Declare MESH network
const uint32_t deviceType = 001;
uint32_t deviceUniqueId = 100002;
static const uint8_t CE_PIN = 9;
static const uint8_t CS_PIN = 10;
const uint8_t RF24_INTERFACE = 0;
const int NUM_INTERFACES = 1;
// Declare radio
RF24 radio(CE_PIN, CS_PIN);

// Declare delay managers
uint16_t seconds() {
  return millis()/1000;
}
struct timer_t {
  uint16_t last;
  uint16_t interval;
  timer_t(uint16_t _interval): interval(_interval) {}
  operator bool(void) {
    uint16_t now = seconds();
    bool result = now - last >= interval;
    if ( result )
      last = now;
    return result;
  }
};
timer_t fast_timer(1); // sec
timer_t slow_timer(60); // sec

// Declare push buttons
OneButton rightButton(A2, true);
OneButton leftButton(A3, true);

// Set up Speaker digital pin
Melody melody(8);

// DHT sensor
#define DHTTYPE DHT11

// Declare RTC
RTC_DS1307 rtc;
DateTime clock;

// Declare constants
//static const char* DATA = "hydroponics";
static const uint8_t HUMIDITY = 2; // air humidity
static const uint8_t AIR_TEMP = 3;
static const uint8_t COMPUTER_TEMP = 4; // temperature inside
static const uint8_t SUBSTRATE_TEMP = 5;
static const uint8_t LIGHT = 6; // light intensivity
static const uint8_t PUMP_MISTING = 7;
static const uint8_t PUMP_WATERING = 8;
static const uint8_t LAMP = 9;
static const uint8_t WARNING = 10;
static const uint8_t ERROR = 11;
static const uint16_t SUNNY_TRESHOLD = 2500; // lux
// Declare warning states
static const uint8_t NO_WARNING = 0;
static const uint8_t INFO_SUBSTRATE_FULL = 1;
static const uint8_t WARNING_SUBSTRATE_LOW = 2;
static const uint8_t INFO_SUBSTRATE_DELIVERED = 3;
static const uint8_t WARNING_WATERING = 4;
static const uint8_t WARNING_MISTING = 5;
static const uint8_t WARNING_AIR_COLD = 6;
static const uint8_t WARNING_AIR_HOT = 7;
static const uint8_t WARNING_SUBSTRATE_COLD = 8;
static const uint8_t WARNING_NO_WATER = 9;
// Declare error states
static const uint8_t NO_ERROR = 0;
static const uint8_t ERROR_LOW_MEMORY = 10;
static const uint8_t ERROR_EEPROM = 11;
static const uint8_t ERROR_DHT = 12;
static const uint8_t ERROR_BH1750 = 13;
static const uint8_t ERROR_DS18B20 = 14;
static const uint8_t ERROR_NO_SUBSTRATE = 15;
// Declare LCD menu items
static const uint8_t HOME = 0;
static const uint8_t WATERING_DURATION = 1;
static const uint8_t WATERING_SUNNY = 2;
static const uint8_t WATERING_NIGHT = 3;
static const uint8_t WATERING_OTHER = 4;
static const uint8_t MISTING_DURATION = 5;
static const uint8_t MISTING_SUNNY = 6;
static const uint8_t MISTING_NIGHT = 7;
static const uint8_t MISTING_OTHER = 8;
static const uint8_t LIGHT_MINIMUM = 9;
static const uint8_t LIGHT_DAY_DURATION = 10;
static const uint8_t LIGHT_DAY_START = 11;
static const uint8_t HUMIDITY_MINIMUM = 12;
static const uint8_t HUMIDITY_MAXIMUM = 13;
static const uint8_t AIR_TEMP_MINIMUM = 14;
static const uint8_t AIR_TEMP_MAXIMUM = 15;
static const uint8_t SUBSTRATE_TEMP_MINIMUM = 16;
static const uint8_t TEST = 17;
static const uint8_t CLOCK = 18;
// Declare pins
static const uint8_t DHTPIN = 3;
static const uint8_t ONE_WIRE_BUS = 2;
static const uint8_t SUBSTRATE_FULLPIN = A0;
static const uint8_t SUBSTRATE_DELIVEREDPIN = A1;
static const uint8_t WATER_LEVELPIN = A6;
static const uint8_t SUBSTRATE_LEVELPIN = A7;
static const uint8_t PUMP_WATERINGPIN = 4;
static const uint8_t PUMP_MISTINGPIN = 5;
static const uint8_t LAMPPIN = 7;

// Declare variables
EEPROM storage;
bool storage_ok;
uint16_t last_misting = 0;
uint16_t last_watering = 0;
uint16_t last_touch;
uint16_t sunrise = 0;
uint8_t misting_duration = 0;
uint16_t start_watering = 0;
bool menuEdit;
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
  // Configure outputs
  Serial.begin(9600);
  fdev_setup_stream(&serial_out, serial_putchar, NULL, _FDEV_SETUP_WRITE);
  stdout = stderr = &serial_out;
  fdev_setup_stream(&lcd_out, lcd_putchar, NULL, _FDEV_SETUP_WRITE);
 
  // Configure lcd
  lcd.begin();
  // load custom characters
  lcd.createChar(CHAR_CELCIUM, char_celcium);
  lcd.createChar(CHAR_HEART, char_heart);
  lcd.createChar(CHAR_HUMIDITY, char_humidity);
  lcd.createChar(CHAR_TEMP, char_temp);
  lcd.createChar(CHAR_FLOWER, char_flower);
  lcd.createChar(CHAR_LAMP, char_lamp);

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

  // initialize network
  rf24init();
}

//
// Loop
//
void loop()
{
  // watchdog
  heartbeat();

  if( fast_timer ) {
    // check level sensors
    check_levels();
    // update watering
    watering();
    // update misting
    misting();
    // prevent redundant update
    if(menuEdit == false) {
      // update clock
      clock = rtc.now();
    }
    // manage LCD
    if(rtc.readnvram(ERROR) != NO_ERROR && 
        last_touch+10 < seconds())
      lcdAlert();
    else
    if(rtc.readnvram(WARNING) != NO_WARNING && 
        last_touch+10 < seconds())
      lcdWarning();
    else
    if(menuItem != HOME && 
        last_touch+30 < seconds() &&
        menuItem != TEST)
      lcdMenu(HOME, false);
    else
    if(menuEdit)
      lcdMenu(menuItem, true);
    else
      lcdMenu(menuItem, false);
  }

  if( slow_timer ) {
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
    if(rtc.readnvram(WARNING) == NO_WARNING &&
        lcd.isBacklight() && last_touch + 5*60 < seconds()) {
      // switch off backlight
      lcd.setBacklight(false);
    }
    // send data to base
    //sendCommand( 1, (void*) &NAME, sizeof(NAME) );
    /*sendCommand( HUMIDITY, (void*) &rtc.readnvram(HUMIDITY), 
      sizeof(rtc.readnvram(HUMIDITY)) );
    sendCommand( AIR_TEMP, (void*) &rtc.readnvram(AIR_TEMP), 
      sizeof(rtc.readnvram(AIR_TEMP)) );
    sendCommand( COMPUTER_TEMP, (void*) &rtc.readnvram(COMPUTER_TEMP),
      sizeof(rtc.readnvram(COMPUTER_TEMP)) );
    /*sendCommand(5, (void*) &states[SUBSTRATE_TEMP], sizeof(states[SUBSTRATE_TEMP]));
    sendCommand(6, (void*) &states[LIGHT], sizeof(states[LIGHT]));
    sendCommand(7, (void*) &states[PUMP_MISTING], sizeof(states[PUMP_MISTING]));
    sendCommand(8, (void*) &states[PUMP_WATERING], sizeof(states[PUMP_WATERING]));
    sendCommand(9, (void*) &states[LAMP], sizeof(states[LAMP]));
    sendCommand(10, (void*) &states[WARNING], sizeof(states[WARNING]));
    sendCommand(11, (void*) &states[ERROR], sizeof(states[ERROR]));*/
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

// Pass a layer3 packet to the layer2
int sendPacket(uint8_t* message, uint8_t len, uint8_t interface, uint8_t macAddress) {  
    // Here should be called the layer2 function corresponding to interface
    if(interface == RF24_INTERFACE) {
      rf24sendPacket(message, len, macAddress);
      return 1;
    }
    return 0;
}

void onCommandReceived(uint8_t command, void* data, uint8_t dataLen) {
  #ifdef DEBUG_MESH
    printf_P(PSTR("MESH: INFO: Received %d, %d\n\r"), command, data);
  #endif
}

/****************************************************************************/

bool read_DHT() {
  DHT dht(DHTPIN, DHTTYPE);
  dht.begin();
  rtc.writenvram(HUMIDITY, dht.readHumidity());
  rtc.writenvram(AIR_TEMP, dht.readTemperature());

  if( isnan(rtc.readnvram(HUMIDITY)) || isnan(rtc.readnvram(AIR_TEMP)) ) {
    printf_P(PSTR("DHT11: Error: Communication failed!\n\r"));
    return false;
  }
  #ifdef DEBUG_DHT11
    printf_P(PSTR("DHT11: Info: Air humidity: %d, temperature: %dC.\n\r"), 
      rtc.readnvram(HUMIDITY), rtc.readnvram(AIR_TEMP));
  #endif
  return true;
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
  rtc.writenvram(COMPUTER_TEMP, value);

  value = ds.read(1);
  if(value == DS_DISCONNECTED) {
    printf_P(PSTR("DS18B20: Error: Substrate sensor communication failed!\n\r"));
    return false;
  }
  #ifdef DEBUG_DS18B20
    printf_P(PSTR("DS18B20: Info: Substrate temperature: %dC.\n\r"), value);
  #endif
  rtc.writenvram(SUBSTRATE_TEMP, value);
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
  rtc.writenvram(LIGHT, value);
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
    rtc.writenvram(WARNING, INFO_SUBSTRATE_DELIVERED);
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
      rtc.writenvram(WARNING, INFO_SUBSTRATE_FULL);
      return;
    }
  } else {
  	substrate_full = false;
  }
}

void relayOn(uint8_t relay) {
  if(rtc.readnvram(relay)) {
    // relay is already on
    return;
  }
  bool status = relays(relay, 0); // 0 is ON
  if(status) {
    #ifdef DEBUG_RELAY
      printf_P(PSTR("RELAY: Info: '%s' is enabled.\n\r"), relay);
    #endif
    rtc.writenvram(relay, true);
  }
}

void relayOff(uint8_t relay) {
  if(rtc.readnvram(relay) == false) {
    // relay is already off
    return;
  }
  bool status = relays(relay, 1); // 1 is OFF
  if(status) {
    #ifdef DEBUG_RELAY
      printf_P(PSTR("RELAY: Info: '%s' is disabled.\n\r"), relay);
    #endif
    rtc.writenvram(relay, false);
  }
}

bool relays(uint8_t relay, uint8_t state) {
  if(relay == PUMP_MISTING) {
    pinMode(PUMP_MISTINGPIN, OUTPUT);
    digitalWrite(PUMP_MISTINGPIN, state);
    return true;
  } 
  if(relay == PUMP_WATERING) {
    pinMode(PUMP_WATERINGPIN, OUTPUT);    
    digitalWrite(PUMP_WATERINGPIN, state);
    return true;
  } 
  if(relay == LAMP) {
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
    rtc.writenvram(ERROR, ERROR_LOW_MEMORY);
    return;
  }
  // check EEPROM
  if(storage_ok == false) {
    rtc.writenvram(ERROR, ERROR_EEPROM);
    return;
  }
  // read and check DHT11 sensor
  if(read_DHT() == false) {
    rtc.writenvram(ERROR, ERROR_DHT);
    return;
  }
  // read and check BH1750 sensor
  if(read_BH1750() == false) {
    rtc.writenvram(ERROR, ERROR_BH1750);
    return;
  }
  // read and check DS18B20 sensor
  if(read_DS18B20() == false) {
    rtc.writenvram(ERROR, ERROR_DS18B20);
    return;
  }
  // reset error
  rtc.writenvram(ERROR, NO_ERROR);

  // check substrate temperature
  if(rtc.readnvram(SUBSTRATE_TEMP) <= settings.subsTempMinimum) {
    rtc.writenvram(WARNING, WARNING_SUBSTRATE_COLD);
    return;
  }
  // check air temperature
  if(rtc.readnvram(AIR_TEMP) <= settings.airTempMinimum && 
      clock.hour() >= 7) {
    rtc.writenvram(WARNING, WARNING_AIR_COLD);
    return;
  } else if(rtc.readnvram(AIR_TEMP) >= settings.airTempMaximum) {
    rtc.writenvram(WARNING, WARNING_AIR_HOT);
  }
  // reset warning
  rtc.writenvram(WARNING, NO_WARNING);
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
  if(rtc.readnvram(ERROR) != NO_ERROR) {
    return;
  }
  uint8_t one_minute = 60;
  // check humidity
  if(rtc.readnvram(HUMIDITY) <= settings.humidMinimum) {
    one_minute /= 2; // do work twice often
  } else if(rtc.readnvram(HUMIDITY) >= settings.humidMaximum) {
    one_minute *= 2; // do work twice rarely
  }
  // sunny time
  if(11 <= clock.hour() && clock.hour() < 16 && 
      rtc.readnvram(LIGHT) >= SUNNY_TRESHOLD) {
    #ifdef DEBUG
      printf_P(PSTR("Work: Info: Sunny time.\n\r"));
    #endif
    checkWateringPeriod(settings.wateringSunnyPeriod, one_minute);
    checkMistingPeriod(settings.mistingSunnyPeriod, one_minute);
    return;
  }
  // night time 
  if((20 <= clock.hour() || clock.hour() < 8) && 
      rtc.readnvram(LIGHT) <= settings.lightMinimum) {
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
  if(rtc.readnvram(AIR_TEMP) <= settings.airTempMinimum &&
      rtc.readnvram(LIGHT) > 100) {
    // turn on lamp
    relayOn(LAMP);
    return;
  }
  uint16_t dtime = clock.hour()*60+clock.minute();

  // light enough
  if(rtc.readnvram(LIGHT) > settings.lightMinimum) {
    // turn off lamp
    relayOff(LAMP);
    
    bool morning = 4 < clock.hour() && clock.hour() <= 8;
    // save sunrise time
    if(morning && sunrise == 0) {
      sunrise = dtime;
    } else
    // watch for 30 min to check
    if(morning && sunrise+30 <= dtime) {
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
  if(settings.lightDayStart <= dtime && dtime <= lightDayEnd) {
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
      rtc.readnvram(WARNING) == WARNING_NO_WATER) {
    // stop misting
    if(rtc.readnvram(PUMP_MISTING)) {
      #ifdef DEBUG
        printf_P(PSTR("Misting: Info: Stop misting.\n\r"));
      #endif
      relayOff(PUMP_MISTING);
      if(rtc.readnvram(WARNING) == WARNING_MISTING)
        rtc.writenvram(WARNING, NO_WARNING);
    }
    return;
  }
  #ifdef DEBUG
    printf_P(PSTR("Misting: Info: Misting...\n\r"));
  #endif
  rtc.writenvram(WARNING, WARNING_MISTING);
  misting_duration--;
  last_misting = seconds();
  relayOn(PUMP_MISTING);
}

void watering() {
  if(start_watering == 0 && 
  	  rtc.readnvram(PUMP_WATERING) == false) {
    return;
  }
  bool timeIsOver = start_watering + (settings.wateringDuration*60) <= seconds();
  // stop watering
  if(rtc.readnvram(WARNING) == INFO_SUBSTRATE_DELIVERED && timeIsOver) {
    #ifdef DEBUG
      printf_P(PSTR("Watering: Info: Stop watering.\n\r"));
    #endif
    relayOff(PUMP_WATERING);
    start_watering = 0;
    if(rtc.readnvram(WARNING) == WARNING_WATERING)
      rtc.writenvram(WARNING, NO_WARNING);
    return;
  }
  // emergency stop
  if(timeIsOver || rtc.readnvram(ERROR) == ERROR_NO_SUBSTRATE) {
    relayOff(PUMP_WATERING);
    rtc.writenvram(WARNING, WARNING_SUBSTRATE_LOW);
    start_watering = 0;
    #ifdef DEBUG
      printf_P(PSTR("Watering: Error: Emergency stop watering.\n\r"));
    #endif
    return;
  }
  // set pause for cleanup pump and rest
  uint8_t pauseDuration = 5;
  if(rtc.readnvram(WARNING) == INFO_SUBSTRATE_DELIVERED)
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
  rtc.writenvram(WARNING, WARNING_WATERING);
  last_watering = seconds();
  relayOn(PUMP_WATERING);
}

uint8_t lcdMenu(uint8_t _menuItem, bool _menuEdit) {
  menuItem = _menuItem;
  menuEdit = _menuEdit;

  lcd.home();
  switch (menuItem) {
    case HOME:
      showHomeScreen();
      return 0;
    case WATERING_DURATION:
      fprintf_P(&lcd_out, PSTR("Watering durat. \nfor %2d min      "), 
        settings.wateringDuration);
      if(menuEdit) lcdTextBlink(1, 4, 5);
      return 0;
    case WATERING_SUNNY:
      lcdMenuPeriod(PSTR("Watering sunny  "), settings.wateringSunnyPeriod);
      return 0;
    case WATERING_NIGHT:
      lcdMenuPeriod(PSTR("Watering night  "), settings.wateringNightPeriod);
      return 0;
    case WATERING_OTHER:
      lcdMenuPeriod(PSTR("Watering evening"), settings.wateringOtherPeriod);
      return 0;
    case MISTING_DURATION:
      fprintf_P(&lcd_out, PSTR("Misting duration\nfor %2d sec      "), 
        settings.mistingDuration);
      if(menuEdit) lcdTextBlink(1, 4, 5);
      return 0;
    case MISTING_SUNNY:
      lcdMenuPeriod(PSTR("Misting sunny   "), settings.mistingSunnyPeriod);
      return 0;
    case MISTING_NIGHT:
      lcdMenuPeriod(PSTR("Misting night   "), settings.mistingNightPeriod);
      return 0;
    case MISTING_OTHER:
      lcdMenuPeriod(PSTR("Misting evening "), settings.mistingOtherPeriod);
      return 0;
    case LIGHT_MINIMUM:
      fprintf_P(&lcd_out, PSTR("Light not less  \nthan %4d lux   "), 
        settings.lightMinimum);
      if(menuEdit) lcdTextBlink(1, 5, 8);
      return 0;
    case LIGHT_DAY_DURATION:
      fprintf_P(&lcd_out, PSTR("Light day       \nduration %2dh    "), 
        settings.lightDayDuration);
      if(menuEdit) lcdTextBlink(1, 9, 10);
      return 0;
    case LIGHT_DAY_START:
      fprintf_P(&lcd_out, PSTR("Light day from  \n"));
      if(menuEdit == false) {
        fprintf_P(&lcd_out, PSTR("%02d:%02d to %02d:%02d  "), 
          settings.lightDayStart/60, settings.lightDayStart%60,
          (settings.lightDayStart/60)+settings.lightDayDuration, 
          settings.lightDayStart%60);
      } else {
        fprintf_P(&lcd_out, PSTR("day start at %2dh"), 
          settings.lightDayStart/60);
        lcdTextBlink(1, 13, 14);
      }
      return 0;
    case HUMIDITY_MINIMUM:
      fprintf_P(&lcd_out, PSTR("Humidity not    \nless than %2d%%   "), 
        settings.humidMinimum);
      if(menuEdit) lcdTextBlink(1, 10, 11);
      return 0;
    case HUMIDITY_MAXIMUM:
      fprintf_P(&lcd_out, PSTR("Humidity not    \ngreater than %2d%%"), 
        settings.humidMaximum);
      if(menuEdit) lcdTextBlink(1, 13, 14);
      return 0;
    case AIR_TEMP_MINIMUM:
      fprintf_P(&lcd_out, PSTR("Temp. air not   \nless than %2d%c   "), 
        settings.airTempMinimum, char_celcium);
      if(menuEdit) lcdTextBlink(1, 10, 11);
      return 0;
    case AIR_TEMP_MAXIMUM:
      fprintf_P(&lcd_out, PSTR("Temp. air not   \ngreater than %2d%c"), 
        settings.airTempMaximum, char_celcium);
      if(menuEdit) lcdTextBlink(1, 13, 14);
      return 0;
    case SUBSTRATE_TEMP_MINIMUM:
      fprintf_P(&lcd_out, PSTR("Substrate temp. \nless than %2d%c   "), 
        settings.subsTempMinimum, char_celcium);
      if(menuEdit) lcdTextBlink(1, 10, 11);
      return 0;
    case TEST:
      if(menuEdit == false) {
        fprintf_P(&lcd_out, PSTR("Test all systems\n       -> Start?"));
        doTest(false);
      } else {
        fprintf_P(&lcd_out, PSTR("Testing.....    \n        -> Stop?"));
        doTest(true);
      }
      lcdTextBlink(1, 10, 15);
      return 0;
    case 255: 
      menuItem = CLOCK;
    case CLOCK:
      if(menuEdit == false) {
        fprintf_P(&lcd_out, PSTR("Time:   %02d:%02d:%02d\nDate: %02d-%02d-%4d"), 
          clock.hour(), clock.minute(), clock.second(), 
          clock.day(), clock.month(), clock.year());
      } else {
        fprintf_P(&lcd_out, PSTR("Setting time    \n%02d:%02d %02d-%02d-%4d"), 
          clock.hour(), clock.minute(), clock.day(), clock.month(), clock.year());
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
      }
      return 4;
    default:
      menuItem = HOME;
      menuHomeItem = 0;
      return 0;
  }
}

void lcd_printf(const char * __row1, const char * __row2, va_list   __ap2) {
  fprintf_P(&lcd_out, __row1); lcd.setCursor(0,1);
  fprintf_P(&lcd_out, __row2, __ap2);
}

void showHomeScreen() {
  fprintf_P(&lcd_out, PSTR("%c%c%c%c%c%c%c    %02d:%02d"), 
    char_flower, char_flower, char_flower, char_flower, char_heart, 
    char_flower, char_heart, clock.hour(), clock.minute());
  lcdTextBlink(0, 13, 13);
  lcd.setCursor(0,1);

  if(menuHomeItem >= 16)
    menuHomeItem = 0;
  switch (menuHomeItem) {
    case 0:
      fprintf_P(&lcd_out, PSTR("Air: %c %2d%c %c %2d%%"),
        char_temp, rtc.readnvram(AIR_TEMP), char_celcium, char_humidity, rtc.readnvram(HUMIDITY));
      break;
    case 4:
      fprintf_P(&lcd_out, PSTR("Substrate: %c %2d%c"),
        char_temp, rtc.readnvram(SUBSTRATE_TEMP), char_celcium);
      break;
    case 8:
      fprintf_P(&lcd_out, PSTR("Light: %c %4dlux"),
        char_lamp, rtc.readnvram(LIGHT));
      break;
    case 12:
      fprintf_P(&lcd_out, PSTR("Computer:  %c %2d%c"),
        char_temp, rtc.readnvram(COMPUTER_TEMP), char_celcium);
      break;
  }
  menuHomeItem++;
}

void lcdMenuPeriod(const char * __fmt, uint8_t _period) {
  fprintf_P(&lcd_out, __fmt); lcd.setCursor(0,1);
  if(_period != 0 || menuEdit) {
    fprintf_P(&lcd_out, PSTR("every %3d min   "), _period);
    if(menuEdit) lcdTextBlink(1, 6, 8);
  } else {
    fprintf_P(&lcd_out, PSTR("is disable      "));
  }

}

void lcdTextBlink(bool _row, uint8_t _start, uint8_t _end) { 
  if(lcdBlink) {
    while(_start <= _end) {
      lcd.setCursor(_start, _row); fprintf_P(&lcd_out, PSTR(" "));
      _start++;
    }
    lcdBlink = false;
  } else {
    lcdBlink = true;   
  }
}

void lcdBacklightBlink(uint8_t _count) {
  for(uint8_t i=0; i<_count; i++) {
    lcd.setBacklight(false); delay(250);
    lcd.setBacklight(true); delay(250);
  }
}

void lcdWarning() {
  lcd.setBacklight(true);
  
  lcd.home();
  switch (rtc.readnvram(WARNING)) { 
    case WARNING_SUBSTRATE_LOW:
      fprintf_P(&lcd_out, PSTR("Low substrate!  \nPlease add some!"));
      melody.beep(1);
      lcdTextBlink(1, 0, 15);
      return;
    case INFO_SUBSTRATE_FULL:
      fprintf_P(&lcd_out, PSTR("Substrate tank  \nis full! :)))   "));
      lcdBacklightBlink(1);
      return;
    case INFO_SUBSTRATE_DELIVERED:
      fprintf_P(&lcd_out, PSTR("Substrate was   \ndelivered! :))) "));
      return;
    case WARNING_WATERING:
      fprintf_P(&lcd_out, PSTR("Watering...     \nPlease wait.    "));
      lcdTextBlink(1, 0, 11);
      return;
    case WARNING_MISTING:
      fprintf_P(&lcd_out, PSTR("Misting...      \nPlease wait.    "));
      return;
    case WARNING_AIR_COLD:
      fprintf_P(&lcd_out, PSTR("Air is too cold \nfor plants! :(  "));
      melody.beep(1);
      lcdTextBlink(1, 12, 13);
      return;
    case WARNING_AIR_HOT:
      fprintf_P(&lcd_out, PSTR("Air is too hot \nfor plants! :(  "));
      melody.beep(1);
      lcdTextBlink(1, 12, 13);
      return;
    case WARNING_SUBSTRATE_COLD:
      fprintf_P(&lcd_out, PSTR("Substrate is too\ncold! :(        "));
      melody.beep(2);
      lcdTextBlink(1, 6, 7);
      return;
    case WARNING_NO_WATER:
      fprintf_P(&lcd_out, PSTR("Misting error!  \nNo water! :(    "));
      melody.beep(1);
      lcdTextBlink(1, 0, 12);
      return;
  }  
}

void lcdAlert() {
  melody.beep(5);
  lcdBacklightBlink(1);

  lcd.home();
  switch (rtc.readnvram(ERROR)) {
    case ERROR_LOW_MEMORY:
      fprintf_P(&lcd_out, PSTR("MEMORY ERROR!   \nLow memory!     "));
      lcdTextBlink(1, 0, 10);
      return;
    case ERROR_EEPROM:
      fprintf_P(&lcd_out, PSTR("EEPROM ERROR!   \nSettings reset! "));
      lcdTextBlink(1, 0, 14);
      return;
    case ERROR_DHT:
      fprintf_P(&lcd_out, PSTR("DHT ERROR!      \nCheck connection"));
      lcdTextBlink(1, 0, 15);
      return;
    case ERROR_BH1750:
      fprintf_P(&lcd_out, PSTR("BH1750 ERROR!   \nCheck connection"));
      lcdTextBlink(1, 0, 15);
      return;
    case ERROR_DS18B20:
      fprintf_P(&lcd_out, PSTR("DS18B20 ERROR!  \nCheck connection"));
      lcdTextBlink(1, 0, 15);
      return;
    case ERROR_NO_SUBSTRATE:
      fprintf_P(&lcd_out, PSTR("No substrate!   \nPlants can die! "));
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
  if(menuEdit == false) {
    // move forward to next menu
    lcdMenu(menuItem + _direction, false);
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
      menuEdit = false;
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
  uint8_t year = clock.year();
  uint8_t month = clock.month();
  uint8_t day = clock.day();
  uint8_t hour = clock.hour();
  uint8_t minute = clock.minute();

  switch (menuEditCursor) {
    case 4:
      if(0 < hour && hour < 23)
        hour += _direction;
      else hour = 0 + _direction;
      break;
    case 3:
      if(0 < minute && minute < 59)
        minute += _direction;
      else minute = 0 + _direction;
      break;
    case 2:
      if(1 < day && day < 31)
        day += _direction;
      else day = 1 + _direction;
      break;
    case 1:
      if(1 < month && month < 12)
        month += _direction;
      else month = 1 + _direction;
      break;
    case 0:
      year += _direction;
      break;
    default:
      menuEditCursor = 0;
  }
  clock = DateTime(year, month, day, hour, minute, 0);
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
  if(menuItem != HOME && menuEdit == false) {
    // enter to Edit Menu and return edit field
    menuEditCursor = lcdMenu(menuItem, true);
    return;
  }
  // action for Edit menu
  if(menuEdit && menuEditCursor > 0) {
    // move to next edit field
    menuEditCursor--;
    lcdMenu(menuItem, true);
    return;
  }
  // save changed time
  if(menuEdit && menuItem == CLOCK) {
    rtc.adjust(clock);
  }
  // close Edit menu
  lcdMenu(menuItem, true);
}
