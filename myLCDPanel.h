
#ifndef MYLCDPANEL_H
#define MYLCDPANEL_H

#include "LiquidCrystal_I2C.h"
#include "OneButton.h"
#include "timer.h"

// Declare LCD1609
// Set the pins on the I2C chip used for LCD connections: 
//                    addr, en,rw,rs,d4,d5,d6,d7,bl,blpol
LiquidCrystal_I2C lcd(0x20, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

// Declare fprintf
static int lcd_putc(char c, FILE *) {
  lcd.write(c);
  return c;
};
static FILE lcdout = {0};

// Declare menu items
#define HOME  0
#define WATTERING_DAY  1
#define WATTERING_NIGHT  2
#define WATTERING_SUNRISE  3
#define MISTING_DAY  4
#define MISTING_NIGHT  5
#define MISTING_SUNRISE  6
#define DAY_TIME  7
#define NIGHT_TIME  8
#define LIGHT_THRESHOLD  9
#define LIGHT_DAY  10
#define HUMIDITY_THRESHOLD  11
#define T_OUTSIDE_THRESHOLD  12
#define T_SUBSTRATE_THRESHOLD  13
#define CLOCK  14

// Declare buttons
OneButton leftButton(4, true); // A4 pin
OneButton rightButton(5, true); // A5 pin

// Callback function types
extern "C" {
  typedef uint8_t (*callback)(uint8_t _item);
}

// Delay manager in ms
timer_t lcd_timer(500);

// Debug info
#define DEBUG  true


class MyLCDPanel 
{
  public:
  void begin( void )
  {
    // Configure LCD1609
    // Initialize the lcd for 16 chars 2 lines and turn on backlight
    lcd.begin(16, 2);
    fdev_setup_stream (&lcdout, lcd_putc, NULL, _FDEV_SETUP_WRITE);
    lcd.autoscroll();
    _showMenu(HOME);

    // Configure buttons
    leftButton.attachClick( leftButtonClick );
    //leftButton.attachLongPressStart( leftButtonLongPress );
    //rightButton.attachClick( rightButtonClick );
    //rightButton.attachLongPressStart( leftButtonLongPress );
  }

  void update( void ) 
  {
    if( lcd_timer ) {
      if( warningItem > 0 ) {
        _warningFunc(warningItem); 
        return;
      }

      if( menuEditMode ) {
        _editMenuFunc(0);
      } else {
        _showMenuFunc(0);
      }
    }
    // update buttons
    leftButton.tick();
    rightButton.tick();
  };

  void attachShowMenu(callback _func) {
    _showMenuFunc = _func;
  };

  void attachEditMenu(callback _func) {
    _editMenuFunc = _func;
  };

  void attachWarning(callback _func) {
    _warningFunc = _func;
  };

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

  private:
    callback _showMenuFunc;
    callback _editMenuFunc;
    callback _warningFunc;    
    uint8_t menuItem;
    bool menuEditMode;
    uint8_t warningItem;
    uint8_t editCursor;
    bool blink;
};

void leftButtonClick() {
  _showMenuFunc(0);
};

#endif // __MYLCDPANEL_H__
