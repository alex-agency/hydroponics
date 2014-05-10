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
OneButton buttonMenu(A2, true);
OneButton buttonNext(A3, true);

// Declare LCD1609
// Set the pins on the I2C chip used for LCD connections: 
//                    addr, en,rw,rs,d4,d5,d6,d7,bl,blpol
LiquidCrystal_I2C lcd(0x20, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

// Declare state map
struct comparator {
  bool operator()(const char* &str1, const char* &str2) {
    return strcmp(str1, str2) == 0;
  }
};
SimpleMap<const char*, int, 12, comparator> states;

// Declare EEPROM values
bool eeprom_ok = true;
int settingsAdress = 0;
// where to store config data
#define memoryBase 32
// ID of the settings block
#define SETTINGS_ID  "001"
// Declare settings structure
struct SettingsStruct {
  char id[4];
  int a;
} settings = { 
  SETTINGS_ID,
  1
};

// Declare delay managers, ms
timer_t check_timer(5000);

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
  buttonMenu.attachClick(buttonMenuClickEvent);
  buttonNext.attachClick(buttonNextClickEvent);
  buttonMenu.attachDoubleClick(buttonMenuDoubleClickEvent);
  buttonNext.attachDoubleClick(buttonNextDoubleClickEvent);
  buttonMenu.attachLongPressStart(buttonMenuLongPressEvent);
  buttonNext.attachLongPressStart(buttonNextLongPressEvent);

  // Configure LCD1609
  // Initialize the lcd for 16 chars 2 lines and turn on backlight
  lcd.begin(16, 2);
  lcdShowHomeScreen();
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

    // check values
    check();
  }

  // update buttons
  buttonMenu.tick();
  buttonNext.tick();

  //if( eeprom_ok )
  //  saveSettings();
}

/****************************************************************************/

bool read_DHT11() {
  dht11 DHT11;
  int state = DHT11.read(DHT11PIN);
  switch (state) {
    case DHTLIB_OK:
      states[HUMIDITY] = DHT11.humidity;
      states[TEMPER_OUT] = DHT11.temperature;

      if(DEBUG) printf("DHT11: Info: Outside sensor values: humidity: %d, temperature: %d.\n\r", states[HUMIDITY], states[TEMPER_OUT]);
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

int get_cdn(int _day, int _month, int _year) {
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

int get_dtime(int _hours, int _minutes) {
  // minutes after 00:00
  return _hours*60+_minutes;
}

/****************************************************************************/

void set_RTC(int _cdn, int _dtime) {
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

  //states[PUMP_3] = relay3.isRelayOn();
  //if(DEBUG) printf("PUMP_3: Info: Relay state: %d.\n\r", states[PUMP_3]);

  states[LAMP] = relay4.isRelayOn();
  if(DEBUG) printf("LAMP: Info: Relay state: %d.\n\r", states[LAMP]);
}

/****************************************************************************/

void check() {


}

/****************************************************************************/

void alarm(int _mode) {
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

void buttonMenuClickEvent() {
  if(DEBUG) printf("Button MENU: Info: Click event.\n\r");

};

/****************************************************************************/

void buttonNextClickEvent() {
  if(DEBUG) printf("Button NEXT: Info: Click event.\n\r");

};

/****************************************************************************/

void buttonMenuDoubleClickEvent() {
  if(DEBUG) printf("Button MENU: Info: DoubleClick event.\n\r");

};

/****************************************************************************/

void buttonNextDoubleClickEvent() {
  if(DEBUG) printf("Button NEXT: Info: DoubleClick event.\n\r");

};

/****************************************************************************/

void buttonMenuLongPressEvent() {
  if(DEBUG) printf("Button MENU: Info: LongPress event.\n\r");

};

/****************************************************************************/

void buttonNextLongPressEvent() {
  if(DEBUG) printf("Button NEXT: Info: LongPress event.\n\r");

};

/****************************************************************************/

void lcdShowHomeScreen() {
  if(DEBUG) printf("LCD1609: Info: Home screen.\n\r");

  lcd.clear(); 
  lcd.print("Hello");
  lcd.setCursor(0,1);
  lcd.print("World!");
};

/****************************************************************************/
