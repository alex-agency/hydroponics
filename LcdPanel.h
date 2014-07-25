#ifndef LCDPANEL_H
#define LCDPANEL_H

#include "LiquidCrystal_I2C.h"
#include "OneButton.h"
#include "Settings.h"
#include "RTClib.h"
#include "Melody.h"

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

// Declare output
static bool charErase = false;
static bool textErase = false;
static bool textBlink = false;
static int lcd_putchar(char c, FILE *) {
  switch(c) {
    case '\n':
      lcd.setCursor(0,1);
      break;
    case '{':
      if(textErase)
        charErase = true;
      break;
    case '}':
      charErase = false;
      textErase = !textErase;
      break;
    default:
      if(textBlink && charErase)
        lcd.write(' ');
      else
        lcd.write(c);
  }
  return 0;
};
FILE lcd_out = {0};

// Declare push buttons
OneButton rightButton(A2, true);
OneButton leftButton(A3, true);

// Declare Speaker digital pin
Melody melody(8);

// Declare settings
EEPROM storage;

// Declare RTC
RTC_DS1307 rtc;
DateTime clock;

// Declare LCD menu items
#define HOME 0
#define WATERING_DURATION  1
#define WATERING_SUNNY  2
#define WATERING_NIGHT 3
#define WATERING_OTHER 4
#define MISTING_DURATION 5
#define MISTING_SUNNY 6
#define MISTING_NIGHT 7
#define MISTING_OTHER  8
#define LIGHT_MINIMUM  9
#define LIGHT_DAY_DURATION  10
#define LIGHT_DAY_START  11
#define HUMIDITY_MINIMUM  12
#define HUMIDITY_MAXIMUM  13
#define AIR_TEMP_MINIMUM  14
#define AIR_TEMP_MAXIMUM  15
#define SUBSTRATE_TEMP_MINIMUM  16
#define TEST  17
#define CLOCK  18
// Declare warning states
static const uint8_t WARNING = 10;
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
static const uint8_t ERROR = 11;
#define NO_ERROR  0
#define ERROR_LOW_MEMORY  10
#define ERROR_EEPROM  11
#define ERROR_DHT  12
#define ERROR_BH1750  13
#define ERROR_DS18B20  14
#define ERROR_NO_SUBSTRATE  15

class LcdPanel 
{
public:
  uint8_t editMode;
  uint8_t menuItem;
  bool testMode;

  void begin() {
    // Configure output
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
    // init values
    lastTouch = millis()/1000;
    lastUpdate = lastTouch;
    editMode, menuItem, homeScreenItem = 0;
    // Load settings
    storage.load();
    // "R2D2" melody
    melody.play(R2D2);
  }

  void update() {
    // timer fo 1 sec
    if((millis()/1000) - lastUpdate >= 1) {
      lastUpdate = millis()/1000;
      // 10 sec after click
      if(lastTouch+10 < lastUpdate) {
        if(rtc.readnvram(ERROR) != NO_ERROR)
          lcdAlert();
        else if(rtc.readnvram(WARNING) != NO_WARNING)
          lcdWarning();
      } else
      // 30 sec after click
      if(lastTouch+30 < lastUpdate && 
          menuItem != HOME && 
          menuItem != TEST) {
        editMode = 0;
        menuItem = HOME;
      } else
      // 5 min after click
      if(lastTouch+300 < lastUpdate && 
          rtc.readnvram(WARNING) == NO_WARNING &&
          lcd.isBacklight()) {
        // switch off backlight
        lcd.setBacklight(false);
      } else
      if(editMode == 0)
        // update clock
        clock = rtc.now();
      // update LCD
      lcdUpdate();
    }
    // update push buttons
    leftButton.tick();
    rightButton.tick();
    // update melody
    melody.update();
  }

  void lcdUpdate() {
    if(direction != 0) {
      melody.beep(1);
      lastTouch = millis()/1000;
      // enable backlight
      if(lcd.isBacklight() == false) {
        lcd.setBacklight(true);
        return;
      }
    }
    if(editMode == 0) {
      // move forward to next menu
      menuItem += direction;
      // don't settings values
      direction = 0;
      textBlink = false;
    } else {
      // enable blink
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
          settings.wateringDuration += direction);
        break;
      case WATERING_SUNNY:
        menuPeriod(PSTR("Watering sunny  \n"), 
          &(settings.wateringSunnyPeriod += direction));
        break;
      case WATERING_NIGHT:
        menuPeriod(PSTR("Watering night  \n"), 
          &(settings.wateringNightPeriod += direction));
        break;
      case WATERING_OTHER:
        menuPeriod(PSTR("Watering evening\n"), 
          &(settings.wateringOtherPeriod += direction));
        break;
      case MISTING_DURATION:
        fprintf_P(&lcd_out, PSTR("Misting duration\nfor {%2d} sec      "), 
          settings.mistingDuration += direction);
        break;
      case MISTING_SUNNY:
        menuPeriod(PSTR("Misting sunny   \n"),
          &(settings.mistingSunnyPeriod += direction));
        break;
      case MISTING_NIGHT:
        menuPeriod(PSTR("Misting night   \n"), 
          &(settings.mistingNightPeriod += direction));
        break;
      case MISTING_OTHER:
        menuPeriod(PSTR("Misting evening \n"), 
          &(settings.mistingOtherPeriod += direction));
        break;
      case LIGHT_MINIMUM:
        fprintf_P(&lcd_out, PSTR("Light not less  \nthan {%4d} lux   "), 
          settings.lightMinimum += direction);
        break;
      case LIGHT_DAY_DURATION:
        fprintf_P(&lcd_out, PSTR("Light day       \nduration {%2d}h    "), 
          settings.lightDayDuration += direction);
        break;
      case LIGHT_DAY_START:
        fprintf_P(&lcd_out, PSTR("Light day from  \n"));
        if(editMode == 0) {
          fprintf_P(&lcd_out, PSTR("%02d:%02d to %02d:%02d  "), 
            settings.lightDayStart/60, settings.lightDayStart%60,
            (settings.lightDayStart/60)+settings.lightDayDuration, 
            settings.lightDayStart%60);
        } else {
          fprintf_P(&lcd_out, PSTR("day start at {%2d}h"), 
            (settings.lightDayStart += direction)/60);
        }
        break;
      case HUMIDITY_MINIMUM:
        fprintf_P(&lcd_out, PSTR("Humidity not    \nless than {%2d}%%   "), 
          settings.humidMinimum += direction);
        break;
      case HUMIDITY_MAXIMUM:
        fprintf_P(&lcd_out, PSTR("Humidity not    \ngreater than {%2d}%%"), 
          settings.humidMaximum += direction);
        break;
      case AIR_TEMP_MINIMUM:
        fprintf_P(&lcd_out, PSTR("Temp. air not   \nless than {%2d}%c   "), 
          settings.airTempMinimum += direction, char_celcium);
        break;
      case AIR_TEMP_MAXIMUM:
        fprintf_P(&lcd_out, PSTR("Temp. air not   \ngreater than {%2d}%c"), 
          settings.airTempMaximum += direction, char_celcium);
        break;
      case SUBSTRATE_TEMP_MINIMUM:
        fprintf_P(&lcd_out, PSTR("Substrate temp. \nless than {%2d}%c   "), 
          settings.subsTempMinimum += direction, char_celcium);
        break;
      case TEST:
        storage.changed = false;
        if(editMode == 0) {
          textBlink = true;
          fprintf_P(&lcd_out, PSTR("Test all systems\n       -> {Start?}"));
          testMode = false;
        } else {
          fprintf_P(&lcd_out, PSTR("Testing.....    \n        -> {Stop?}"));
          testMode = true;
        }
        break;
      case 255: 
        menuItem = CLOCK;
      case CLOCK:
        storage.changed = false;
        /*if(editMode == 0) {
          //fprintf_P(&lcd_out, PSTR("Time:   %02d:%02d:%02d\nDate: %02d-%02d-%4d"), 
          //  clock.hour(), clock.minute(), clock.second(), 
          //  clock.day(), clock.month(), clock.year());
        } else 
        if(direction != 0) {
          //settingClock(direction);
          // clock not used storage
          //storage.changed = false;
        } 
        else {
          fprintf_P(&lcd_out, PSTR("Setting time    \n%02d:%02d %02d-%02d-%4d"), 
            clock.hour(), clock.minute(), clock.day(), clock.month(), clock.year());
          if(editModeCursor == 4) {
            panel.textBlink(1, 0, 1);
          } else 
          if(editModeCursor == 3) {
            panel.textBlink(1, 3, 4);
          } else 
          if(editModeCursor == 2) {
            panel.textBlink(1, 6, 7);
          } else 
          if(editModeCursor == 1) {
            panel.textBlink(1, 9, 10);
          } else {
            panel.textBlink(1, 12, 15);       
          }
        }
        editModeCursor = 4;*/
        break;
      default:
        menuItem = HOME;
        menuHomeItem = 0;
        break;
    }
    direction = 0;
  }

  void backlightBlink(uint8_t _count) {
    for(uint8_t i=0; i<_count; i++) {
      lcd.setBacklight(false); delay(250);
      lcd.setBacklight(true); delay(250);
    }
  }

private:
  uint16_t lastTouch;
  uint16_t lastUpdate;
  uint8_t homeScreenItem;
  int direction;

  void homeScreen() {
    textBlink = true;
    fprintf_P(&lcd_out, PSTR("%c%c%c%c%c%c%c    %02d{:}%02d\n"), 
      char_flower, char_flower, char_flower, char_flower, char_heart, 
      char_flower, char_heart, clock.hour(), clock.minute());

    if(menuHomeItem >= 16)
      menuHomeItem = 0;
    switch (menuHomeItem) {
      case 0:
        fprintf_P(&lcd_out, PSTR("Air: %c %2d%c %c %2d%%"),
          char_temp, rtc.readnvram(AIR_TEMP), char_celcium, char_humidity, 
          rtc.readnvram(HUMIDITY));
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

  void menuPeriod(const char * __fmt, uint8_t * _period) {
    fprintf_P(&lcd_out, __fmt);
    if(_period > 0 || menuEdit > 0) {
      fprintf_P(&lcd_out, PSTR("every {%3d} min   "), _period); 
    } else {
      fprintf_P(&lcd_out, PSTR("is disable      ")); 
    }
  }

  /*void settingClock(int _direction) {
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
  }*/

  void lcdWarning() {
    lcd.setBacklight(true);
    textBlink = true;
    lcd.home();
    switch (rtc.readnvram(WARNING)) { 
      case WARNING_SUBSTRATE_LOW:
        fprintf_P(&lcd_out, PSTR("Low substrate!  \n{Please add some!}"));
        melody.beep(1);
        return;
      case INFO_SUBSTRATE_FULL:
        fprintf_P(&lcd_out, PSTR("Substrate tank  \nis full! :)))   "));
        lcdBacklightBlink(1);
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
        melody.beep(1);
        return;
      case WARNING_AIR_HOT:
        fprintf_P(&lcd_out, PSTR("Air is too hot \nfor plants! {:(}  "));
        melody.beep(1);
        return;
      case WARNING_SUBSTRATE_COLD:
        fprintf_P(&lcd_out, PSTR("Substrate is too\ncold! {:(}        "));
        melody.beep(2);
        return;
      case WARNING_NO_WATER:
        fprintf_P(&lcd_out, PSTR("Misting error!  \nNo water! {:(}    "));
        melody.beep(1);
        return;
    }  
  }

  void lcdAlert() {
    melody.beep(5);
    lcdBacklightBlink(1);
    textBlink = true;
    lcd.home();
    switch (rtc.readnvram(ERROR)) {
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

  void leftButtonClick() {
    direction = -1;
    panel.lcdUpdate();
  }

  void rightButtonClick() {
    direction = 1;
    panel.lcdUpdate();
  }

  void buttonsLongPress() {
    melody.beep(2);
    if(editMode > 0) {
      // move to next edit field
      editMode--;
      if(menuItem == CLOCK)
        // save changed time
        rtc.adjust(clock);
    } else if(menuItem != HOME) {
      // enable edit mode
      editMode = 1;
    } else {
      // close Edit menu
      editMode = 0;
    }
    panel.lcdUpdate();
  }

} panel;

// Configure buttons
rightButton.attachClick( panel.rightButtonClick );
rightButton.attachLongPressStart( panel.buttonsLongPress );
leftButton.attachClick( panel.leftButtonClick );
leftButton.attachLongPressStart( panel.buttonsLongPress );

#endif // __LCDPANEL_H__
