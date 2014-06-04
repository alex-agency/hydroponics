// Watchdog utility library based on code posted here:
// http://arduino.cc/forum/index.php/topic,12874.0.html

#ifndef WATCHDOG_H
#define WATCHDOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <avr/wdt.h>

// Declare a function which should be executed very early
// in the boot process.  This function probably shouldn't
// be called by you, the user.
void _wdt_clear(void) __attribute__((naked)) __attribute__((section(".init3")));

// Here are some utility functions for you, the user:
// Unconditional reset
void softReset();
// Reset if available memory falls below <bytes>
extern int __watchdog_bytes;
void softResetMem(int bytes);
// Reset if we go more than 8 seconds without a heartbeat
extern int __watchdog_timeout_ms;
void softResetTimeout();
// Reset using external watchdog circuit on pin <pin>
extern int __watchdog_pin;
void hardResetPin(int pin);

void heartbeat();


#ifdef  __cplusplus
}
#endif

#endif