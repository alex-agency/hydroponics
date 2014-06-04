#include "Watchdog.h"

#if (ARDUINO >= 100)
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

#include "MemoryFree.h"
#include <avr/wdt.h>

// Remove any configured watchdog so we can complete the boot process without
// getting reset.
void _wdt_clear(void) {
  MCUSR = 0;
  wdt_disable();
}

void softReset() {
  wdt_enable(WDTO_15MS);
  for (;;) {}
}

int __watchdog_bytes = -1;
void softResetMem(int bytes) {
  __watchdog_bytes = bytes;
}

int __watchdog_timeout_ms = -1;
void softResetTimeout() {
  // TODO add argument for configurable timeout length, and handle appropriately.
  __watchdog_timeout_ms = 8000;
  wdt_enable(WDTO_8S);
}

int __watchdog_pin = -1;
void hardResetPin(int pin) {
  __watchdog_pin = pin;
}

void heartbeat() {
  if (__watchdog_timeout_ms >= 0) {
    wdt_reset();
  }
  if (__watchdog_bytes >= 0) {
    int bytesFree = freeMemory();
    if (bytesFree < __watchdog_bytes) {
      softReset();
    }
  }
  if (__watchdog_pin >= 0) {
    pinMode(__watchdog_pin, OUTPUT);
    digitalWrite(__watchdog_pin, LOW);
    delay(300);
    // Return to high-Z
    pinMode(__watchdog_pin, INPUT);
  }
}