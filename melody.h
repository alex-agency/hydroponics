
#ifndef MELODY_H
#define MELODY_H

#include "pitches.h"
#include <avr/pgmspace.h>

#define DEBUG

// declare melody count
#define numMelodies  2

class Melody
{
public:
  Melody( uint8_t _pin ) {
    pin = _pin;
  };

  void beep( uint8_t _beepCount ) {
    if( _beepCount < 0 )
	  return;
	// set beep count
	beepCount = _beepCount-1;
	// play beep first time
	play(1);
  };

  void play( uint8_t _melodyNum ) {
  	// initialize speaker pin
    pinMode(pin, OUTPUT);
    // prevent new play if previous not ended
    if( noteIndex < numNotes ) 
	  return;
	// check number
	if(_melodyNum < 0 || _melodyNum == 0 || _melodyNum > numMelodies)
      melodyNum = random(2, numMelodies);
	else
	  melodyNum = _melodyNum;
	// compile chosen melody
	compileMelody();
	// reset index
	noteIndex = 0;
	notePause = 0;
	time = 0;
  };

  void update( void ) {
	// beep cycle
	if( beepCount > 0 && noteIndex >= numNotes ) {
	  // play beep melody
	  play(1);
	  beepCount--;
	}
	// skip if it last note or pause between notes
	if( noteIndex >= numNotes || (millis() - time) < notePause )
	  return;
	// play current note
	playNote();
	// change note cursor
	noteIndex++;
	time = millis();	
  };

private:
  uint8_t pin;
  uint8_t beepCount;
  uint8_t melodyNum;
  const uint16_t *notes;
  const uint8_t *beats;
  uint8_t tempo;	
  uint8_t numNotes;
  uint8_t noteIndex;
  uint16_t notePause;
  uint32_t time;

  void compileMelody( void ) {
	// short beep 
	if( melodyNum == 1 ) {
	  const uint16_t beep1[] 			= { N_A5, N_E5, N_REST };
	  notes 							= beep1;
	  const uint8_t beep1_beats[] 		= { 2, 	  4,    12 };
	  beats 							= beep1_beats;
	  tempo 							= 60;
	  numNotes 							= sizeof(beep1)/sizeof(uint16_t);
	}
	// "R2D2" melody
    else if( melodyNum == 2 ) {
	  const uint16_t r2d2[] 			= { N_A6, N_G6, N_E6, N_C6, N_D6, N_B6, N_F6, N_C7,
                     						N_A6, N_G6, N_E6, N_C6, N_D6, N_B6, N_F6, N_C7 };
	  notes 							= r2d2;
	  const uint8_t r2d2_beats[] 		= { 2,    2,    2,    2,    2,    2,    2,    2,
                               				2,    2,    2,    2,    2,    2,    2,    2 };
	  beats 							= r2d2_beats;
	  tempo 							= 40;
	  numNotes 							= sizeof(r2d2)/sizeof(uint16_t);
	}
  };

  void playNote( void ) {
	noTone(pin);
		
	int freq = notes[noteIndex] * 2;
	int duration = tempo * beats[noteIndex];

	#ifdef DEBUG
	  printf_P(PSTR("MELODY: Info: Note #%u, freq: %u*2, duration: %u*%u.\n\r"),
        noteIndex, notes[noteIndex], beats[noteIndex], tempo);
	#endif

	if (freq > 0) {
	  tone(pin, freq, duration); 
	}
	// to distinguish the notes, set a minimum time between them.
	// the note's duration + 30% seems to work well:
	notePause = duration + (duration * 0.30);
  };
};

#endif // __MELODY_H__
