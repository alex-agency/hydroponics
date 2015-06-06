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
static uint8_t blinkCursor, blinkPos;
static int lcd_putchar(char c, FILE *) {
  switch(c) {
    case '\n':
      lcd.setCursor(0,1);
      blinkCursor = 0;
      break;
    case '{':
      blinkCursor++;
      if(textBlink && textErase && 
          (blinkPos == false || blinkPos == blinkCursor))
        charErase = true;
      break;
    case '}':
      if(blinkPos == false || blinkPos == blinkCursor) {
        charErase = false;
        textErase = !textErase;
      }
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
SimpleMap<uint8_t, uint16_t, 17> states;

// Define custom LCD characters
static const uint8_t C_CELCIUM = 0;
static const uint8_t C_HEART = 1;
static const uint8_t C_HUMIDITY = 2;
static const uint8_t C_TEMP = 3;
static const uint8_t C_FLOWER = 4;
static const uint8_t C_LAMP = 5;
static const uint8_t C_UP = 6;
static const uint8_t C_DOWN = 7;
// Define LCD menu items
static const uint8_t HOME = 0;
static const uint8_t WATERING_DURATION = 1;
static const uint8_t WATERING_PERIOD = 2;
static const uint8_t MISTING_DURATION = 3;
static const uint8_t MISTING_PERIOD = 4;
static const uint8_t LIGHT_DURATION = 5;
static const uint8_t LIGHT_DAY_START = 6;
static const uint8_t HUMIDITY_RANGE = 7;
static const uint8_t AIR_TEMP_RANGE = 8;
static const uint8_t SUBSTRATE_TEMP_MINIMUM = 9;
static const uint8_t SILENT_NIGHT = 10;
static const uint8_t EMERGENCE = 11;
static const uint8_t CLOCK = 12;
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
static const uint8_t ERROR_CLOCK = 16;
// Define state map keys constants
static const uint8_t HUMIDITY = 1; // air humidity
static const uint8_t AIR_TEMP = 2;
static const uint8_t COMPUTER_TEMP = 3; // temperature inside
static const uint8_t SUBSTRATE_TEMP = 4;
static const uint8_t LIGHT = 5; // light intensivity
static const uint8_t PUMP_MISTING = 6;
static const uint8_t PUMP_WATERING = 7;
static const uint8_t LAMP = 8;
static const uint8_t WARNING = 9;
static const uint8_t ERROR = 10;
static const uint8_t PREV_HUMIDITY = 11;
static const uint8_t PREV_AIR_TEMP = 12;
static const uint8_t PREV_COMPUTER_TEMP = 13;
static const uint8_t PREV_SUBSTRATE_TEMP = 14;
static const uint8_t PREV_LIGHT = 15;
static const uint8_t WATERING = 16;
static const uint8_t MISTING = 17;
// Define constants
static const uint8_t ENHANCED_MODE = 2; // edit mode
static const bool ONE_BLINK = 1;
static const uint16_t ONE_SEC = 1000;
static const uint16_t HALF_MIN = 30*ONE_SEC;
static const uint16_t ONE_MIN = 60*ONE_SEC;

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
    lcd.createChar(C_UP, settings.c_up);
    lcd.createChar(C_DOWN, settings.c_down);
  }

  void update() {
    // timer for 1 sec
    if(millis() - lastUpdate >= ONE_SEC) {
      lastUpdate = millis();
      // keep home screen and sleeping
      keepDefault();
      // update clock      
      if(editMode == false)
        clock = rtc.now();
      else
        lastUpdate -= 500; // twice faster
      // update lcd
      show();
    }
    // update beep
    if(states[ERROR] == ERROR_CLOCK || 
        (settings.silentMorning <= clock.hour() && 
          clock.hour() < settings.silentEvening)) {
      beep.update();
    }
  }

  void keepDefault() {
    // less 30 sec after touch
    if(lastTouch+HALF_MIN > lastUpdate || 
        (menuItem == EMERGENCE && editMode != false)) {
      return;
    }
    // return to home
    if(menuItem != HOME) {
      menuItem = HOME; 
      editMode = false;
    }
    // 5 min after touch
    if(lcd.isBacklight() && lastTouch+(5*ONE_MIN) < lastUpdate) {
      // switch off backlight
      lcd.setBacklight(false);
    }
  }

  void show() {
    // check button click
    if(nextItem != false) {
      lastTouch = millis();
      beep.play(ONE_BEEP);
      // enable backlight
      if(lcd.isBacklight() == false) {
        lcd.setBacklight(true);
        // reset click
        nextItem = false;
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
      blinkPos = false;
      editMode = false;
    }
    // error screen
    if(states[ERROR] != NO_ERROR && 
        lastTouch+HALF_MIN <= lastUpdate) {
      showAlert();
      homeScreenItem = 0;
      return;
    }
    // warning screen
    if(states[WARNING] != NO_WARNING && 
        lastTouch+HALF_MIN <= lastUpdate) {
      showWarning();
      homeScreenItem = 0;
      return;
    }
    // main screen
    if(menuItem == HOME) {
      homeScreen();
      return;
    } else {
      // reset home screen
      homeScreenItem = 0;
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

      case WATERING_PERIOD:
        fprintf_P(&lcd_out, PSTR("Watering period \n"));
        if(settings.wateringSunnyPeriod == 0) {
          fprintf_P(&lcd_out, PSTR("sun ---/{%3d} min "), 
            settings.wateringPeriod += nextItem); 
        } 
        else if(settings.wateringPeriod == 0) {
          fprintf_P(&lcd_out, PSTR("sun {%3d}/--- min "), 
            settings.wateringSunnyPeriod += nextItem); 
        } else {
            switch (editMode) {
              case true:
                editMode = 4;
              case 4:
                blinkPos = 1;
                settings.wateringSunnyPeriod += nextItem;
                break;
              case 3:
                blinkPos = 2;
                settings.wateringPeriod += nextItem;
                break;
            }  
            fprintf_P(&lcd_out, PSTR("sun {%3d}/{%3d} min "), 
              settings.wateringSunnyPeriod, settings.wateringPeriod);
        }
        break;

      case MISTING_DURATION:
        fprintf_P(&lcd_out, PSTR("Misting duration\nfor {%2d} sec      "), 
          settings.mistingDuration += nextItem);
        break;

      case MISTING_PERIOD:
        fprintf_P(&lcd_out, PSTR("Misting period  \n"));
        if(settings.mistingSunnyPeriod == 0) {
          fprintf_P(&lcd_out, PSTR("sun ---/{%3d} min "), 
            settings.mistingPeriod += nextItem); 
        } 
        else if(settings.mistingPeriod == 0) {
          fprintf_P(&lcd_out, PSTR("sun {%3d}/--- min "), 
            settings.mistingSunnyPeriod += nextItem); 
        } else {
            switch (editMode) {
              case true:
                editMode = 4;
              case 4:
                blinkPos = 1;
                settings.mistingSunnyPeriod += nextItem;
                break;
              case 3:
                blinkPos = 2;
                settings.mistingPeriod += nextItem;
                break;
            } 
            fprintf_P(&lcd_out, PSTR("sun {%3d}/{%3d} min "), 
              settings.mistingSunnyPeriod, settings.mistingPeriod);          
        }
        break;

      case LIGHT_DURATION:
        switch (editMode) {
          case true:
            editMode = 4;
          case 4:
            blinkPos = 1;
            settings.lightDayDuration += nextItem;
            break;
          case 3:
            blinkPos = 2;
            settings.lightMinimum += nextItem;
            break;
        }    
        fprintf_P(&lcd_out, PSTR("Light day       \n{%2d}h with {%4d}lux"),
          settings.lightDayDuration, settings.lightMinimum);
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
            (settings.lightDayStart += (nextItem*60))/60);
        }
        break;

      case HUMIDITY_RANGE:
        switch (editMode) {
          case true:
            editMode = 4;
          case 4:
            blinkPos = 1;
            settings.humidMinimum += nextItem;
            break;
          case 3:
            blinkPos = 2;
            settings.humidMaximum += nextItem;
            break;
        }   
        fprintf_P(&lcd_out, PSTR("Humidity range  \nfrom {%2d}%% to {%2d}%% "),
          settings.humidMinimum, settings.humidMaximum);
        break;

      case AIR_TEMP_RANGE:
        switch (editMode) {
          case true:
            editMode = 4;
          case 4:
            blinkPos = 1;
            settings.airTempMinimum += nextItem;
            break;
          case 3:
            blinkPos = 2;
            settings.airTempMaximum += nextItem;
            break;
        }
        fprintf_P(&lcd_out, PSTR("Air temp. range \nfrom {%2d}%c to {%2d}%c "), 
          settings.airTempMinimum, C_CELCIUM, settings.airTempMaximum, C_CELCIUM);
        break; 

      case SUBSTRATE_TEMP_MINIMUM:
        fprintf_P(&lcd_out, PSTR("Substrate temp. \nminimum {%2d}%c     "), 
          settings.subsTempMinimum += nextItem, C_CELCIUM);
        break;

      case SILENT_NIGHT:
        switch (editMode) {
          case true:
            editMode = 4;
          case 4:
            blinkPos = 1;
            settings.silentEvening += nextItem;
            if(settings.silentEvening > 23)
              settings.silentEvening = 17;
            else if(settings.silentEvening < 17)
              settings.silentEvening = 23;
            break;
          case 3:
            blinkPos = 2;
            settings.silentMorning += nextItem;
            if(settings.silentMorning > 12)
              settings.silentMorning = 5;
            else if(settings.silentMorning < 5)
              settings.silentMorning = 12;
            break;
        }
        fprintf_P(&lcd_out, PSTR("Silent night    \nfrom {%2d}h to {%2d}h "),
          settings.silentEvening, settings.silentMorning);
        break;

      case EMERGENCE:
        textBlink = true;
        switch (editMode) {
          case false:
            fprintf_P(&lcd_out, PSTR("Plant emergence \n       -> {Start?}"));
            // disable emergence mode
            if(emergenceTimer != false) {   
              // restore previous settings
              settings = backup;
              emergenceTimer = false;
            }
            break;
          case true:
            editMode = 4;
          case 4:
            fprintf_P(&lcd_out, PSTR("Plant emergence \nduration {%3d} min"),
              settings.emergenceDuration += nextItem);
            break;
          case 3:
            // disable EEPROM store
            storage.changed = false;
            // enable emergence mode
            if(emergenceTimer == false) {
              // enable timer
              emergenceTimer = millis()/ONE_MIN;  
              // save previous settings
              backup = settings;
              // change settings for push emergence mode
              settings.lightMinimum = 20000; // lux
              settings.lightDayDuration = 18; //hours
              settings.mistingSunnyPeriod = 5; // min
              settings.mistingPeriod = 5; //min
              settings.wateringSunnyPeriod = 2; // min
              settings.wateringPeriod = 2; //min
              settings.silentMorning = 0; // hour
              settings.silentEvening = 24; // hour
              settings.wateringDuration = 4; //min
              settings.mistingDuration = 5; //sec
            }
            if(millis()/ONE_MIN - emergenceTimer >= settings.emergenceDuration)
              // exit
              editMode = false;
            fprintf_P(&lcd_out, PSTR("Emergence.....  \n%3d min -> {Stop?}"),
              settings.emergenceDuration - (millis()/ONE_MIN - emergenceTimer));
            break;
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
  unsigned long emergenceTimer;
  uint8_t homeScreenItem;

  void backlightBlink(uint8_t _count) {
    for(uint8_t i=0; i<_count; i++) {
      lcd.setBacklight(false); delay(250);
      lcd.setBacklight(true); delay(250);
    }
  }

  void homeScreen() {
    textBlink = true;
    lcd.home();
    if(homeScreenItem >= 16)
      homeScreenItem = 0;
    
    if(states[MISTING] == 0 && states[WATERING] == 0)
      fprintf_P(&lcd_out, PSTR("Sleeping  "));
    else if(1 <= homeScreenItem && homeScreenItem < 7) 
      fprintf_P(&lcd_out, PSTR("%c%3d min  "), C_HEART, states[WATERING]);
    else if(9 <= homeScreenItem && homeScreenItem < 15)
      fprintf_P(&lcd_out, PSTR("%c%3d min  "), C_FLOWER, states[MISTING]);  
    else
      fprintf_P(&lcd_out, PSTR("          "));

    fprintf_P(&lcd_out, PSTR(" %02d{:}%02d\n"), clock.hour(), clock.minute());
    switch (homeScreenItem) {
      case 0:
        fprintf_P(&lcd_out, PSTR("Air: "));
        if(states[AIR_TEMP] > states[PREV_AIR_TEMP])
          fprintf_P(&lcd_out, PSTR("%c "), C_UP);
        else if(states[AIR_TEMP] < states[PREV_AIR_TEMP])
          fprintf_P(&lcd_out, PSTR("%c "), C_DOWN);
        else
          fprintf_P(&lcd_out, PSTR("%c "), C_TEMP);
        fprintf_P(&lcd_out, PSTR("%2d%c "), states[AIR_TEMP], C_CELCIUM);

        if(states[HUMIDITY] > states[PREV_HUMIDITY])
          fprintf_P(&lcd_out, PSTR("%c "), C_UP);
        else if(states[HUMIDITY] < states[PREV_HUMIDITY])
          fprintf_P(&lcd_out, PSTR("%c "), C_DOWN);
        else
          fprintf_P(&lcd_out, PSTR("%c "), C_HUMIDITY);
        fprintf_P(&lcd_out, PSTR("%2d%%"), states[HUMIDITY]);
        break;
      case 4:
        fprintf_P(&lcd_out, PSTR("Substrate: "));
        if(states[SUBSTRATE_TEMP] > states[PREV_SUBSTRATE_TEMP])
          fprintf_P(&lcd_out, PSTR("%c "), C_UP);
        else if(states[SUBSTRATE_TEMP] < states[PREV_SUBSTRATE_TEMP])
          fprintf_P(&lcd_out, PSTR("%c "), C_DOWN);
        else
          fprintf_P(&lcd_out, PSTR("%c "), C_TEMP);
        fprintf_P(&lcd_out, PSTR("%2d%c"), states[SUBSTRATE_TEMP], C_CELCIUM);
        break;
      case 8:
        fprintf_P(&lcd_out, PSTR("Light: "));
        if(states[LIGHT] > states[PREV_LIGHT])
          fprintf_P(&lcd_out, PSTR("%c "), C_UP);
        else if(states[LIGHT] < states[PREV_LIGHT])
          fprintf_P(&lcd_out, PSTR("%c "), C_DOWN);
        else
          fprintf_P(&lcd_out, PSTR("%c "), C_LAMP);
        fprintf_P(&lcd_out, PSTR("%4dlux"), states[LIGHT]);
        break;
      case 12:
        fprintf_P(&lcd_out, PSTR("Computer:  "));
        if(states[COMPUTER_TEMP] > states[PREV_COMPUTER_TEMP])
          fprintf_P(&lcd_out, PSTR("%c "), C_UP);
        else if(states[COMPUTER_TEMP] < states[PREV_COMPUTER_TEMP])
          fprintf_P(&lcd_out, PSTR("%c "), C_DOWN);
        else
          fprintf_P(&lcd_out, PSTR("%c "), C_TEMP);
        fprintf_P(&lcd_out, PSTR("%2d%c"), states[COMPUTER_TEMP], C_CELCIUM);
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
    storage.changed = false; // don't save EEPROM
    switch (editMode) {
      case true:
        editMode = 7;
      case 7:
        blinkPos = 1;
        hour += nextItem;
        if(hour > 23)
          hour = 0;
        break;
      case 6:
        blinkPos = 2;
        minute += nextItem;
        if(minute > 59)
          minute = 0;
        break;
      case 5:
        blinkPos = 3;
        day += nextItem;
        if(day < 1 || day > 31)
          day = 1;
        break;
      case 4:
        blinkPos = 4;
        month += nextItem;
        if(month < 1 || month > 12)
          month = 1;
        break;
      case 3:
        blinkPos = 5;
        year += nextItem;
        if(year < 2014 || year > 2024)
          year = 2014; 
        break;
    }
    fprintf_P(&lcd_out, PSTR("Setting time    \n{%02d}:{%02d} {%02d}-{%02d}-{%4d}"), 
      hour, minute, day, month, year);
    clock = DateTime(year, month, day, hour, minute);
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
        beep.play(ONE_BEEP);
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
      case ERROR_CLOCK:
        fprintf_P(&lcd_out, PSTR("Clock ERROR!    \n{Set up clock!}   "));
        return;
    }  
  }

};

#endif // __LCDMENU_H__
