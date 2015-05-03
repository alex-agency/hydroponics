#ifndef LCDPANEL_H
#define LCDPANEL_H

#include "LcdMenu.h"
#include "OneButton.h"

static const int UP = 1;
static const int DOWN = -1;

// Declare lcd menu
LcdMenu menu;

// Declare push buttons
OneButton rightButton(A2, true);
OneButton leftButton(A3, true);

void rightButtonClick() {
  menu.nextItem = UP;
  menu.show();
}

void leftButtonClick() {
  menu.nextItem = DOWN;
  menu.show();
}

void buttonsLongPress() {
  beep.play(TWO_BEEP);
  if(menu.editMode != false) {
    // move to next edit field
    menu.editMode--;
    if(menu.menuItem == CLOCK)
      // save changed time
      rtc.adjust(clock);
  } else if(menu.menuItem != HOME) {
    menu.editMode = true;
  } else {
    menu.editMode = false;
  }
  menu.show();
}


class LcdPanel 
{
public:
  void begin() {
    // init buttons
    rightButton.attachClick( rightButtonClick );
    rightButton.attachLongPressStart( buttonsLongPress );
    leftButton.attachClick( leftButtonClick );
    leftButton.attachLongPressStart( buttonsLongPress );
    // init menu
    menu.begin();
  }

  void update() {
    // update menu
    menu.update();
    // update push buttons
    leftButton.tick();
    rightButton.tick();
  }

};

#endif // __LCDPANEL_H__
