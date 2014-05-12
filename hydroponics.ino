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
static int lcd_putc(char c, FILE *) {
  lcd.write(c);
  return c;
};
static FILE lcdout = {0};
// Declare menu id
int lcdMenuItem = 0;
int lcdMenuEditMode = false;
int lcdEditCursor = 0;

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
  fdev_setup_stream (&lcdout, lcd_putc, NULL, _FDEV_SETUP_WRITE);
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

void buttonLeftClickEvent() {
  if(DEBUG) printf("Button LEFT: Info: Click event.\n\r");
  
  if(lcdMenuEditMode == false) {
    lcdShowMenuScreen(--lcdMenuItem);
    return;  	
  }

  switch (lcdMenuItem) {
    case 1:
	  return;
	case 2:
	  return;
	case 3:
	  return;
	case 4:
	  return;
	case 5:
	  return;
	case 6:
	  return;
	case 7:
	  return;
	case 8:
	  return;
	case 9:
	  return;
	case 10:
	  return;
	case 11:
	  return;
	case 12:
	  return;
	case 13:
	  return;
    default: 
      return;
  }

};

/****************************************************************************/

void buttonRightClickEvent() {
  if(DEBUG) printf("Button RIGHT: Info: Click event.\n\r");

  if(lcdMenuEditMode == false) {
    lcdShowMenuScreen(++lcdMenuItem);
    return;  	
  }

  switch (lcdMenuItem) {
    case 1:
	  return;
	case 2:
	  return;
	case 3:
	  return;
	case 4:
	  return;
	case 5:
	  return;
	case 6:
	  return;
	case 7:
	  return;
	case 8:
	  return;
	case 9:
	  return;
	case 10:
	  return;
	case 11:
	  return;
	case 12:
	  return;
	case 13:
	  return;
    default: 
      return;
  }

};

/****************************************************************************/

void buttonLeftDoubleClickEvent() {
  if(DEBUG) printf("Button LEFT: Info: DoubleClick event.\n\r");
  
  buttonLeftClickEvent();
  buttonLeftClickEvent();
};

/****************************************************************************/

void buttonRightDoubleClickEvent() {
  if(DEBUG) printf("Button RIGHT: Info: DoubleClick event.\n\r");

  buttonRightClickEvent();
  buttonRightClickEvent();
};

/****************************************************************************/

void buttonLeftLongPressEvent() {
  if(DEBUG) printf("Button LEFT: Info: LongPress event.\n\r");

  if(lcdMenuEditMode == false) {
  	lcdMenuEditMode = true;
    lcdEditCursor = lcdShowEditScreen(lcdMenuItem, lcdEditCursor);
    return;
  }

  if(lcdEditCursor != 0) {
  	lcdEditCursor = lcdShowEditScreen(lcdMenuItem, --lcdEditCursor);
  	return;
  }

  lcdMenuEditMode = false;
  lcdShowMenuScreen(lcdMenuItem);	
  return;
};

/****************************************************************************/

void buttonRightLongPressEvent() {
  if(DEBUG) printf("Button RIGHT: Info: LongPress event.\n\r");

  buttonLeftLongPressEvent(); 
};

/****************************************************************************/

void lcdShowHomeScreen() {
  if(DEBUG) printf("LCD1609: Info: Home screen.\n\r");

  lcd.clear();
  fprintf(&lcdout, "Hydroponic %d:%d", RTC.hour, RTC.minute); lcd.setCursor(0,1);
  fprintf(&lcdout, "%dC %d% %dlux", states[TEMPER_OUT], states[HUMIDITY], states[LIGHT]);
  lcdBlink(0, 13, 13);
};

/****************************************************************************/

void lcdShowMenuScreen(int menu) {
  if(DEBUG) printf("LCD1609: Info: Show menu screen #%d.\n\r", menu);
  
  lcd.clear();
  switch (menu) {
    case 1:
      fprintf(&lcdout, "Watering daytime"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", wateringDayPeriod);
	  return;
	case 2:
      fprintf(&lcdout, "Watering night"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", wateringNightPeriod);
	  return;
	case 3:
      fprintf(&lcdout, "Watering sunrise"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", wateringSunrisePeriod);
	  return;
	case 4:
      fprintf(&lcdout, "Misting daytime"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", mistingDayPeriod);
	  return;
	case 5:
      fprintf(&lcdout, "Misting night"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", mistingNightPeriod);
	  return;
	case 6:
      fprintf(&lcdout, "Misting sunrise"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", mistingSunrisePeriod);
	  return;
	case 7:
      fprintf(&lcdout, "Day time"); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %dh to %dh", daytimeFrom, daytimeTo);
	  return;
	case 8:
      fprintf(&lcdout, "Night time"); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %dh to %dh", nighttimeFrom, nighttimeTo);
	  return;
	case 9:
      fprintf(&lcdout, "Light on when"); lcd.setCursor(0,1);
      fprintf(&lcdout, "lower %d lux", lightOnLower);
	  return;
	case 10:
      fprintf(&lcdout, "Light day"); lcd.setCursor(0,1);
      fprintf(&lcdout, "duration %dh", lightDayDuration);
	  return;
	case 11:
      fprintf(&lcdout, "Humidity not"); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %d%", humidityLow);
	  return;
	case 12:
      fprintf(&lcdout, "Temperature not"); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %dC", temperatureLow);
	  return;
	case 13:
  	  fprintf(&lcdout, "Current time"); lcd.setCursor(0,1);
      fprintf(&lcdout, "%d:%d %d-%d-%d", RTC.hour, RTC.minute, 
      	RTC.day, RTC.month, RTC.year);
	  return;	  
    default: 
      lcdShowHomeScreen();
      return;
  }
};

/****************************************************************************/

int lcdShowEditScreen(int menu, cursor) {
  if(DEBUG) printf("LCD1609: Info: Show edit screen #%d.\n\r", menu);
  lcd.clear();
  switch (menu) {
    case 1:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", wateringDayPeriod);
      lcdBlink(1, 6, 8);
	  return 0;
	case 2:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", wateringNightPeriod);
      lcdBlink(1, 6, 8);
	  return 0;
	case 3:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", wateringSunrisePeriod);
      lcdBlink(1, 6, 8);
	  return 0;
	case 4:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", mistingDayPeriod);
      lcdBlink(1, 6, 8);
	  return 0;
	case 5:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", mistingNightPeriod);
      lcdBlink(1, 6, 8);
	  return 0;
	case 6:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", mistingSunrisePeriod);
      lcdBlink(1, 6, 8);
	  return 0;
	case 7:
      fprintf(&lcdout, "Changing range"); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %dh to %dh", daytimeFrom, daytimeTo);
      if(cursor == 1) {
      	lcdBlink(1, 12, 14);
      	return 0;	
      }	
      lcdBlink(1, 5, 7);
	  return 1;
	case 8:
      fprintf(&lcdout, "Changing range"); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %dh to %dh", nighttimeFrom, nighttimeTo);
      if(cursor == 1) {
      	lcdBlink(1, 12, 14);
      	return 0;	
      }	
      lcdBlink(1, 5, 7);
	  return 1;
	case 9:
      fprintf(&lcdout, "Changing light"); lcd.setCursor(0,1);
      fprintf(&lcdout, "lower %d lux", lightOnLower);
      lcdBlink(1, 6, 11);
	  return;
	case 10:
      fprintf(&lcdout, "Changing light"); lcd.setCursor(0,1);
      fprintf(&lcdout, "duration %dh", lightDayDuration);
      lcdBlink(1, 10, 13);
	  return;
	case 11:
      fprintf(&lcdout, "Changing humid."); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %d%", humidityLow);
      lcdBlink(1, 10, 13);
      return;
	case 12:
      fprintf(&lcdout, "Changing temp."); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %dC", temperatureLow);
      lcdBlink(1, 10, 13);
	  return;
	case 13:
  	  fprintf(&lcdout, "Setting time"); lcd.setCursor(0,1);
      fprintf(&lcdout, "%d:%d %d-%d-%d", RTC.hour, RTC.minute, 
      	RTC.day, RTC.month, RTC.year);
	  if(cursor == 4) {
	  	lcdBlink(1, 12, 13);
	  	return 0;
	  } else if(cursor == 3) {
		lcdBlink(1, 9, 10);
		return 1;
	  } else if(cursor == 2) {
	  	lcdBlink(1, 6, 7);
	  	return 2;
	  } else if(cursor == 1) {
	  	lcdBlink(1, 3, 4);
	  	return 3;
	  }
	  lcdBlink(1, 0, 1);
	  return 4;
    default:
      lcdShowHomeScreen();
      return;
  }
};

/****************************************************************************/

void lcdBlink(int row, int start, int end) {
    


  if(DEBUG) printf("LCD1609: Info: Blink for: ?\n\r");
};
