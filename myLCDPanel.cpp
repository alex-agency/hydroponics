
#include "mylcdpanel.h"
#include "timer.h"

// Delay manager in ms
timer_t lcd_timer(500);

// Debug info
#define DEBUG 		true

/****************************************************************************/

MyLCDPanel::MyLCDPanel( uint8_t _left_pin, uint8_t _right_pin ) {
  leftButton(_left_pin, true);
  rightButton(_right_pin, true);
}

/****************************************************************************/

void begin( void )
{
  // Configure LCD1609
  // Initialize the lcd for 16 chars 2 lines and turn on backlight
  lcd.begin(16, 2);
  fdev_setup_stream (&lcdout, lcd_putc, NULL, _FDEV_SETUP_WRITE);
  lcd.autoscroll();
  showMenu();

  // Configure buttons
  leftButton.attachClick( leftButtonClick );
  leftButton.attachLongPressStart( leftButtonLongPress );
  rightButton.attachClick( rightButtonClick );
  rightButton.attachLongPressStart( leftButtonLongPress );
}

/****************************************************************************/

void update( void ) 
{
  if( lcd_timer ) {
    if(states[WARNING] != NO_WARNING) {
      showWarning();	
      return;
    }

    if( menuEditMode ) {
      editMenu();
    } else {
      showMenu();
    }
  }

  leftButton.tick();
  rightButton.tick();
};

/****************************************************************************/

void showMenu() {
  if(DEBUG) printf_P(PSTR("LCD1609: Info: Show menu screen #%d.\n\r", menuItem);
  lcd.clear();
  switch (menuItem) {
    case HOME:
      fprintf(&lcdout, "Hydroponic %d:%d", RTC.hour, RTC.minute); 
      lcd.setCursor(0,1);
      fprintf(&lcdout, "tIn %dC, tOut %dC, Hum. %d%, Light %d lux", 
      states[T_INSIDE], states[T_OUTSIDE], states[HUMIDITY], states[LIGHT]);
      doBlink(1, 13, 13);
      return;
    case WATTERING_DAY:
      fprintf(&lcdout, "Watering daytime"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.wateringDayPeriod);
	  return;
	case WATTERING_NIGHT:
      fprintf(&lcdout, "Watering night"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.wateringNightPeriod);
	  return;
	case WATTERING_SUNRISE:
      fprintf(&lcdout, "Watering sunrise"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.wateringSunrisePeriod);
	  return;
	case MISTING_DAY:
      fprintf(&lcdout, "Misting daytime"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.mistingDayPeriod);
	  return;
	case MISTING_NIGHT:
      fprintf(&lcdout, "Misting night"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.mistingNightPeriod);
	  return;
	case MISTING_SUNRISE:
      fprintf(&lcdout, "Misting sunrise"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.mistingSunrisePeriod);
	  return;
	case DAY_TIME:
      fprintf(&lcdout, "Day time"); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %dh to %dh", 
        settings.daytimeFrom, settings.daytimeTo);
	  return;
	case NIGHT_TIME:
      fprintf(&lcdout, "Night time"); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %dh to %dh", 
        settings.nighttimeFrom, settings.nighttimeTo);
	  return;
	case LIGHT_THRESHOLD:
      fprintf(&lcdout, "Light on when"); lcd.setCursor(0,1);
      fprintf(&lcdout, "lower %d lux", settings.lightThreshold);
	  return;
	case LIGHT_DAY:
      fprintf(&lcdout, "Light day"); lcd.setCursor(0,1);
      fprintf(&lcdout, "duration %dh", settings.lightDayDuration);
	  return;
	case HUMIDITY_THRESHOLD:
      fprintf(&lcdout, "Humidity not"); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %d%", settings.humidThreshold);
	  return;
	case T_OUTSIDE_THRESHOLD:
      fprintf(&lcdout, "Temperature not"); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %dC", settings.tempThreshold);
	  return;
	case T_SUBSTRATE_THRESHOLD:
      fprintf(&lcdout, "Temperature not"); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %dC", settings.tempSubsThreshold);
	  return;
	case CLOCK:
  	  fprintf(&lcdout, "Current time"); lcd.setCursor(0,1);
      fprintf(&lcdout, "%d:%d %d-%d-%d", RTC.hour, RTC.minute, 
      	RTC.day, RTC.month, RTC.year);
	  return;	  
    default: 
      menuItem = 0; 
      showMenu();
      return;
  }
};

/****************************************************************************/

uint8_t editMenu() {
  if(DEBUG) printf_P(PSTR("LCD1609: Info: Show edit screen #%d with cursor: %d.\n\r", 
    menuItem, editCursor);
  lcd.clear();
  switch (menuItem) {
    case WATTERING_DAY:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.wateringDayPeriod);
      doBlink(1, 6, 8);
	  return 0;
	case WATTERING_NIGHT:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.wateringNightPeriod);
      doBlink(1, 6, 8);
	  return 0;
	case WATTERING_SUNRISE:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.wateringSunrisePeriod);
      doBlink(1, 6, 8);
	  return 0;
	case MISTING_DAY:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.mistingDayPeriod);
      doBlink(1, 6, 8);
	  return 0;
	case MISTING_NIGHT:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.mistingNightPeriod);
      doBlink(1, 6, 8);
	  return 0;
	case MISTING_SUNRISE:
      fprintf(&lcdout, "Changing period"); lcd.setCursor(0,1);
      fprintf(&lcdout, "every %d min", settings.mistingSunrisePeriod);
      doBlink(1, 6, 8);
	  return 0;
	case DAY_TIME:
      fprintf(&lcdout, "Changing range"); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %dh to %dh", 
        settings.daytimeFrom, settings.daytimeTo);
      if(editCursor == 1) {
      	doBlink(1, 12, 14);
      	return 0;	
      }	
      doBlink(1, 5, 7);
	  return 1;
	case NIGHT_TIME:
      fprintf(&lcdout, "Changing range"); lcd.setCursor(0,1);
      fprintf(&lcdout, "from %dh to %dh", 
        settings.nighttimeFrom, settings.nighttimeTo);
      if(lcdEditCursor == 1) {
      	doBlink(1, 12, 14);
      	return 0;	
      }	
      doBlink(1, 5, 7);
	  return 1;
	case LIGHT_THRESHOLD:
      fprintf(&lcdout, "Changing light"); lcd.setCursor(0,1);
      fprintf(&lcdout, "lower %d lux", settings.lightThreshold);
      doBlink(1, 6, 11);
	  return 0;
	case LIGHT_DAY:
      fprintf(&lcdout, "Changing light"); lcd.setCursor(0,1);
      fprintf(&lcdout, "duration %dh", settings.lightDayDuration);
      doBlink(1, 10, 13);
	  return 0;
	case HUMIDITY_THRESHOLD:
      fprintf(&lcdout, "Changing humid."); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %d%", settings.humidThreshold);
      doBlink(1, 10, 13);
      return 0;
	case T_OUTSIDE_THRESHOLD:
      fprintf(&lcdout, "Changing temp."); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %dC", settings.tempThreshold);
      doBlink(1, 10, 13);
	  return 0;
	case T_SUBSTRATE_THRESHOLD:
      fprintf(&lcdout, "Changing temp."); lcd.setCursor(0,1);
      fprintf(&lcdout, "less than %dC", settings.tempSubsThreshold);
      doBlink(1, 10, 13);
	  return 0;	  
	case CLOCK:
    case HOME:
  	  fprintf(&lcdout, "Setting time"); lcd.setCursor(0,1);
      fprintf(&lcdout, "%d:%d %d-%d-%d", RTC.hour, RTC.minute, 
      	RTC.day, RTC.month, RTC.year);
	  if(editCursor == 4) {
	  	doBlink(1, 12, 13);
	  	return 0;
	  } else if(editCursor == 3) {
		doBlink(1, 9, 10);
		return 1;
	  } else if(editCursor == 2) {
	  	doBlink(1, 6, 7);
	  	return 2;
	  } else if(editCursor == 1) {
	  	doBlink(1, 3, 4);
	  	return 3;
	  }
	  doBlink(1, 0, 1);
	  return 4;
    default:
      menuItem = 0; showMenu();
      return 0;
  }
};

/****************************************************************************/

void showWarning() {
  switch (states[WARNING]) {
    case WARNING_NO_WATER:
      fprintf(&lcdout, "No water! :("); lcd.setCursor(0,1);
      fprintf(&lcdout, "Can’t misting!");
      doBlink(1, 0, 13);
      return;
    case WARNING_SUBSTRATE_FULL:
      fprintf(&lcdout, "Substrate tank"); lcd.setCursor(0,1);
      fprintf(&lcdout, "is full! :)");
      return;
    case WARNING_SUBSTRATE_LOW:
      fprintf(&lcdout, "Low substrate!"); lcd.setCursor(0,1);
      fprintf(&lcdout, ":( Please add.");
      return;
    case WARNING_NO_SUBSTRATE:
      fprintf(&lcdout, "Can't watering!"); lcd.setCursor(0,1);
      fprintf(&lcdout, "Plants could die");
      doBlink(0, 0, 14);
      return;
    case WARNING_DONE:
      fprintf(&lcdout, "Watering done!"); lcd.setCursor(0,1);
      fprintf(&lcdout, ":) wait... 10m");
      return;
    case WARNING_WATERING:
      fprintf(&lcdout, "Watering..."); lcd.setCursor(0,1);
      fprintf(&lcdout, "Please wait.");
      return;
    case WARNING_MISTING:
      fprintf(&lcdout, "Misting..."); lcd.setCursor(0,1);
      fprintf(&lcdout, "Please wait.");
      return;
    case WARNING_TEMPERATURE_COLD:
      fprintf(&lcdout, "Is too cold"); lcd.setCursor(0,1);
      fprintf(&lcdout, "for plants! :(");
      doBlink(0, 0, 10);
      return;
    case WARNING_SUBSTRATE_COLD:
      fprintf(&lcdout, "Substrate is"); lcd.setCursor(0,1);
      fprintf(&lcdout, "too cold! :O");
      doBlink(1, 10, 11);
      return;
  }  
};

/****************************************************************************/

bool doBlink(uint8_t _row, uint8_t _start, uint8_t _end) { 
  if(blink) {
    if(DEBUG) printf_P(PSTR("LCD1609: Info: Blink for: %d\n\r", _end-_start+1); 
    
    while(_start <= _end) {
      lcd.setCursor(_row, _start); lcd.print(" ");
      _start++;
    }
    blink = false;
    return;
  }
  blink = true;
};

/****************************************************************************/

void leftButtonClick() {
  if(DEBUG) printf_P(PSTR("Button LEFT: Info: Click event.\n\r");
  
  if(menuEditMode == false) {
  	menuItem--; // move backward, previous menu
    showMenu();
    return;  	
  }

  settingsChanged = true;
  switch (menuItem) {
    case WATTERING_DAY:
      settings.wateringDayPeriod--;
  	  break;
  	case WATTERING_NIGHT:
      settings.wateringNightPeriod--;
      break;
  	case WATTERING_SUNRISE:
      settings.wateringSunrisePeriod--;
      break;
  	case MISTING_DAY:
      settings.mistingDayPeriod--;
      break;
  	case MISTING_NIGHT:
      settings.mistingNightPeriod--;
      break;
  	case MISTING_SUNRISE:
      settings.mistingSunrisePeriod--;
      break;
  	case DAY_TIME:
      if(editCursor == 1) {
        settings.daytimeFrom--;
        editCursor--;
      } else {
        settings.daytimeTo--;
      }
      break;
  	case NIGHT_TIME:
      if(editCursor == 1) {
        settings.nighttimeFrom--;
        lcdEditCursor--;
      } else {
        settings.nighttimeTo--;
      }
      break;
  	case LIGHT_THRESHOLD:
      settings.lightThreshold--;
      break;
  	case LIGHT_DAY:
      settings.lightDayDuration--;
  	  break;
  	case HUMIDITY_THRESHOLD:
      settings.humidThreshold--;
  	  break;
  	case T_OUTSIDE_THRESHOLD:
      settings.tempThreshold--;
  	  break;
  	case T_SUBSTRATE_THRESHOLD:
      settings.tempSubsThreshold--;
  	  break;
  	case CLOCK:
      RTC.stopClock();
      if(editCursor == 4) {
        if(RTC.hour > 0)
          RTC.hour--;
        editCursor--;
      } else
      if(editCursor == 3) {
        if(RTC.minute > 0)
          RTC.minute--;
        editCursor--;
      } else
      if(editCursor == 2) {
        if(RTC.day > 1)
          RTC.day--;
        editCursor--;
      } else
      if(editCursor == 1) {
        if(RTC.month > 1)
          RTC.month--;
        editCursor--;
      } else {
        RTC.year--;
      }
      settingsChanged = false;
  	  break;
  }
  editMenu();
};

/****************************************************************************/

void rightButtonClick() {
  if(DEBUG) printf_P(PSTR("Button RIGHT: Info: Click event.\n\r");

  if(menuEditMode == false) {
  	menuItem++; // move forward, next menu
    showMenu();
    return;  	
  }

  settingsChanged = true;
  switch (menuItem) {
    case WATTERING_DAY:
      settings.wateringDayPeriod++;
      break;
    case WATTERING_NIGHT:
      settings.wateringNightPeriod++;
      break;
    case WATTERING_SUNRISE:
      settings.wateringSunrisePeriod++;
      break;
    case MISTING_DAY:
      settings.mistingDayPeriod++;
      break;
    case MISTING_NIGHT:
      settings.mistingNightPeriod++;
      break;
    case MISTING_SUNRISE:
      settings.mistingSunrisePeriod++;
      break;
    case DAY_TIME:
      if(editCursor == 1) {
        settings.daytimeFrom++;
        editCursor--;
      } else {
        settings.daytimeTo++;
      }
      break;
    case NIGHT_TIME:
      if(editCursor == 1) {
        settings.nighttimeFrom++;
        editCursor--;
      } else {
        settings.nighttimeTo++;
      }
      break;
    case LIGHT_THRESHOLD:
      settings.lightThreshold++;
      break;
    case LIGHT_DAY:
      settings.lightDayDuration++;
      break;
    case HUMIDITY_THRESHOLD:
      settings.humidThreshold++;
      break;
    case T_OUTSIDE_THRESHOLD:
      settings.tempThreshold++;
      break;
    case T_SUBSTRATE_THRESHOLD:
      settings.tempSubsThreshold++;
      break;
    case CLOCK:
      RTC.stopClock();
      if(editCursor == 4) {
        if(RTC.hour < 23)
          RTC.hour++;
        editCursor--;
      } else
      if(editCursor == 3) {
        if(RTC.minute < 59)
          RTC.minute++;
        editCursor--;
      } else
      if(editCursor == 2) {
        if(RTC.day < 31)
          RTC.day++;
        leditCursor--;
      } else
      if(editCursor == 1) {
        if(RTC.month < 12)
          RTC.month++;
        editCursor--;
      } else {
        RTC.year++;
      }
      settingsChanged = false; // nothing save to eeprom
      break;
  }
  editMenu();
};

/****************************************************************************/

void leftButtonLongPress() {
  if(DEBUG) printf_P(PSTR("Button LEFT: Info: LongPress event.\n\r");

  if(menuEditMode == false) {
  	menuEditMode = true;
    editCursor = editMenu();
    return;
  }

  if(editCursor != 0) {
  	editCursor--; // move to next edit field
  	editCursor = editMenu();
  	return;
  }

  menuEditMode = false;
  if(menuItem == CLOCK) {
    RTC.setTime();
    RTC.startClock();
  }
  showMenu();	
  return;
};

/****************************************************************************/
};
