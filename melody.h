#ifndef MELODY_H
#define MELODY_H

#include "pitches.h"

// short beep
#define BEEP  0
const uint16_t beep1[]         = { N_A5, N_E5, N_REST };
const uint8_t beep1_beats[]    = { 2, 	 2,    4 };
const uint8_t beep1_tempo      = 25;
// "R2D2" melody
#define R2D2  1
const uint16_t r2d2[]          = { N_A6, N_G6, N_E6, N_C6, N_D6, N_B6, N_F6, N_C7,
                                   N_A6, N_G6, N_E6, N_C6, N_D6, N_B6, N_F6, N_C7 };
const uint8_t r2d2_beats[]     = { 2,    2,    2,    2,    2,    2,    2,    2,
                                   2,    2,    2,    2,    2,    2,    2,    2 };
const uint8_t r2d2_tempo       = 40;


class Melody
{
public:
  Melody( uint8_t _pin ) {
    pin = _pin;
  }

  void beep( uint8_t _beepCount ) {
	beepCount = _beepCount;
  }

  void play( uint8_t _melodyNum ) {
    pinMode(pin, OUTPUT);
    // prevent new play if previous not ended
    if( noteIndex < numNotes ) 
	  return;
	// compile chosen melody
	compileMelody(_melodyNum);
	// reset index
	noteIndex = 0;
	notePause = 0;
	time = 0;
  }

  void update( void ) {
	// beep cycle
	if( beepCount > 0 && noteIndex >= numNotes ) {
	  play(0);
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
  }

private:
  uint8_t pin;
  uint8_t beepCount;
  const uint16_t *notes;
  const uint8_t *beats;
  uint8_t tempo;	
  uint8_t numNotes;
  uint8_t noteIndex;
  uint16_t notePause;
  uint32_t time;

  void compileMelody( uint8_t _melodyNum ) {
    switch(_melodyNum) {
      case BEEP:
      	notes = beep1;
        beats = beep1_beats;
        tempo = beep1_tempo;
        numNotes = sizeof(beep1)/sizeof(uint16_t);
      	break;
      case R2D2:
      	notes = r2d2;
      	beats = r2d2_beats;
      	tempo = r2d2_tempo;
        numNotes = sizeof(r2d2)/sizeof(uint16_t);
        break;
    }
    #ifdef DEBUG_MELODY
      printf_P(PSTR("MELODY: Info: Melody: #%d, Notes count: #%d.\n\r"), 
        _melodyNum, numNotes);
    #endif
  }

  void playNote( void ) {
    noTone(pin);
		
    uint16_t freq = notes[noteIndex] * 2;
    uint16_t duration = tempo * beats[noteIndex];
	
    #ifdef DEBUG_MELODY
      printf_P(PSTR("MELODY: Info: Note #%d, freq: %d*2, duration: %d*%d.\n\r"),
      noteIndex, notes[noteIndex], beats[noteIndex], tempo);
    #endif

    if (freq > 0) {
      tone(pin, freq, duration); 
    }
    // to distinguish the notes, set a minimum time between them.
    // the note's duration + 30% seems to work well:
    notePause = duration + (duration * 0.30);
  }
};

#endif // __MELODY_H__
