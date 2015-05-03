#ifndef BEEP_H
#define BEEP_H

#include "pitches.h"

static const uint8_t ONE_BEEP = 1;
static const uint8_t TWO_BEEP = 2;
static const uint8_t FIVE_BEEP = 5;

const uint16_t notes[] = { N_A5, N_E5, N_REST };
const uint8_t beats[] = { 2, 2, 4 };
const uint8_t tempo = 25;
const uint8_t numNotes = 3;

class Beep
{
public:
  Beep( uint8_t _pin ) {
    pin = _pin;
    pinMode(pin, OUTPUT);
  }

  void play( uint8_t _beepCount ) {
    beepCount = _beepCount;
  }

  void update() {
    // fast exit
    if( beepCount == 0 )
      return;
    // last note
    if( noteIndex >= numNotes ) {
      noteIndex = 0;
      notePause = 0;
      time = 0;
      beepCount--;
      return;
    }
    // pause between notes
    if( millis() - time < notePause )
      return;
    // play current note
    noTone(pin);
    uint16_t duration = tempo * beats[noteIndex];
    if( notes[noteIndex] > 0 ) {
      tone(pin, notes[noteIndex] * 2, duration); 
    }
    // to distinguish the notes, set a minimum time between them.
    // the note's duration + 30% seems to work well:
    notePause = duration * 1.30;
    // change note cursor
    noteIndex++;
    time = millis();
  }

private:
  uint8_t pin;
  uint8_t beepCount;
  uint8_t noteIndex;
  uint16_t notePause;
  uint32_t time;

};

#endif // __BEEP_H__
