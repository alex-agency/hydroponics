
#ifndef LCDPANEL_H
#define LCDPANEL_H

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

// Declare buttons
OneButton leftButton(4, true); // A4 pin
OneButton rightButton(5, true); // A5 pin

// Delay manager in ms
timer_t lcd_timer(500);

// Debug info
#define DEBUG  true


class LCDPanel 
{
  public:
    typedef uint8_t (*callback)(uint8_t _item);

    //LCDPanel( uint8_t _leftPin, uint8_t _rightPin ): {}

    void begin( void )
    {
      // Configure LCD1609
      // Initialize the lcd for 16 chars 2 lines and turn on backlight
      lcd.begin(16, 2);
      fdev_setup_stream (&lcdout, lcd_putc, NULL, _FDEV_SETUP_WRITE);
      lcd.autoscroll();
      _showMenuFunc(menuItem);

      // Configure buttons
      leftButton.attachClick( leftClick );
      leftButton.attachLongPressStart( longPress );
      rightButton.attachClick( rightClick );
      rightButton.attachLongPressStart( longPress );
    }

    void update( void ) 
    {
      if( lcd_timer ) {
        if( warningItem > 0 ) {
          _warningFunc(warningItem); 
          return;
        }

        if( menuEditMode ) {
          _editMenuFunc(menuItem);
        } else {
          _showMenuFunc(menuItem);
        }
      }
      // update buttons
      leftButton.tick();
      rightButton.tick();
    }

    void attachShowMenu(callback _func) {
      _showMenuFunc = _func;
    }

    void attachEditMenu(callback _func) {
      _editMenuFunc = _func;
    }

    void attachWarning(callback _func) {
      _warningFunc = _func;
    }
    
    void attachLeftClick(callbackFunction _func) {
      _leftClickFunc = _func;
    };

    void attachRightClick(callbackFunction _func) {
      _rightClickFunc = _func;
    };

    void attachLongPress(callbackFunction _func) {
      _longPressFunc = _func;
    };

    bool doBlink(uint8_t _row, uint8_t _start, uint8_t _end) { 
      if(blink) {
        //if(DEBUG) printf_P(PSTR("LCD1609: Info: Blink for: %d\n\r", _end-_start+1); 
        
        while(_start <= _end) {
          lcd.setCursor(_row, _start); lcd.print(" ");
          _start++;
        }
        blink = false;
      }
      blink = true; 
      return blink;
    };

  private:
    callback _showMenuFunc;
    callback _editMenuFunc;
    callback _warningFunc;
    callbackFunction _leftClickFunc;
    callbackFunction _rightClickFunc;
    callbackFunction _longPressFunc;
    static LCDPanel panel;
    uint8_t menuItem;
    bool menuEditMode;
    uint8_t warningItem;
    uint8_t editCursor;
    bool blink;

    static void leftClick() {
      
      if(panel._leftClickFunc) panel._leftClickFunc();
    }

    static void rightClick() {

      if(panel._rightClickFunc) panel._rightClickFunc();
    }

    static void longPress() {

      if(panel._longPressFunc) panel._longPressFunc();
    }
};

#endif // __LCDPANEL_H__
