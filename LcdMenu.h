#ifndef LCDMENU_H
#define LCDMENU_H

#include "LiquidCrystal_I2C.h"
#include "SimpleMap.h"
#include "Settings.h"
#include "RTClib.h"
#include "Beep.h"

// Declare LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);
// Declare lcd output
FILE lcd_out = {0};
static bool charErase, textErase, textBlink;
static int lcd_putchar(char c, FILE *) {
  switch(c) {
    case '\n':
      lcd.setCursor(0,1);
      break;
    case '{':
      if(textBlink && textErase)
        charErase = true;
      break;
    case '}':
      charErase = false;
      textErase = !textErase;
      break;
    default:
      if(charErase)
        c = ' ';
      lcd.write(c);
  }
  return 0;
};

// Declare Speaker digital pin
Beep beep(8);

// Declare settings
EEPROM storage;

// Declare RTC
RTC_DS1307 rtc;
DateTime clock;

// Declare state map
SimpleMap<uint8_t, uint16_t, 10> states;

// Define custom LCD characters
static const uint8_t C_CELCIUM = 0;
static const uint8_t C_HEART = 1;
static const uint8_t C_HUMIDITY = 2;
static const uint8_t C_TEMP = 3;
static const uint8_t C_FLOWER = 4;
static const uint8_t C_LAMP = 5;
// Define LCD menu items
static const uint8_t HOME = 0;
static const uint8_t WATERING_DURATION = 1;
static const uint8_t WATERING_SUNNY = 2;
static const uint8_t WATERING = 3;
static const uint8_t MISTING_DURATION = 4;
static const uint8_t MISTING_SUNNY = 5;
static const uint8_t MISTING = 6;
static const uint8_t LIGHT_MINIMUM = 7;
static const uint8_t LIGHT_DAY_DURATION = 8;
static const uint8_t LIGHT_DAY_START = 9;
static const uint8_t HUMIDITY_MINIMUM = 10;
static const uint8_t HUMIDITY_MAXIMUM = 11;
static const uint8_t AIR_TEMP_MINIMUM = 12;
static const uint8_t AIR_TEMP_MAXIMUM = 13;
static const uint8_t SUBSTRATE_TEMP_MINIMUM = 14;
static const uint8_t SILENT_NIGHT = 15;
static const uint8_t TEST = 16;
static const uint8_t CLOCK = 17;
// Define clock settings items
static const uint8_t YEAR_SETTING = 3;
static const uint8_t MONTH_SETTING = 4;
static const uint8_t DAY_SETTING = 5;
static const uint8_t MINUTE_SETTING = 6;
static const uint8_t HOUR_SETTING = 7;
// Define warning states
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
// Define error states
static const uint8_t NO_ERROR = 0;
static const uint8_t ERROR_LOW_MEMORY = 10;
static const uint8_t ERROR_EEPROM = 11;
static const uint8_t ERROR_DHT = 12;
static const uint8_t ERROR_BH1750 = 13;
static const uint8_t ERROR_DS18B20 = 14;
static const uint8_t ERROR_NO_SUBSTRATE = 15;
// Define state map keys constants
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
// Define constants
static const uint8_t ENHANCED_MODE = 2; // edit mode
static const uint16_t TEST_ENABLE = 20000;
static const bool ONE_BLINK = 1;
static const uint16_t MILLIS_TO_SEC = 1000;
static const bool ONE_SEC = 1;
static const uint8_t TEN_SEC = 10;
static const uint8_t HALF_MIN = 30;
static const uint8_t SEC_TO_MIN = 60;
static const uint16_t FIVE_MIN = 300;

class LcdMenu
{
public:
  uint8_t editMode;
  uint8_t menuItem;
  int nextItem;

  void begin() {
    // Load settings
    storage.load();
    // Configure output
    fdev_setup_stream(&lcd_out, lcd_putchar, NULL, _FDEV_SETUP_WRITE);
    // Configure lcd
    lcd.begin();
    // load custom characters
    lcd.createChar(C_CELCIUM, settings.c_celcium);
    //lcd.createChar(C_HEART, settings.c_heart);
    lcd.createChar(C_HUMIDITY, settings.c_humidity);
    lcd.createChar(C_TEMP, settings.c_temp);
    //lcd.createChar(C_FLOWER, settings.c_flower);
    lcd.createChar(C_LAMP, settings.c_lamp);
  }

  void update() {
    // timer for 1 sec
    if(millis()/MILLIS_TO_SEC - lastUpdate >= ONE_SEC) {
      lastUpdate = millis()/MILLIS_TO_SEC;
      // keep home screen and sleeping
      keepDefault();
      // update clock      
      if(editMode == false)
        clock = rtc.now();
      // update lcd
      show();
    }
    // update beep
    if(settings.silentMorning <= clock.hour() && 
      clock.hour() < settings.silentEvening)
    beep.update();
  }

  void keepDefault() {
    // less 30 sec after touch
    if(lastTouch+HALF_MIN > lastUpdate || menuItem == TEST) {
      return;
    }
    // return to home
    if(menuItem != HOME) {
      menuItem = HOME; 
      editMode = false;
    }
    // 5 min after touch
    if(lcd.isBacklight() && lastTouch+FIVE_MIN < lastUpdate) {
      // switch off backlight
      lcd.setBacklight(false);
    }
  }

  void show() {
    // check button click
    if(nextItem != false) {
      lastTouch = millis()/MILLIS_TO_SEC;
      beep.play(ONE_BEEP);
      // enable backlight
      if(lcd.isBacklight() == false) {
        lcd.setBacklight(true);
        // reset click
        nextItem = false;
        // reset home screen
      	homeScreenItem = 0;  
        return;
      }
    }
    // check edit mode
    if(editMode == false || editMode == ENHANCED_MODE) {
      // move forward to next menu
      menuItem += nextItem;
      // don't change settings
      nextItem = false;
      textBlink = false;
      editMode = false;
    }
    if(menuItem != HOME || 
        states[WARNING] != NO_WARNING ||
        states[ERROR] != NO_ERROR) {
      // reset home screen
      homeScreenItem = 0;  
    }
    // error screen
    if(states[ERROR] != NO_ERROR && 
        lastTouch+TEN_SEC <= lastUpdate) {
      showAlert();
      return;
    }
    // warning screen
    if(states[WARNING] != NO_WARNING && 
        lastTouch+TEN_SEC <= lastUpdate) {
      showWarning();
      return;
    }
    // main screen
    if(menuItem == HOME) {
      homeScreen();
      return;
    }
    // menu screen
    showMenu();
  }

  void showMenu() {
    if(editMode != false) {
      // enable blink for edit mode
      textBlink = true;
      // requested save settings
      storage.changed = true;
    }
    // print menu
    lcd.home();
    switch (menuItem) {
      case WATERING_DURATION:
        fprintf_P(&lcd_out, PSTR("Watering durat. \nfor {%2d} min      "), 
          settings.wateringDuration += nextItem);
        break;
      case WATERING_SUNNY:
        menuPeriod(PSTR("Watering sunny  \n"), 
          (settings.wateringSunnyPeriod += nextItem));
        break;
      case WATERING:
        menuPeriod(PSTR("Watering period \n"), 
          (settings.wateringPeriod += nextItem));
        break;
      case MISTING_DURATION:
        fprintf_P(&lcd_out, PSTR("Misting duration\nfor {%2d} sec      "), 
          settings.mistingDuration += nextItem);
        break;
      case MISTING_SUNNY:
        menuPeriod(PSTR("Misting sunny   \n"),
          (settings.mistingSunnyPeriod += nextItem));
        break;
      case MISTING:
        menuPeriod(PSTR("Misting period  \n"), 
          (settings.mistingPeriod += nextItem));
        break;
      case LIGHT_MINIMUM:
        fprintf_P(&lcd_out, PSTR("Light not less  \nthan {%4d} lux   "), 
          settings.lightMinimum += nextItem);
        break;
      case LIGHT_DAY_DURATION:
        fprintf_P(&lcd_out, PSTR("Light day       \nduration {%2d}h    "), 
          settings.lightDayDuration += nextItem);
        break;
      case LIGHT_DAY_START:
        fprintf_P(&lcd_out, PSTR("Light day from  \n"));
        if(editMode == false) {
          fprintf_P(&lcd_out, PSTR("%02d:%02d to %02d:%02d  "), 
            settings.lightDayStart/SEC_TO_MIN, settings.lightDayStart%SEC_TO_MIN,
            (settings.lightDayStart/SEC_TO_MIN)+settings.lightDayDuration, 
            settings.lightDayStart%SEC_TO_MIN);
        } else {
          fprintf_P(&lcd_out, PSTR("{%2d} o'clock      "), 
            (settings.lightDayStart += (nextItem*SEC_TO_MIN))/SEC_TO_MIN);
        }
        break;
      case HUMIDITY_MINIMUM:
        fprintf_P(&lcd_out, PSTR("Humidity not    \nless than {%2d}%%   "), 
          settings.humidMinimum += nextItem);
        break;
      case HUMIDITY_MAXIMUM:
        fprintf_P(&lcd_out, PSTR("Humidity not    \ngreater than {%2d}%%"), 
          settings.humidMaximum += nextItem);
        break;
      case AIR_TEMP_MINIMUM:
        fprintf_P(&lcd_out, PSTR("Air temper. not \nless than {%2d}%c   "), 
          settings.airTempMinimum += nextItem, C_CELCIUM);
        break;
      case AIR_TEMP_MAXIMUM:
        fprintf_P(&lcd_out, PSTR("Air temper. not \ngreater than {%2d}%c"), 
          settings.airTempMaximum += nextItem, C_CELCIUM);
        break;
      case SUBSTRATE_TEMP_MINIMUM:
        fprintf_P(&lcd_out, PSTR("Substrate not   \nless than {%2d}%c   "), 
          settings.subsTempMinimum += nextItem, C_CELCIUM);
        break;
      case SILENT_NIGHT:
        fprintf_P(&lcd_out, PSTR("Silent night    \n"));
        uint8_t blinkFrom, blinkTo;
        if(editMode == true || editMode == 4) {
          editMode = 4;
          blinkFrom = 5;
          blinkTo = 6;
          settings.silentEvening += nextItem;
        } else {
          blinkFrom = 12;
          blinkTo = 13;
          settings.silentMorning += nextItem;
        }
        fprintf_P(&lcd_out, PSTR("from %2dh to %2d}h "),
          settings.silentEvening, settings.silentMorning);
        if(editMode != false)
          textBlinkPos(1, blinkFrom, blinkTo);
        break;
      case TEST:
        textBlink = true;
        if(editMode == false) {
          fprintf_P(&lcd_out, PSTR("Test all systems\n       -> {Start?}"));
          // disable test mode
          if(settings.lightMinimum == TEST_ENABLE) {   
            // restore previous settings
            settings = test;
          }
        } else {
          fprintf_P(&lcd_out, PSTR("Testing.....    \n        -> {Stop?}"));
          storage.changed = false;
          // enable test mode
          if(settings.lightMinimum != TEST_ENABLE) {
            // save previous settings
            test = settings;
            // change settings for test
            settings.lightMinimum = TEST_ENABLE;
            settings.lightDayDuration = 18; //hours
            settings.mistingSunnyPeriod = 1; // min
            settings.mistingPeriod = 1; //min
            settings.wateringSunnyPeriod = 2; // min
            settings.wateringPeriod = 2; //min
            settings.silentMorning = 0; // hour
            settings.silentEvening = 24; // hour

            settings.wateringDuration = 1; //min
            settings.mistingDuration = 10; //sec
          }
        }
        break;
      case 255: 
        menuItem = CLOCK;
      case CLOCK:
        clockScreen();
        break;
      default:
        menuItem = HOME;
        break;
    }
    nextItem = false;
  }

private:
  unsigned long lastTouch;
  unsigned long lastUpdate;
  uint8_t homeScreenItem;

  void textBlinkPos(uint8_t _row, uint8_t _start, uint8_t _end) {
    while(textErase && _start <= _end) {
      lcd.setCursor(_start, _row); fprintf_P(&lcd_out, PSTR(" "));
      _start++;
    }
  }

  void backlightBlink(uint8_t _count) {
    for(uint8_t i=0; i<_count; i++) {
      lcd.setBacklight(false); delay(250);
      lcd.setBacklight(true); delay(250);
    }
  }

  void homeScreen() {
    textBlink = true;
    lcd.home();
    //fprintf_P(&lcd_out, PSTR("%c%c%c%c%c%c%c    %02d{:}%02d\n"), 
    //  C_FLOWER, C_FLOWER, C_FLOWER, C_FLOWER, C_HEART, 
    //  C_FLOWER, C_HEART, clock.hour(), clock.minute());
    fprintf_P(&lcd_out, PSTR("           %02d{:}%02d\n"), 
      clock.hour(), clock.minute());

    if(homeScreenItem >= 16)
      homeScreenItem = 0;
    switch (homeScreenItem) {
      case 0:
        fprintf_P(&lcd_out, PSTR("Air: %c %2d%c %c %2d%%"),
          C_TEMP, states[AIR_TEMP], C_CELCIUM, C_HUMIDITY, 
          states[HUMIDITY]);
        break;
      case 4:
        fprintf_P(&lcd_out, PSTR("Substrate: %c %2d%c"),
          C_TEMP, states[SUBSTRATE_TEMP], C_CELCIUM);
        break;
      case 8:
        fprintf_P(&lcd_out, PSTR("Light: %c %4dlux"),
          C_LAMP, states[LIGHT]);
        break;
      case 12:
        fprintf_P(&lcd_out, PSTR("Computer:  %c %2d%c"),
          C_TEMP, states[COMPUTER_TEMP], C_CELCIUM);
        break;
    }
    homeScreenItem++;
  }

  void clockScreen() {
    uint8_t hour = clock.hour();
    uint8_t minute = clock.minute();
    uint8_t day = clock.day();
    uint8_t month = clock.month();
    uint16_t year = clock.year();
    // show clock
    if(editMode == false) {
      fprintf_P(&lcd_out, PSTR("Time:   %02d:%02d:%02d\nDate: %02d-%02d-%4d"), 
        hour, minute, clock.second(), day, month, year);
      return;
    }
    // edit mode
    fprintf_P(&lcd_out, PSTR("Setting time    \n%02d:%02d %02d-%02d-%4d}"), 
      hour, minute, day, month, year);
    storage.changed = false;
    uint8_t blinkFrom, blinkTo;
    switch (editMode) {
      case true:
        editMode = HOUR_SETTING;
      case HOUR_SETTING:
        hour += nextItem;
        if(hour > 23)
          hour = 0;
        blinkFrom = 0;
        blinkTo = 1;
        break;
      case MINUTE_SETTING:
        minute += nextItem;
        if(minute > 59)
          minute = 0;
        blinkFrom = 3;
        blinkTo = 4;
        break;
      case DAY_SETTING:
        day += nextItem;
        if(day < 1 || day > 31)
          day = 1;
        blinkFrom = 6;
        blinkTo = 7;
        break;
      case MONTH_SETTING:
        month += nextItem;
        if(month < 1 || month > 12)
          month = 1;  
        blinkFrom = 9;
        blinkTo = 10;
        break;
      case YEAR_SETTING:
        year += nextItem;
        if(year < 2014 || year > 2024)
          year = 2014;
        blinkFrom = 12;
        blinkTo = 15;
        break;
    }
    textBlinkPos(1, blinkFrom, blinkTo);
    clock = DateTime(year, month, day, hour, minute);
  }

  void menuPeriod(const char * __fmt, uint8_t _period) {
    fprintf_P(&lcd_out, __fmt);
    if(_period > 0 || editMode != false) {
      fprintf_P(&lcd_out, PSTR("every {%3d} min   "), _period); 
    } else {
      fprintf_P(&lcd_out, PSTR("is disable      ")); 
    }
  }

  void showWarning() {
    lcd.setBacklight(true);
    textBlink = true;
    lcd.home();
    switch (states[WARNING]) { 
      case WARNING_SUBSTRATE_LOW:
        fprintf_P(&lcd_out, PSTR("Low substrate!  \n{Please add some!}"));
        return;
      case INFO_SUBSTRATE_FULL:
        fprintf_P(&lcd_out, PSTR("Substrate tank  \nis full! :)))   "));
        backlightBlink(ONE_BLINK);
        return;
      case INFO_SUBSTRATE_DELIVERED:
        fprintf_P(&lcd_out, PSTR("Substrate was   \ndelivered! :))) "));
        return;
      case WARNING_SUBSTRATE_COLD:
        fprintf_P(&lcd_out, PSTR("Substrate is too\ncold! {:(}        "));
        beep.play(TWO_BEEP);
        return;
      case WARNING_AIR_COLD:
        fprintf_P(&lcd_out, PSTR("Air is too cold \nfor plants! {:(}  "));
        beep.play(ONE_BEEP);
        return;
      case WARNING_AIR_HOT:
        fprintf_P(&lcd_out, PSTR("Air is too hot \nfor plants! {:(}  "));
        beep.play(ONE_BEEP);
        return;
      case WARNING_NO_WATER:
        fprintf_P(&lcd_out, PSTR("Misting error!  \nNo water! {:(}    "));
        //beep.play(ONE_BEEP);
        return;                
      case WARNING_WATERING:
        fprintf_P(&lcd_out, PSTR("Watering...     \n{Please wait.}    "));
        return;
      case WARNING_MISTING:
        fprintf_P(&lcd_out, PSTR("Misting...      \nPlease wait.    "));
        return;
    }  
  }

  void showAlert() {
    beep.play(FIVE_BEEP);
    backlightBlink(ONE_BLINK);
    textBlink = true;
    lcd.home();
    switch (states[ERROR]) {
      case ERROR_LOW_MEMORY:
        fprintf_P(&lcd_out, PSTR("MEMORY ERROR!   \n{Low memory!}     "));
        return;
      case ERROR_EEPROM:
        fprintf_P(&lcd_out, PSTR("EEPROM ERROR!   \n{Settings reset!} "));
        return;
      case ERROR_DHT:
        fprintf_P(&lcd_out, PSTR("DHT ERROR!      \n{Check connection}"));
        return;
      case ERROR_BH1750:
        fprintf_P(&lcd_out, PSTR("BH1750 ERROR!   \n{Check connection}"));
        return;
      case ERROR_DS18B20:
        fprintf_P(&lcd_out, PSTR("DS18B20 ERROR!  \n{Check connection}"));
        return;
      case ERROR_NO_SUBSTRATE:
        fprintf_P(&lcd_out, PSTR("No substrate!   \n{Plants can die!} "));
        return;
    }  
  }

};

#endif // __LCDMENU_H__
