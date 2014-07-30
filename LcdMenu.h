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
static bool charErase, textErase, textBlink = false;
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
SimpleMap<uint8_t, uint16_t, 11> states;

// Declare custom LCD characters
static const uint8_t C_CELCIUM = 0;
static const uint8_t C_HEART = 1;
static const uint8_t C_HUMIDITY = 2;
static const uint8_t C_TEMP = 3;
static const uint8_t C_FLOWER = 4;
static const uint8_t C_LAMP = 5;
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
// Declare state map keys constants
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
    lcd.createChar(C_HEART, settings.c_heart);
    lcd.createChar(C_HUMIDITY, settings.c_humidity);
    lcd.createChar(C_TEMP, settings.c_temp);
    lcd.createChar(C_FLOWER, settings.c_flower);
    lcd.createChar(C_LAMP, settings.c_lamp);
    // init values
    lastUpdate, lastTouch = millis()/1000;
  }

  void update() {
    uint16_t now = millis()/1000;
    // timer fo 1 sec
    if(now - lastUpdate >= 1) {
      lastUpdate = now;
      // keep home screen and sleep
      checkTouch();
      // update clock      
      if(editMode == 0)
        clock = rtc.now();
      // update menu
      showMenu();
    }
    // update beep
    beep.update();
  }

  void checkTouch() {
    // check button click
    if(nextItem != 0) {
      lastTouch = millis()/1000;
      beep.play(1);
      // enable backlight
      if(lcd.isBacklight() == false) {
        lcd.setBacklight(true);
        // reset click
        nextItem = 0;
      }
      return;
    }
    // less 10 sec after touch
    if(lastTouch+10 > lastUpdate) {
      return;
    }
    // check error
    if(states[ERROR] != NO_ERROR) {
      showAlert();
      return;
    }
    // check warnings
    if(states[WARNING] != NO_WARNING) {
      showWarning();
      return;
    }
    // less 30 sec after touch
    if(menuItem == TEST || lastTouch+30 > lastUpdate) {
      return;
    }
    // return to home
    if(menuItem != HOME) {
      menuItem = HOME; 
      editMode = false;
      homeScreenItem = 0;
    }
    // 5 min after touch
    if(lcd.isBacklight() && lastTouch+300 < lastUpdate) {
      // switch off backlight
      lcd.setBacklight(false);
    }
  }

  void showMenu() {
    // check edit mode
    if(editMode == false || editMode == 2) {
      // move forward to next menu
      menuItem += nextItem;
      // don't change settings
      nextItem = 0;
      textBlink, editMode = false;
    } else {
      // enable blink for edit mode
      textBlink = true;
    }
    // print menu
    lcd.home();
    switch (menuItem) {
      case HOME:
        homeScreen();
        break;
      case WATERING_DURATION:
        fprintf_P(&lcd_out, PSTR("Watering durat. \nfor {%2d} min      "), 
          settings.wateringDuration += nextItem);
        break;
      case WATERING_SUNNY:
        menuPeriod(PSTR("Watering sunny  \n"), 
          (settings.wateringSunnyPeriod += nextItem));
        break;
      case WATERING_NIGHT:
        menuPeriod(PSTR("Watering night  \n"), 
          (settings.wateringNightPeriod += nextItem));
        break;
      case WATERING_OTHER:
        menuPeriod(PSTR("Watering evening\n"), 
          (settings.wateringOtherPeriod += nextItem));
        break;
      case MISTING_DURATION:
        fprintf_P(&lcd_out, PSTR("Misting duration\nfor {%2d} sec      "), 
          settings.mistingDuration += nextItem);
        break;
      case MISTING_SUNNY:
        menuPeriod(PSTR("Misting sunny   \n"),
          (settings.mistingSunnyPeriod += nextItem));
        break;
      case MISTING_NIGHT:
        menuPeriod(PSTR("Misting night   \n"), 
          (settings.mistingNightPeriod += nextItem));
        break;
      case MISTING_OTHER:
        menuPeriod(PSTR("Misting evening \n"), 
          (settings.mistingOtherPeriod += nextItem));
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
            settings.lightDayStart/60, settings.lightDayStart%60,
            (settings.lightDayStart/60)+settings.lightDayDuration, 
            settings.lightDayStart%60);
        } else {
          fprintf_P(&lcd_out, PSTR("{%2d} o'clock      "), 
            (settings.lightDayStart += nextItem)/60);
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
        fprintf_P(&lcd_out, PSTR("Temp. air not   \nless than {%2d}%c   "), 
          settings.airTempMinimum += nextItem, C_CELCIUM);
        break;
      case AIR_TEMP_MAXIMUM:
        fprintf_P(&lcd_out, PSTR("Temp. air not   \ngreater than {%2d}%c"), 
          settings.airTempMaximum += nextItem, C_CELCIUM);
        break;
      case SUBSTRATE_TEMP_MINIMUM:
        fprintf_P(&lcd_out, PSTR("Substrate temp. \nless than {%2d}%c   "), 
          settings.subsTempMinimum += nextItem, C_CELCIUM);
        break;
      case TEST:
        storage.changed = false;
        textBlink = true;
        if(editMode == false) {
          fprintf_P(&lcd_out, PSTR("Test all systems\n       -> {Start?}"));
          // disable test mode
          if(settings.lightMinimum == 10000) {   
            // restore previous settings
            settings = test;
          }
        } else {
          fprintf_P(&lcd_out, PSTR("Testing.....    \n        -> {Stop?}"));
          // enable test mode
          if(settings.lightMinimum != 10000) {
            // save previous settings
            test = settings;
            // change settings for test
            settings.lightMinimum = 10000; //lux
            settings.lightDayDuration = 18; //hours
            settings.mistingSunnyPeriod, 
            settings.mistingNightPeriod, 
            settings.mistingOtherPeriod = 1; //min
            settings.wateringSunnyPeriod, 
            settings.wateringNightPeriod,
            settings.wateringOtherPeriod = 3; //min

            settings.wateringDuration = 2; //min
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
        homeScreenItem = 0;
        break;
    }
    nextItem = 0;
  }

private:
  uint16_t lastTouch;
  uint16_t lastUpdate;
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
    fprintf_P(&lcd_out, PSTR("%c%c%c%c%c%c%c    %02d{:}%02d\n"), 
      C_FLOWER, C_FLOWER, C_FLOWER, C_FLOWER, C_HEART, 
      C_FLOWER, C_HEART, clock.hour(), clock.minute());

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
        editMode = 7;
      case 7:
        hour += nextItem;
        if(hour > 23)
          hour = 0;
        blinkFrom = 0;
        blinkTo = 1;
        break;
      case 6:
        minute += nextItem;
        if(minute > 59)
          minute = 0;
        blinkFrom = 3;
        blinkTo = 4;
        break;
      case 5:
        day += nextItem;
        if(day < 1 || day > 31)
          day = 1;
        blinkFrom = 6;
        blinkTo = 7;
        break;
      case 4:
        month += nextItem;
        if(month < 1 || month > 12)
          month = 1;  
        blinkFrom = 9;
        blinkTo = 10;
        break;
      case 3:
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
    if(_period > 0 || editMode > 0) {
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
        beep.play(1);
        return;
      case INFO_SUBSTRATE_FULL:
        fprintf_P(&lcd_out, PSTR("Substrate tank  \nis full! :)))   "));
        backlightBlink(1);
        return;
      case INFO_SUBSTRATE_DELIVERED:
        fprintf_P(&lcd_out, PSTR("Substrate was   \ndelivered! :))) "));
        return;
      case WARNING_WATERING:
        fprintf_P(&lcd_out, PSTR("Watering...     \n{Please wait.}    "));
        return;
      case WARNING_MISTING:
        fprintf_P(&lcd_out, PSTR("Misting...      \nPlease wait.    "));
        return;
      case WARNING_AIR_COLD:
        fprintf_P(&lcd_out, PSTR("Air is too cold \nfor plants! {:(}  "));
        beep.play(1);
        return;
      case WARNING_AIR_HOT:
        fprintf_P(&lcd_out, PSTR("Air is too hot \nfor plants! {:(}  "));
        beep.play(1);
        return;
      case WARNING_SUBSTRATE_COLD:
        fprintf_P(&lcd_out, PSTR("Substrate is too\ncold! {:(}        "));
        beep.play(2);
        return;
      case WARNING_NO_WATER:
        fprintf_P(&lcd_out, PSTR("Misting error!  \nNo water! {:(}    "));
        beep.play(1);
        return;
    }  
  }

  void showAlert() {
    beep.play(5);
    backlightBlink(1);
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
