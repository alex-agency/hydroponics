// Import libraries
#include <avr/pgmspace.h>
#include <Wire.h>
#include "LiquidCrystal_I2C.h"
//#include "MemoryFree.h"
#include "Watchdog.h"
#include "timer.h"

#define DEBUG

// Declare LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Declare output functions
static int serial_putchar(char c, FILE *) {
  Serial.write(c);
  return 0;
};
FILE serial_out = {0};
static int lcd_putchar(char c, FILE *) {
  lcd.write(c);
  return 0;
};
FILE lcd_out = {0};

// Declare delay managers
timer_t fast_timer(1000);
timer_t slow_timer(30000);

// Declare variables


/****************************************************************************/

//
// Setup
//
void setup()
{
  softResetMem(512);

  // Configure output
  Serial.begin(9600);
  fdev_setup_stream(&serial_out, serial_putchar, NULL, _FDEV_SETUP_WRITE);
  fdev_setup_stream(&lcd_out, lcd_putchar, NULL, _FDEV_SETUP_WRITE);
  stdout = stderr = &serial_out;

  // Configure lcd
  lcd.begin();
  lcd.clear();

}

//
// Loop
//
void loop()
{
  heartbeat();

  if(fast_timer) {
    #ifdef DEBUG
      printf_P(PSTR("Free memory: %d bytes.\n\r"), freeMemory());
    #endif

    fprintf(&lcd_out, "%d", freeMemory());
  }

  if(slow_timer) {


  }

  // not so fast
  delay(50);
}

/****************************************************************************/




