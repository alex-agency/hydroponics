
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

// Delay manager in ms
timer_t lcd_timer(500);

// Debug info
#define DEBUG  true


class LCDPanel 
{
  public:
    typedef uint8_t (*callback)(uint8_t _item);

    LCDPanel( uint8_t _leftButtonPin, uint8_t _rightButtonPin ): 
      leftButton(_leftButtonPin, true), rightButton(_rightButtonPin, true) {}

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
    
    void attachLeftClick(callback _func) {
      _leftClickFunc = _func;
    }

    void attachRightClick(callback _func) {
      _rightClickFunc = _func;
    }

    void attachLongPress(callback _func) {
      _longPressFunc = _func;
    }

    void showMenu(uint8_t _menuItem) {    
      if(DEBUG) printf_P(PSTR("LCD: Info: Show Menu #%d.\n\r"), _menuItem);

      menuItem = _menuItem;
      lcd.clear();

      // callback
      if(panel._showMenuFunc) 
        panel._showMenuFunc(menuItem);
    }

    void editMenu(uint8_t _menuItem, uint8_t _editCursor) {    
      if(DEBUG) 
        printf_P(PSTR("LCD: Info: Edit menu: Menu #%d, Cursor #%d.\n\r"), 
          _menuItem, _editCursor);

      menuItem = _menuItem;
      editCursor = _editCursor;
      lcd.clear();

      // callback
      if(panel._editMenuFunc) 
        editCursor = panel._editMenuFunc(menuItem, editCursor);
    }

    bool doBlink(uint8_t _row, uint8_t _start, uint8_t _end) { 
      if(blink) {
        if(DEBUG) printf_P(PSTR("LCD: Info: Blink for: %d\n\r"), _end-_start+1); 
        
        while(_start <= _end) {
          lcd.setCursor(_row, _start); lcd.print(" ");
          _start++;
        }
        blink = false;
      }
      blink = true; 
      return blink;
    }

  private:
    OneButton leftButton;
    OneButton rightButton;
    callback _showMenuFunc;
    callback _editMenuFunc;
    callback _leftClickFunc;
    callback _rightClickFunc;
    callback _longPressFunc;
    static LCDPanel panel;
    uint8_t menuItem;
    bool menuEditMode;
    uint8_t editCursor;
    bool blink;

    static void leftClick() {
      if(DEBUG) printf_P(PSTR("LCD LEFT button: Info: Click event.\n\r"));
      
      if(menuEditMode == false) {
        // move backward, previous menu
        showMenu(--menuItem);
        return;   
      }
      // callback
      if(panel._leftClickFunc) 
        menuItem = panel._leftClickFunc(menuItem, editCursor);

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
      if(panel._rightClickFunc) 
        menuItem = panel._rightClickFunc(menuItem, editCursor);

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
        editCursor = editMenu(menuItem, --editCursor);
      } 
      else {
        menuEditMode = false;
      }
      // callback
      if(panel._longPressFunc) 
        menuItem = panel._longPressFunc(menuItem);
      
      showMenu(menuItem); 
    }
};

#endif // __LCDPANEL_H__
