
#ifndef MYLCDPANEL_H
#define MYLCDPANEL_H

#include "LiquidCrystal_I2C.h"
#include "OneButton.h"

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
uint8_t const HOME = 0;
uint8_t const WATTERING_DAY = 1;
uint8_t const WATTERING_NIGHT = 2;
uint8_t const WATTERING_SUNRISE = 3;
uint8_t const MISTING_DAY = 4;
uint8_t const MISTING_NIGHT = 5;
uint8_t const MISTING_SUNRISE = 6;
uint8_t const DAY_TIME = 7;
uint8_t const NIGHT_TIME = 8;
uint8_t const LIGHT_THRESHOLD = 9;
uint8_t const LIGHT_DAY = 10;
uint8_t const HUMIDITY_THRESHOLD = 11;
uint8_t const T_OUTSIDE_THRESHOLD = 12;
uint8_t const T_SUBSTRATE_THRESHOLD = 13;
uint8_t const CLOCK = 14;

class MyLCDPanel 
{
public:
  OneButton leftButton;
  OneButton rightButton;

  MyLCDPanel( uint8_t _left_pin, uint8_t _right_pin );

  void begin( void );

  void update( void );

  void showMenu( void );

  uint8_t editMenu( void );

  void showWarning( void );
  
  void leftButtonClick( void );

  void rightButtonClick( void );

  void leftButtonLongPress( void );

private:
  uint8_t left_pin; 
  uint8_t right_pin;
  uint8_t menuItem = HOME;
  bool menuEditMode = false;
  uint8_t editCursor = 0;
  bool blink = false;

  bool doBlink(uint8_t _row, uint8_t _start, uint8_t _end);
};

#endif // __MYLCDPANEL_H__
