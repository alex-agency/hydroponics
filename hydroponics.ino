// Import libraries
#include <Wire.h>
#include <SPI.h>
#include "LcdPanel.h"
#include "MemoryFree.h"
#include "Watchdog.h"
#include "DHT.h"
#include "DS18B20.h"
#include "BH1750.h"
#include "LowPower.h"
//#define MESH
#ifdef MESH
  #include "nRF24L01.h"
  #include "RF24.h"
  #include "RF24Layer2.h"
  #include "MeshNet.h"
#endif

//#define DEBUG

// Declare output
static int serial_putchar(char c, FILE *) {
  Serial.write(c);
  return 0;
};
FILE serial_out = {0};

// Declare Lcd Panel
LcdPanel panel;

#ifdef MESH
  // Declare MESH network
  const uint32_t deviceType = 001;
  uint32_t deviceUniqueId = 100002;
  static const uint8_t CE_PIN = 9;
  static const uint8_t CS_PIN = 10;
  const uint8_t RF24_INTERFACE = 0;
  const int NUM_INTERFACES = 1;
  // Declare radio
  RF24 radio(CE_PIN, CS_PIN);
#endif

// Declare DHT sensor
#define DHTTYPE DHT11

// Declare variables
unsigned long timerFast, timerSlow, lastMisting, lastWatering;
uint16_t sunrise;
uint8_t startMisting;
unsigned long startWatering;
bool substTankFull;

// Define pins
static const uint8_t DHTPIN = 3;
static const uint8_t ONE_WIRE_BUS = 2;
static const uint8_t SUBSTRATE_FULLPIN = A0;
static const uint8_t SUBSTRATE_DELIVEREDPIN = A1;
static const uint8_t WATER_LEVELPIN = A6;
static const uint8_t SUBSTRATE_LEVELPIN = A7;
static const uint8_t PUMP_WATERINGPIN = 4;
static const uint8_t PUMP_MISTINGPIN = 5;
static const uint8_t LAMPPIN = 7;
// Define relay states
static const uint8_t RELAY_ON = 0;
static const uint8_t RELAY_OFF = 1;

/****************************************************************************/

//
// Setup
//
void setup()
{
  // Configure output
  Serial.begin(9600);
  fdev_setup_stream(&serial_out, serial_putchar, NULL, _FDEV_SETUP_WRITE);
  stdout = stderr = &serial_out;
  // prevent continiously restart
  delay(500); // millis
  // restart if low memory
  softResetMem(512); // bytes
  // restart after freezing for 8 sec
  softResetTimeout();
  #ifdef MESH
    // initialize network
    rf24init();
  #endif
  // initialize lcd panel
  panel.begin();
}

//
// Loop
//
void loop()
{
  // watchdog
  heartbeat();
  // timer fo 1 sec
  if(millis() - timerFast >= MILLIS_TO_SEC) {
    timerFast = millis();
    // check level sensors
    check_levels();
    // update watering
    watering();
    // update misting
    misting();
    // timer fo 1 min
    if(timerFast - timerSlow >= 60*MILLIS_TO_SEC) {
      timerSlow = timerFast;
      // system check
      check();
      // manage light
      doLight();
      // manage misting and watering
      doWork();
      // save settings
      #ifdef DEBUG_EEPROM
        printf_P(PSTR("EEPROM: Info: storage changed->%d, ok->%d.\n\r"), 
            storage.changed, storage.ok);
      #endif
      if(storage.changed && storage.ok) {
        // WARNING: EEPROM can burn!
        storage.save();
        storage.changed = false;
      }
      #ifdef MESH
        //meshTest();
        // send data to base
        sendCommand( 1, (void*) &states, sizeof(states) );
        /*sendCommand( 1, (void*) &"Hydroponics", sizeof("Hydroponics") );
        sendCommand( HUMIDITY, (void*) states[HUMIDITY], 
          sizeof(states[HUMIDITY]) );
        sendCommand( AIR_TEMP, (void*) states[AIR_TEMP], 
          sizeof(states[AIR_TEMP]) );
        sendCommand( COMPUTER_TEMP, (void*) states[COMPUTER_TEMP],
          sizeof(states[COMPUTER_TEMP]) );
        sendCommand( SUBSTRATE_TEMP, (void*) states[SUBSTRATE_TEMP], 
          sizeof(states[SUBSTRATE_TEMP]) );
        sendCommand( LIGHT, (void*) states[LIGHT], 
          sizeof(states[LIGHT]) );
        sendCommand( PUMP_MISTING, (void*) states[PUMP_MISTING], 
          sizeof(states[PUMP_MISTING]) );
        sendCommand( PUMP_WATERING, (void*) states[PUMP_WATERING], 
          sizeof(states[PUMP_WATERING]) );
        sendCommand( LAMP, (void*) states[LAMP], 
          sizeof(states[LAMP]) );
        sendCommand( WARNING, (void*) states[WARNING], 
          sizeof(states[WARNING]) );
        sendCommand( ERROR, (void*) states[ERROR], 
          sizeof(states[ERROR]) );*/
      #endif
    }
  }
  // update LCD 
  panel.update();
  #ifdef MESH
    // update network
    rf24receive();
  #endif
}

/****************************************************************************/
#ifdef MESH
  // Pass a layer3 packet to the layer2 of MESH network
  int sendPacket(uint8_t* message, uint8_t len, 
      uint8_t interface, uint8_t macAddress) {  
    // Here should be called the layer2 function corresponding to interface
    if(interface == RF24_INTERFACE) {
      #ifdef DEBUG_MESH
        printf_P(PSTR("MESH: INFO: Sending %d byte to %d\n\r"), len, macAddress);
      #endif
      rf24sendPacket(message, len, macAddress);
      return true;
    }
    return false;
  }

  void onCommandReceived(uint8_t command, void* data, uint8_t dataLen) {
    #ifdef DEBUG_MESH
      printf_P(PSTR("MESH: INFO: Received %d, %d\n\r"), command, data);
    #endif
  }
#endif
/****************************************************************************/

bool read_DHT() {
  DHT dht(DHTPIN, DHTTYPE);
  dht.begin();
  states[HUMIDITY] = dht.readHumidity();
  states[AIR_TEMP] = dht.readTemperature();

  if( isnan(states[HUMIDITY]) || isnan(states[AIR_TEMP]) ) {
    #ifdef DEBUG_DHT11
      printf_P(PSTR("DHT11: Error: Communication failed!\n\r"));
    #endif
    return false;
  }
  #ifdef DEBUG_DHT11
    printf_P(PSTR("DHT11: Info: Air humidity: %d, temperature: %dC.\n\r"), 
      states[HUMIDITY], states[AIR_TEMP]);
  #endif
  return true;
}

bool read_DS18B20() {
  DS18B20 ds(ONE_WIRE_BUS);
  
  int value = ds.read(0);
  if(value == DS_DISCONNECTED) {
    #ifdef DEBUG_DS18B20
      printf_P(PSTR("DS18B20: Error: Computer sensor communication failed!\n\r"));
    #endif
    return false;
  }
  #ifdef DEBUG_DS18B20
    printf_P(PSTR("DS18B20: Info: Computer temperature: %dC.\n\r"), value);
  #endif
  states[COMPUTER_TEMP] = value;

  value = ds.read(1);
  if(value == DS_DISCONNECTED) {
    #ifdef DEBUG_DS18B20
      printf_P(PSTR("DS18B20: Error: Substrate sensor communication failed!\n\r"));
    #endif
    return false;
  }
  #ifdef DEBUG_DS18B20
    printf_P(PSTR("DS18B20: Info: Substrate temperature: %dC.\n\r"), value);
  #endif
  states[SUBSTRATE_TEMP] = value;
  return true;
}

bool read_BH1750() {
  BH1750 lightMeter;
  lightMeter.begin(BH1750_ONE_TIME_HIGH_RES_MODE);
  uint16_t value = lightMeter.readLightLevel();

  if(value == 54612) {
    #ifdef DEBUG_BH1750
      printf_P(PSTR("BH1750: Error: Light sensor communication failed!\n\r"));
    #endif
    return false;
  }
  #ifdef DEBUG_BH1750
    printf_P(PSTR("BH1750: Info: Light intensity: %d.\n\r"), value);
  #endif
  states[LIGHT] = value;
  return true;
}

void check_levels() {
  pinMode(SUBSTRATE_DELIVEREDPIN, INPUT_PULLUP);
  #ifdef DEBUG_LEVELS
    printf_P(PSTR("LEVELS: Info: Substrate delivered: %d.\n\r"), 
      digitalRead(SUBSTRATE_DELIVEREDPIN));
  #endif
  if(digitalRead(SUBSTRATE_DELIVEREDPIN) == 1) {
    states[WARNING] = INFO_SUBSTRATE_DELIVERED;
    return;
  }
  // no pull-up for A6 and A7
  pinMode(SUBSTRATE_LEVELPIN, INPUT);
  #ifdef DEBUG_LEVELS
    printf_P(PSTR("LEVELS: Info: Substrate level: %d.\n\r"), 
      analogRead(SUBSTRATE_LEVELPIN));
  #endif
  if(analogRead(SUBSTRATE_LEVELPIN) > 700) {
    // prevent fail alert
    if(millis()/MILLIS_TO_SEC > lastWatering + 180)
      states[ERROR] = ERROR_NO_SUBSTRATE;
    else
      states[WARNING] = WARNING_SUBSTRATE_LOW;
    return; 
  }
  pinMode(SUBSTRATE_FULLPIN, INPUT_PULLUP);
  #ifdef DEBUG_LEVELS
    printf_P(PSTR("LEVELS: Info: Substrate full: %d.\n\r"), 
      digitalRead(SUBSTRATE_FULLPIN));
  #endif
  if(digitalRead(SUBSTRATE_FULLPIN) == 1) { 	  
  	if(substTankFull == false) {
      substTankFull = true;
      states[WARNING] = INFO_SUBSTRATE_FULL;
      return;
    }
  } else {
  	substTankFull = false;
  }
  // no pull-up for A6 and A7
  pinMode(WATER_LEVELPIN, INPUT);
  #ifdef DEBUG_LEVELS
    printf_P(PSTR("LEVELS: Info: Water level: %d.\n\r"), 
      analogRead(WATER_LEVELPIN));
  #endif
  if(analogRead(WATER_LEVELPIN) > 700) {
    states[WARNING] = WARNING_NO_WATER;
    return;
  }
}

void relayOn(uint8_t relay) {
  if(states[relay]) {
    // relay is already on
    return;
  }
  bool status = relays(relay, RELAY_ON);
  if(status) {
    #ifdef DEBUG_RELAY
      printf_P(PSTR("RELAY: Info: '%s' is enabled.\n\r"), relay);
    #endif
    states[relay] = true;
  }
}

void relayOff(uint8_t relay) {
  if(states[relay] == false) {
    // relay is already off
    return;
  }
  bool status = relays(relay, RELAY_OFF);
  if(status) {
    #ifdef DEBUG_RELAY
      printf_P(PSTR("RELAY: Info: '%s' is disabled.\n\r"), relay);
    #endif
    states[relay] = false;
  }
}

bool relays(uint8_t relay, uint8_t state) {
  if(relay == PUMP_MISTING) {
    pinMode(PUMP_MISTINGPIN, OUTPUT);
    digitalWrite(PUMP_MISTINGPIN, state);
    return true;
  } 
  if(relay == PUMP_WATERING) {
    pinMode(PUMP_WATERINGPIN, OUTPUT);    
    digitalWrite(PUMP_WATERINGPIN, state);
    return true;
  } 
  if(relay == LAMP) {
    pinMode(LAMPPIN, OUTPUT);
    digitalWrite(LAMPPIN, state);
    return true;
  }
  #ifdef DEBUG_RELAY
    printf_P(PSTR("RELAY: Error: '%s' is unknown!\n\r"), relay);
  #endif
  return false;
}

void check() {
  #ifdef DEBUG
    printf_P(PSTR("Free memory: %d bytes.\n\r"), freeMemory());
  #endif
  // check if memory less than 600 bytes
  if(freeMemory() < 600) {
    states[ERROR] = ERROR_LOW_MEMORY;
    return;
  }
  // prevent burn system
  if(states[COMPUTER_TEMP] >= 40) {
    #ifdef DEBUG
      printf_P(PSTR("SLEEP: Info: Go to long sleep.\n\r"));
    #endif
    // turn off relays
    relayOff(LAMP);
    relayOff(PUMP_WATERING);
    relayOff(PUMP_MISTING);
    // tunr off lcd
    lcd.setBacklight(false);
    // power down computer for 64 sec
    LowPower.powerDown(SLEEP_8S, 8, ADC_OFF, BOD_OFF); 
    return;
  }
  // check EEPROM
  if(storage.ok == false) {
    states[ERROR] = ERROR_EEPROM;
    return;
  }
  // read DHT sensor
  if(read_DHT() == false) {
    states[ERROR] = ERROR_DHT;
    return;
  }
  // read BH1750 sensor
  if(read_BH1750() == false) {
    states[ERROR] = ERROR_BH1750;
    return;
  }
  // read DS18B20 sensors
  if(read_DS18B20() == false) {
    states[ERROR] = ERROR_DS18B20;
    return;
  }
  // reset error
  states[ERROR] = NO_ERROR;

  // check substrate temperature
  if(states[SUBSTRATE_TEMP] <= settings.subsTempMinimum) {
    states[WARNING] = WARNING_SUBSTRATE_COLD;
    return;
  }
  // check air temperature
  if(states[AIR_TEMP] <= settings.airTempMinimum) {
    states[WARNING] = WARNING_AIR_COLD;
    return;
  } else if(states[AIR_TEMP] >= settings.airTempMaximum) {
    states[WARNING] = WARNING_AIR_HOT;
  }
  // reset warning
  states[WARNING] = NO_WARNING;
}

void doWork() {
  // don't do any work while error
  if(states[ERROR] != NO_ERROR && 
  	  // skip no substrate error
  	  states[ERROR] != ERROR_NO_SUBSTRATE) {
    return;
  }
  uint8_t speed = 60; // sec per min
  // check humidity
  if(states[HUMIDITY] <= settings.humidMinimum) {
    speed /= 2; // do work twice often
  } else if(states[HUMIDITY] >= settings.humidMaximum) {
    speed *= 2; // do work twice rarely
  }
  // sunny time
  if(states[LIGHT] >= 2500 &&
      settings.silentMorning <= clock.hour() && 
      clock.hour() < settings.silentEvening) {
    #ifdef DEBUG
      printf_P(PSTR("Work: Info: Sunny time.\n\r"));
    #endif
    checkWateringPeriod(settings.wateringSunnyPeriod, speed);
    checkMistingPeriod(settings.mistingSunnyPeriod, speed);
    return;
  }
  // night time
  if(states[LIGHT] <= settings.lightMinimum && 
      (settings.silentEvening <= clock.hour() || 
      clock.hour() < settings.silentMorning)) {
    #ifdef DEBUG
      printf_P(PSTR("Work: Info: Night time.\n\r"));
    #endif
    // nothing, silent night
    return;
  }
  // other time period
  checkWateringPeriod(settings.wateringPeriod, speed);
  checkMistingPeriod(settings.mistingPeriod, speed);
}

void checkWateringPeriod(uint8_t _period, uint8_t _time) {
  if(_period != 0 && millis()/MILLIS_TO_SEC > lastWatering + (_period * _time)) {
    startWatering = millis()/MILLIS_TO_SEC;
    beep.play(ONE_BEEP);
  }
}

void checkMistingPeriod(uint8_t _period, uint8_t _time) {
  if(_period != 0 && millis()/MILLIS_TO_SEC > lastMisting + (_period * _time)) {
    startMisting = settings.mistingDuration;
    beep.play(ONE_BEEP);
  }
}

void doLight() { 
  // try to up temperature
  if(states[AIR_TEMP] <= settings.airTempMinimum &&
      states[LIGHT] > 100) {
    // turn on lamp
    relayOn(LAMP);
    return;
  }
  uint16_t dtime = clock.hour()*60+clock.minute();

  // light enough
  if(states[LIGHT] > settings.lightMinimum) {
    // turn off lamp
    relayOff(LAMP);
    
    bool morning = 4 < clock.hour() && clock.hour() <= 8;
    // save sunrise time
    if(morning && sunrise == 0) {
      sunrise = dtime;
    } else
    // watch for 30 min to check
    if(morning && sunrise+30 <= dtime) {
      #ifdef DEBUG
        printf_P(PSTR("Light: Info: Set new Day Start time: %02d:%02d.\n\r"), 
        sunrise/60, sunrise%60);
      #endif
      // save to EEPROM if big difference (more than 30 min)
      if(sunrise-90 > settings.lightDayStart || 
          sunrise-30 < settings.lightDayStart) {
        storage.changed = true;
      }
      // setup 1 hour earlier
      settings.lightDayStart = sunrise-60;
      // prevent rewrite, move to out of morning
      sunrise += 300;
    }
    return;
  }
  // reset sunrise time
  sunrise = 0;
  // keep light day
  uint16_t lightDayEnd = settings.lightDayStart+(settings.lightDayDuration*60);
  if(settings.lightDayStart <= dtime && dtime <= lightDayEnd) {
    #ifdef DEBUG
      printf_P(PSTR("Light: Info: Lamp On till: %02d:%02d.\n\r"), 
        lightDayEnd/60, lightDayEnd%60);
    #endif
    // turn on lamp
    relayOn(LAMP);
    return;
  }
  // turn off lamp
  relayOff(LAMP);
}

void misting() {
  if(startMisting == 0 || 
      states[WARNING] == WARNING_NO_WATER) {
    // stop misting
    if(states[PUMP_MISTING]) {
      #ifdef DEBUG
        printf_P(PSTR("Misting: Info: Stop misting.\n\r"));
      #endif
      relayOff(PUMP_MISTING);
      if(states[WARNING] == WARNING_MISTING)
        states[WARNING] = NO_WARNING;
    }
    return;
  }
  #ifdef DEBUG
    printf_P(PSTR("Misting: Info: Misting...\n\r"));
  #endif
  states[WARNING] = WARNING_MISTING;
  startMisting--;
  lastMisting = millis()/MILLIS_TO_SEC;
  relayOn(PUMP_MISTING);
}

void watering() {
  if(startWatering == 0 && 
  	  states[PUMP_WATERING] == false) {
    return;
  }
  // time is over
  if(startWatering + (settings.wateringDuration*60) <= millis()/MILLIS_TO_SEC || 
      states[ERROR] == ERROR_NO_SUBSTRATE) {
    // stop watering
    if(states[WARNING] == INFO_SUBSTRATE_DELIVERED) {
      #ifdef DEBUG
        printf_P(PSTR("Watering: Info: Stop watering.\n\r"));
      #endif
      relayOff(PUMP_WATERING);
      startWatering = 0;
      if(states[WARNING] == WARNING_WATERING)
        states[WARNING] = NO_WARNING;
      return;
    }
    // emergency stop
    relayOff(PUMP_WATERING);
    states[WARNING] = WARNING_SUBSTRATE_LOW;
    startWatering = 0;
    #ifdef DEBUG
      printf_P(PSTR("Watering: Error: Emergency stop watering.\n\r"));
    #endif
    return;
  }
  // set pause for cleanup pump and rest
  uint8_t pauseDuration = 5;
  if(states[WARNING] == INFO_SUBSTRATE_DELIVERED ||
      states[WARNING] == WARNING_SUBSTRATE_LOW)
    pauseDuration = 19; // max officient pause
  // pause every 30 sec
  if((millis()/MILLIS_TO_SEC-startWatering) % 30 <= pauseDuration) {
    #ifdef DEBUG
      printf_P(PSTR("Watering: Info: Pause for clean up.\n\r"));
    #endif
    relayOff(PUMP_WATERING);
    return;
  }
  #ifdef DEBUG
    printf_P(PSTR("Misting: Info: Watering...\n\r"));
  #endif
  states[WARNING] = WARNING_WATERING;
  lastWatering = millis()/MILLIS_TO_SEC;
  relayOn(PUMP_WATERING);
}
