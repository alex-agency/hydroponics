
#ifndef LCDPANEL_H
#define LCDPANEL_H

class LcdPanel 
{
public:
  last_touch;


  void update() {
    // is it come after button click?
    if(editValue != 0) {
      //melody.beep(1);
      last_touch = millis()/1000;
      if(lcd.isBacklight() == false) {
        lcd.setBacklight(true);
        return;
      }
      // action for simple Menu
      if(editMode == false) {
        // move forward to next menu
        lcdMenu(menuItem + editValue, false);
        return;
      }
      // mark settings for save
      //storage.changed = true;
    }

  lcd.home();
  switch (menuItem) {
    case HOME:
      showHomeScreen();
      break;
    case WATERING_DURATION:
      fprintf_P(&lcd_out, PSTR("Watering durat. \nfor %2d min      "), 
        settings.wateringDuration);
      if(editMode) {
        lcdTextBlink(1, 4, 5);
        settings.wateringDuration += editValue;
      }
      break;
    case WATERING_SUNNY:
      lcdMenuPeriod(PSTR("Watering sunny  "), &settings.wateringSunnyPeriod);
      break;
    case WATERING_NIGHT:
      lcdMenuPeriod(PSTR("Watering night  "), &settings.wateringNightPeriod);
      break;
    case WATERING_OTHER:
      lcdMenuPeriod(PSTR("Watering evening"), &settings.wateringOtherPeriod);
      break;
    case MISTING_DURATION:
      fprintf_P(&lcd_out, PSTR("Misting duration\nfor %2d sec      "), 
        settings.mistingDuration);
      if(editMode) {
        lcdTextBlink(1, 4, 5);
        settings.mistingDuration += editValue;
      }
      break;
    case MISTING_SUNNY:
      lcdMenuPeriod(PSTR("Misting sunny   "), &settings.mistingSunnyPeriod);
      break;
    case MISTING_NIGHT:
      lcdMenuPeriod(PSTR("Misting night   "), &settings.mistingNightPeriod);
      break;
    case MISTING_OTHER:
      lcdMenuPeriod(PSTR("Misting evening "), &settings.mistingOtherPeriod);
      break;
    case LIGHT_MINIMUM:
      fprintf_P(&lcd_out, PSTR("Light not less  \nthan %4d lux   "), 
        settings.lightMinimum);
      if(editMode) {
        lcdTextBlink(1, 5, 8);
        settings.lightMinimum += editValue;
      }
      break;
    case LIGHT_DAY_DURATION:
      fprintf_P(&lcd_out, PSTR("Light day       \nduration %2dh    "), 
        settings.lightDayDuration);
      if(editMode) {
        lcdTextBlink(1, 9, 10);
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
        lcdTextBlink(1, 13, 14);
        settings.lightDayStart += editValue;
      }
      break;
    case HUMIDITY_MINIMUM:
      fprintf_P(&lcd_out, PSTR("Humidity not    \nless than %2d%%   "), 
        settings.humidMinimum);
      if(editMode) {
        lcdTextBlink(1, 10, 11);
        settings.humidMinimum += editValue;
      }
      break;
    case HUMIDITY_MAXIMUM:
      fprintf_P(&lcd_out, PSTR("Humidity not    \ngreater than %2d%%"), 
        settings.humidMaximum);
      if(editMode) {
        lcdTextBlink(1, 13, 14);
        settings.humidMaximum += editValue;
      }
      break;
    case AIR_TEMP_MINIMUM:
      fprintf_P(&lcd_out, PSTR("Temp. air not   \nless than %2d%c   "), 
        settings.airTempMinimum, char_celcium);
      if(editMode) {
        lcdTextBlink(1, 10, 11);
        settings.airTempMinimum += editValue;
      }
      break;
    case AIR_TEMP_MAXIMUM:
      fprintf_P(&lcd_out, PSTR("Temp. air not   \ngreater than %2d%c"), 
        settings.airTempMaximum, char_celcium);
      if(editMode) {
        lcdTextBlink(1, 13, 14);
        settings.airTempMaximum += editValue;
      }
      break;
    case SUBSTRATE_TEMP_MINIMUM:
      fprintf_P(&lcd_out, PSTR("Substrate temp. \nless than %2d%c   "), 
        settings.subsTempMinimum, char_celcium);
      if(editMode) {
        lcdTextBlink(1, 10, 11);
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
      lcdTextBlink(1, 10, 15);
      if(editValue != 0) {
        editMode = false;
        storage.changed = false;
        doTest(false);
      }
      break;
    case 255: 
      menuItem = CLOCK;
    case CLOCK:
      if(editMode == false) {
        fprintf_P(&lcd_out, PSTR("Time:   %02d:%02d:%02d\nDate: %02d-%02d-%4d"), 
          clock.hour(), clock.minute(), clock.second(), 
          clock.day(), clock.month(), clock.year());
      } else 
      if(editValue != 0) {
        settingClock(editValue);
        // clock not used storage
        storage.changed = false;
      } 
      else {
        fprintf_P(&lcd_out, PSTR("Setting time    \n%02d:%02d %02d-%02d-%4d"), 
          clock.hour(), clock.minute(), clock.day(), clock.month(), clock.year());
        if(editModeCursor == 4) {
          lcdTextBlink(1, 0, 1);
        } else 
        if(editModeCursor == 3) {
          lcdTextBlink(1, 3, 4);
        } else 
        if(editModeCursor == 2) {
          lcdTextBlink(1, 6, 7);
        } else 
        if(editModeCursor == 1) {
          lcdTextBlink(1, 9, 10);
        } else {
          lcdTextBlink(1, 12, 15);       
        }
      }
      return 4;
    default:
      menuItem = HOME;
      menuHomeItem = 0;
      break;
  }
  editValue = 0;
  return 0;


  }



private:
  int editValue = 0;

  void leftButtonClick() {
    editValue = -1;
  }

  void rightButtonClick() {
    editValue = 1;
  }

} menu;

#endif // __LCDPANEL_H__
