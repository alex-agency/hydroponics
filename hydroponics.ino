// Import libraries
#include <SPI.h>
#include "printf.h"
//#include "nRF24L01.h"
//#include "RF24.h"
//#include "Mesh.h"
#include "dht11.h"
#include "SimpleMap.h"
#include "timer.h"
#include "button.h"
#include <Wire.h>
#include "DS1307new.h"
//#include "melody.h"
//#include "LowPower.h"

// Debug info
#define DEBUG   true

/*// Declare SPI bus pins
#define CE_PIN  9
#define CS_PIN  10
// Set up nRF24L01 radio
RF24 radio(CE_PIN, CS_PIN);
// Set up network
Mesh mesh(radio);
// Declare radio channel 0-127
const uint8_t channel = 76;
// Declare unique node id
const uint16_t node_id = 333;
// Declare base id, base always has 00 id
const uint16_t base_id = 00;*/

// Declare DHT11 sensor digital pin
#define DHT11PIN  3
// Declare state map keys
#define HUMIDITY  "humidity"
#define TEMPERATURE  "temperature"

// Declare state map keys
#define CDN  "CDN" // days after 2000-01-01
#define TIME  "TIME" // minutes after 00:00

/*// Set up Speaker digital pin
Melody melody(4);
// Declare interval for play melody
const uint16_t alarm_interval = 3600; // seconds
uint32_t last_time_alarm;*/

// Declare state map
struct comparator {
  bool operator()(const char* &str1, const char* &str2) {
    return strcmp(str1, str2) == 0;
  }
};
SimpleMap<const char*, int, 9, comparator> states;

// Declare delay manager
timer_t timer(10*RTC.second);

//
// Setup
//
void setup()
{
  // Configure console
  Serial.begin(57600);
  printf_begin();

  /*// initialize radio
  radio.begin();
  // initialize network
  mesh.begin(channel, node_id);*/

  // Shift NV-RAM address 0x08 for RTC
  RTC.setRAM(0, (uint8_t *)0x0000, sizeof(uint16_t));
  
  // Set Clock
  // 365*13+30*11+23=4745+330+23=5098
  //set_time(5075, 1407);
}

//
// Loop
//
void loop()
{  
  if( timer ) { 
	// sleeping
    /*if(DEBUG) printf("SLEEP: Info: Go to Sleep...\n\r");
    Serial.flush();
    // set all pin to low with pullup.
    for(int i=1; i<=21; i++) {
      pinMode(i, INPUT_PULLUP);
      digitalWrite(i, LOW);
    }
    // Enter power down state for 8*22 sec with ADC and BOD module disabled
    LowPower.powerDown(SLEEP_8S, 22, ADC_OFF, BOD_OFF); 
    if(DEBUG) printf("SLEEP: Info: WakeUp.\n\r");*/

    // read sensors
    read_DHT11();
    read_time();

    // check values
    check();
  }

  // check button
  handle_button();

  // update melody
  //melody.update();

  // send values to base
  /*if( mesh.ready() && timer ) {      
    // send DHT11 sensor values
    Payload payload1(HUMIDITY, states[HUMIDITY]);
    mesh.send(payload1, base_id);
    Payload payload2(TEMPERATURE, states[TEMPERATURE]);
    mesh.send(payload2, base_id);

    // send time value
    Payload payload3(TIME, states[TIME]);
    mesh.send(payload3, base_id);
  }
  
  // update network
  mesh.update();
  
  // read message if available
  while( mesh.available() ) {
    Payload payload;
    mesh.read(payload);
    
    // set new time
    if(payload.key == TIME) {
      set_time(0,payload.value);
    }
  }*/
}

/****************************************************************************/

bool read_DHT11() {
  dht11 DHT11;
  int state = DHT11.read(DHT11PIN);
  switch (state) {
    case DHTLIB_OK:
      states[HUMIDITY] = DHT11.humidity;
      states[TEMPERATURE] = DHT11.temperature;

      if(DEBUG) printf_P(PSTR("DHT11: Info: Sensor values: humidity: %d, temperature: %d.\n\r"), 
                  states[HUMIDITY], states[TEMPERATURE]);
      return true;
    case DHTLIB_ERROR_CHECKSUM:
      printf_P(PSTR("DHT11: Error: Checksum test failed!: The data may be incorrect!\n\r"));
      return false;
    case DHTLIB_ERROR_TIMEOUT: 
      printf_P(PSTR("DHT11: Error: Timeout occured!: Communication failed!\n\r"));
      return false;
    default: 
      printf_P(PSTR("DHT11: Error: Unknown error!\n\r"));
      return false;
  }
}

/****************************************************************************/

void read_time() {
  RTC.getRAM(54, (uint8_t *)0xaa55, sizeof(uint16_t));
  RTC.getTime();
  states[CDN] = RTC.cdn;
  states[TIME] = RTC.hour*60+RTC.minute;

  if(DEBUG) printf_P(PSTR("RTC: Info: CDN: %d -> %d-%d-%d, TIME: %d -> %d:%d:%d.\n\r"), 
              states[CDN], RTC.year, RTC.month, RTC.day, 
              states[TIME], RTC.hour, RTC.minute, RTC.second);
}

/****************************************************************************/

void set_time(int _cdn, int _time) {
  RTC.setRAM(54, (uint8_t *)0xffff, sizeof(uint16_t));
  RTC.getRAM(54, (uint8_t *)0xffff, sizeof(uint16_t));
  RTC.stopClock();
  
  if(_cdn > 0) {
    RTC.fillByCDN(_cdn);
    printf_P(PSTR("RTC: Warning: set new CDN: %d.\n\r"), _cdn);  	
  }

  if(_time > 0) {
    uint8_t minutes = _time % 60;
    _time /= 60;
    uint8_t hours = _time % 24;
    RTC.fillByHMS(hours, minutes, 00);
    printf_P(PSTR("RTC: Warning: set new time: %d:%d:00.\n\r"), 
      hours, minutes);
  }

  RTC.setTime();
  RTC.setRAM(54, (uint8_t *)0xaa55, sizeof(uint16_t));
  RTC.startClock();
}

/****************************************************************************/

void check() {
  if( states[TEMPERATURE] < 13 || states[TEMPERATURE] > 28 ) {
    alarm(0);
    printf_P(PSTR("CHECK: WARNING: Temperature: %d!\n\r"), states[TEMPERATURE]); 
  }

  if( states[HUMIDITY] < 35 && states[TEMPERATURE] > 25 ) {
    printf_P(PSTR("CHECK: WARNING: Humidity too low: %d!\n\r"), states[HUMIDITY]);
  }
}

/****************************************************************************/

void alarm(int mode) {
  //melody.beep(mode);	  	

  //int duration = states[TIME] - last_time_alarm;
  // do alarm each time interval
  /*if( duration > alarm_interval ) {
    last_time_alarm = states[TIME];
    if(DEBUG) printf_P(PSTR("CHECK: Info: Play alarm every %d sec.\n\r"), duration);
    
    melody.play();
  }*/
}

/****************************************************************************/

void relay(const char* relay, int state) {
  /*// initialize relays pin
  pinMode(RELAY1PIN, OUTPUT);
  pinMode(RELAY2PIN, OUTPUT);
  // turn on/off
  if(strcmp(relay, RELAY_1) == 0) {
    digitalWrite(RELAY1PIN, state);
    // !!!lack in principal scheme, 
    //both relay should be enabled!
    digitalWrite(RELAY2PIN, state);
  } 
  else if(strcmp(relay, RELAY_2) == 0) {
    digitalWrite(RELAY2PIN, state);
    // !!!lack in principal scheme
    //both relay should be enabled!
    digitalWrite(RELAY1PIN, state);
  } 
  else {
    printf("RELAY: Error: '%s' is unknown!\n\r", relay);
    return;
  }
  // save states
  if(state == RELAY_ON) {
    if(DEBUG) printf("RELAY: Info: %s is enabled.\n\r", relay);
    states[relay] = true;
  } 
  else if(state == RELAY_OFF) {
    if(DEBUG) printf("RELAY: Info: %s is disabled.\n\r", relay);
    states[relay] = false;
  }*/
}

/****************************************************************************/

bool handle_button() {
  button BUTTON;
  //read button
  /*int state = BUTTON.read(BUTTONPIN, led);
  if(state == BUTTONLIB_RELEASE) {
    return false;
  } 
  else if(state != BUTTONLIB_OK) {
    printf("BUTTON: Error: Incorrect push! It's too short or long.\n\r");
    return false;
  }
  // handle command
  switch (BUTTON.command) {
    case 1:
      // turning ON relay #1
      relay(RELAY_1, RELAY_ON);
      return true;
    case 2:
      // turning ON relay #2
      relay(RELAY_2, RELAY_ON);
      return true;
    case 3:
      // turning OFF relay #1 and #2
      relay(RELAY_1, RELAY_OFF);
      relay(RELAY_2, RELAY_OFF);
      return true;
    default:
      return false;
  }*/
}

/****************************************************************************/
