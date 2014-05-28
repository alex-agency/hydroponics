
#ifndef LCDPANEL_H
#define LCDPANEL_H

#include "LiquidCrystal_I2C.h"
#include "OneButton.h"
#include "timer.h"

// Debug info
#define DEBUG  true

// Declare buttons
OneButton leftButton(A4, true); // A4 pin
OneButton rightButton(A5, true); // A5 pin

// Declare LCD1609
// Set the pins on the I2C chip used for LCD connections: 
//                    addr, en,rw,rs,d4,d5,d6,d7,bl,blpol
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

// Declare fprintf
static int lcd_putc(char c, FILE *) {
  lcd.write(c);
  return c;
};
static FILE lcdout = {0};

// Delay manager in ms
timer_t lcd_timer(2000);


class LCDPanel 
{
  public:
    typedef uint8_t (*callbackOneParam)(uint8_t _a);
    typedef uint8_t (*callbackTwoParams)(uint8_t _a, uint8_t _b);

    static void begin( void )
    {
      // Configure LCD1609
      // Initialize the lcd for 16 chars 2 lines and turn on backlight
      lcd.begin(16, 2);
      fdev_setup_stream (&lcdout, lcd_putc, NULL, _FDEV_SETUP_WRITE);

      // Configure buttons
      leftButton.attachClick( leftClick );
      leftButton.attachLongPressStart( longPress );
      rightButton.attachClick( rightClick );
      rightButton.attachLongPressStart( longPress );
    }

    static void update( void ) 
    {
      if( lcd_timer ) {
        if( menuEditMode ) {
          editMenu(menuItem, editCursor);
        } else {
          showMenu(menuItem);
        }
      }
      // update buttons
      leftButton.tick();
      rightButton.tick();
    }

    static void attachShowMenu(callbackOneParam _func) {
      showMenuCallback = _func;
    }

    static void attachEditMenu(callbackTwoParams _func) {
      editMenuCallback = _func;
    }
    
    static void attachLeftClick(callbackTwoParams _func) {
      leftClickCallback = _func;
    }

    static void attachRightClick(callbackTwoParams _func) {
      rightClickCallback = _func;
    }

    static void attachLongPress(callbackOneParam _func) {
      longPressCallback = _func;
    }

    static void showMenu(uint8_t _menuItem) {    
      if(DEBUG) printf_P(PSTR("LCD: Info: Show Menu #%d.\n\r"), _menuItem);

      menuItem = _menuItem;
      // callback
      if(showMenuCallback) showMenuCallback(menuItem);
    }

    static void editMenu(uint8_t _menuItem, uint8_t _editCursor) {    
      if(DEBUG) 
        printf_P(PSTR("LCD: Info: Edit menu: Menu #%d, Cursor #%d.\n\r"), 
          _menuItem, _editCursor);
      
      menuEditMode = true;
      menuItem = _menuItem;
      editCursor = _editCursor;
      // callback
      if(editMenuCallback) editCursor = editMenuCallback(menuItem, editCursor);
    }

    static bool doBlink(uint8_t _row, uint8_t _start, uint8_t _end) { 
      if(blink) {
        if(DEBUG) printf_P(PSTR("LCD: Info: Blink for: %d:%d-%d\n\r"),
          _row, _start, _end); 
        
        while(_start <= _end) {
          lcd.setCursor(_start, _row); lcd.print(" ");
          _start++;
        }
        blink = false;
        return blink;
      }
      blink = true; 
      return blink;
    }

  private:
    static callbackOneParam showMenuCallback;
    static callbackTwoParams editMenuCallback;
    static callbackTwoParams leftClickCallback;
    static callbackTwoParams rightClickCallback;
    static callbackOneParam longPressCallback;
    static uint8_t menuItem;
    static bool menuEditMode;
    static uint8_t editCursor;
    static bool blink;

    static void leftClick() {
      if(DEBUG) printf_P(PSTR("LCD LEFT button: Info: Click event.\n\r"));
      
      if(menuEditMode == false) {
        // move backward, previous menu
        showMenu(--menuItem);
        return;   
      }
      // callback
      if(leftClickCallback) menuItem = leftClickCallback(menuItem, editCursor);
      editMenu(menuItem, editCursor);
    }

    static void rightClick() {
      if(DEBUG) printf_P(PSTR("LCD RIGHT button: Info: Click event.\n\r"));

      if(menuEditMode == false) {
        // move forward, next menu
        showMenu(++menuItem);
        return;   
      }
      // callback
      if(rightClickCallback) menuItem = rightClickCallback(menuItem, editCursor);
      editMenu(menuItem, editCursor);
    }

    static void longPress() {
      if(DEBUG) printf_P(PSTR("LCD buttons: Info: LongPress event.\n\r"));

      if(menuEditMode == false) {
        menuEditMode = true;
        editMenu(menuItem, editCursor);
      }

      if(editCursor != 0) {
        // move to next edit field
        editMenu(menuItem, --editCursor);
      } 
      else {
        menuEditMode = false;
      }
      // callback
      if(longPressCallback) menuItem = longPressCallback(menuItem);
      showMenu(menuItem);
    }
};

#endif // __LCDPANEL_H__
