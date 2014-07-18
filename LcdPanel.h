
#ifndef LCDPANEL_H
#define LCDPANEL_H

#include "LiquidCrystal_I2C.h"
#include "OneButton.h"
#include "Settings.h"
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
static int lcd_putchar(char c, FILE *) {
  if(c == '\n')
    lcd.setCursor(0,1);
  lcd.write(c);
  return 0;
};
FILE lcd_out = {0};

// Declare push buttons
OneButton rightButton(A2, true);
OneButton leftButton(A3, true);

// Declare Speaker digital pin
Melody melody(8);

// Declare 
EEPROM storage;

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

class LcdPanel 
{
public:
  uint16_t lastTouch;
  uint8_t editMode;
  uint8_t menuItem;
  uint8_t homeScreenItem;
  bool storage_ok;

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

    // touch init
    lastTouch = millis()/1000;

    // Load settings
    storage_ok = storage.load();

    // "R2D2" melody
    melody.play(R2D2);
  }

  void update() {
    
    // update push buttons
    leftButton.tick();
    rightButton.tick();
    // update melody
    melody.update();
  }

  void lcdUpdate() {
    // is it come after button click?
    if(editValue != 0) {
      melody.beep(1);
      last_touch = millis()/1000;
      if(lcd.isBacklight() == false) {
        lcd.setBacklight(true);
        return;
      }
      // action for simple Menu
      if(editMode == false) {
        // move forward to next menu
        menuItem += editValue;
        return;
      }
      // mark settings for save
      storage.changed = true;
    }

    lcd.home();
    switch (menuItem) {
      case HOME:
        homeScreen();
        break;
      case WATERING_DURATION:
        fprintf_P(&lcd_out, PSTR("Watering durat. \nfor %2d min      "), 
          settings.wateringDuration);
        if(editMode) {
          panel.textBlink(1, 4, 5);
          settings.wateringDuration += editValue;
        }
        break;
      case WATERING_SUNNY:
        menuPeriod(PSTR("Watering sunny  "), &settings.wateringSunnyPeriod);
        break;
      case WATERING_NIGHT:
        menuPeriod(PSTR("Watering night  "), &settings.wateringNightPeriod);
        break;
      case WATERING_OTHER:
        menuPeriod(PSTR("Watering evening"), &settings.wateringOtherPeriod);
        break;
      case MISTING_DURATION:
        fprintf_P(&lcd_out, PSTR("Misting duration\nfor %2d sec      "), 
          settings.mistingDuration);
        if(editMode) {
          panel.textBlink(1, 4, 5);
          settings.mistingDuration += editValue;
        }
        break;
      case MISTING_SUNNY:
        menuPeriod(PSTR("Misting sunny   "), &settings.mistingSunnyPeriod);
        break;
      case MISTING_NIGHT:
        menuPeriod(PSTR("Misting night   "), &settings.mistingNightPeriod);
        break;
      case MISTING_OTHER:
        menuPeriod(PSTR("Misting evening "), &settings.mistingOtherPeriod);
        break;
      case LIGHT_MINIMUM:
        fprintf_P(&lcd_out, PSTR("Light not less  \nthan %4d lux   "), 
          settings.lightMinimum);
        if(editMode) {
          panel.textBlink(1, 5, 8);
          settings.lightMinimum += editValue;
        }
        break;
      case LIGHT_DAY_DURATION:
        fprintf_P(&lcd_out, PSTR("Light day       \nduration %2dh    "), 
          settings.lightDayDuration);
        if(editMode) {
          panel.textBlink(1, 9, 10);
          settings.lightDayDuration += editValue;
        }
        break;
      case LIGHT_DAY_START:
        fprintf_P(&lcd_out, PSTR("Light day from  \n"));
        if(editMode == false) {
          fprintf_P(&lcd_out, PSTR("%02d:%02d to %02d:%02d  "), 
            settings.lightDayStart/60, settings.lightDayStart%60,
            (settings.lightDayStart/60)+settings.lightDayDuration, 
            settings.lightDayStart%60);
        } else {
          fprintf_P(&lcd_out, PSTR("day start at %2dh"), 
            settings.lightDayStart/60);
          panel.textBlink(1, 13, 14);
          settings.lightDayStart += editValue;
        }
        break;
      case HUMIDITY_MINIMUM:
        fprintf_P(&lcd_out, PSTR("Humidity not    \nless than %2d%%   "), 
          settings.humidMinimum);
        if(editMode) {
          panel.textBlink(1, 10, 11);
          settings.humidMinimum += editValue;
        }
        break;
      case HUMIDITY_MAXIMUM:
        fprintf_P(&lcd_out, PSTR("Humidity not    \ngreater than %2d%%"), 
          settings.humidMaximum);
        if(editMode) {
          panel.textBlink(1, 13, 14);
          settings.humidMaximum += editValue;
        }
        break;
      case AIR_TEMP_MINIMUM:
        fprintf_P(&lcd_out, PSTR("Temp. air not   \nless than %2d%c   "), 
          settings.airTempMinimum, char_celcium);
        if(editMode) {
          panel.textBlink(1, 10, 11);
          settings.airTempMinimum += editValue;
        }
        break;
      case AIR_TEMP_MAXIMUM:
        fprintf_P(&lcd_out, PSTR("Temp. air not   \ngreater than %2d%c"), 
          settings.airTempMaximum, char_celcium);
        if(editMode) {
          panel.textBlink(1, 13, 14);
          settings.airTempMaximum += editValue;
        }
        break;
      case SUBSTRATE_TEMP_MINIMUM:
        fprintf_P(&lcd_out, PSTR("Substrate temp. \nless than %2d%c   "), 
          settings.subsTempMinimum, char_celcium);
        if(editMode) {
          panel.textBlink(1, 10, 11);
          settings.subsTempMinimum += editValue;
        }
        break;
      case TEST:
        if(editMode == false) {
          fprintf_P(&lcd_out, PSTR("Test all systems\n       -> Start?"));
          doTest(false);
        } else {
          fprintf_P(&lcd_out, PSTR("Testing.....    \n        -> Stop?"));
          doTest(true);
        }
        panel.textBlink(1, 10, 15);
        if(editValue != 0) {
          editMode = false;
          //storage.changed = false;
          //doTest(false);
        }
        break;
      case 255: 
        menuItem = CLOCK;
      case CLOCK:
        if(editMode == false) {
          //fprintf_P(&lcd_out, PSTR("Time:   %02d:%02d:%02d\nDate: %02d-%02d-%4d"), 
          //  clock.hour(), clock.minute(), clock.second(), 
          //  clock.day(), clock.month(), clock.year());
        } else 
        if(editValue != 0) {
          //settingClock(editValue);
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
        editModeCursor = 4;
        break;
      default:
        menuItem = HOME;
        menuHomeItem = 0;
        break;
    }
    editValue = 0;
    return;
  }

  void textBlink(bool _row, uint8_t _start, uint8_t _end) { 
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

  void backlightBlink(uint8_t _count) {
    for(uint8_t i=0; i<_count; i++) {
      lcd.setBacklight(false); delay(250);
      lcd.setBacklight(true); delay(250);
    }
  }

private:
  int editValue;
  bool lcdBlink;
  bool lcdBacklight;

  void homeScreen() {
    fprintf_P(&lcd_out, PSTR("%c%c%c%c%c%c%c    %02d:%02d"), 
      char_flower, char_flower, char_flower, char_flower, char_heart, 
      char_flower, char_heart, clock.hour(), clock.minute());
    panel.textBlink(0, 13, 13);
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

  void menuPeriod(const char * __fmt, uint8_t * _period) {
    fprintf_P(&lcd_out, __fmt); lcd.setCursor(0,1);
    if(_period == 0 && menuEdit == false) {
      fprintf_P(&lcd_out, PSTR("is disable      "));
      return;
    }
    fprintf_P(&lcd_out, PSTR("every %3d min   "), _period);
    if(menuEdit) {
      panel.textBlink(1, 6, 8);
      _period += editButton;
      return;
    }
  }

  void lcdWarning() {
    lcd.setBacklight(true);
    
    lcd.home();
    switch (rtc.readnvram(WARNING)) { 
      case WARNING_SUBSTRATE_LOW:
        fprintf_P(&lcd_out, PSTR("Low substrate!  \nPlease add some!"));
        melody.beep(1);
        panel.textBlink(1, 0, 15);
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
        panel.textBlink(1, 0, 11);
        return;
      case WARNING_MISTING:
        fprintf_P(&lcd_out, PSTR("Misting...      \nPlease wait.    "));
        return;
      case WARNING_AIR_COLD:
        fprintf_P(&lcd_out, PSTR("Air is too cold \nfor plants! :(  "));
        melody.beep(1);
        panel.textBlink(1, 12, 13);
        return;
      case WARNING_AIR_HOT:
        fprintf_P(&lcd_out, PSTR("Air is too hot \nfor plants! :(  "));
        melody.beep(1);
        panel.textBlink(1, 12, 13);
        return;
      case WARNING_SUBSTRATE_COLD:
        fprintf_P(&lcd_out, PSTR("Substrate is too\ncold! :(        "));
        melody.beep(2);
        panel.textBlink(1, 6, 7);
        return;
      case WARNING_NO_WATER:
        fprintf_P(&lcd_out, PSTR("Misting error!  \nNo water! :(    "));
        melody.beep(1);
        panel.textBlink(1, 0, 12);
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
        panel.textBlink(1, 0, 10);
        return;
      case ERROR_EEPROM:
        fprintf_P(&lcd_out, PSTR("EEPROM ERROR!   \nSettings reset! "));
        panel.textBlink(1, 0, 14);
        return;
      case ERROR_DHT:
        fprintf_P(&lcd_out, PSTR("DHT ERROR!      \nCheck connection"));
        panel.textBlink(1, 0, 15);
        return;
      case ERROR_BH1750:
        fprintf_P(&lcd_out, PSTR("BH1750 ERROR!   \nCheck connection"));
        panel.textBlink(1, 0, 15);
        return;
      case ERROR_DS18B20:
        fprintf_P(&lcd_out, PSTR("DS18B20 ERROR!  \nCheck connection"));
        panel.textBlink(1, 0, 15);
        return;
      case ERROR_NO_SUBSTRATE:
        fprintf_P(&lcd_out, PSTR("No substrate!   \nPlants can die! "));
        panel.textBlink(1, 0, 14);
        return;
    }  
  }

  void leftButtonClick() {
    editValue = -1;
  }

  void rightButtonClick() {
    editValue = 1;
  }

  void buttonsLongPress() {
    melody.beep(2);
    // reset text blink state
    lcdBlink = false;
    // action for simple Menu
    if(menuItem != HOME && menuEdit == false) {
      // enter to Edit Menu and return edit field
      menuEditMode = true;
      panel.update();
      return;
    }
    // action for Edit menu
    if(menuEdit && menuEditCursor > 0) {
      // move to next edit field
      menuEditCursor--;
      panel.update();
      return;
    }
    // save changed time
    if(menuEdit && menuItem == CLOCK) {
      rtc.adjust(clock);
    }
    // close Edit menu
    panel.update();
  }

} panel;

// Configure buttons
rightButton.attachClick( panel.rightButtonClick );
rightButton.attachLongPressStart( panel.buttonsLongPress );
leftButton.attachClick( panel.leftButtonClick );
leftButton.attachLongPressStart( panel.buttonsLongPress );

#endif // __LCDPANEL_H__
