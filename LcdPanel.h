#ifndef LCDPANEL_H
#define LCDPANEL_H

#include "LcdMenu.h"
#include "OneButton.h"

// Declare lcd menu
LcdMenu menu;

// Declare push buttons
OneButton rightButton(A2, true);
OneButton leftButton(A3, true);

void rightButtonClick() {
  menu.nextItem = 1;
  menu.showMenu();
}

void leftButtonClick() {
  menu.nextItem = -1;
  menu.showMenu();
}

void buttonsLongPress() {
  melody.beep(2);
  if(menu.editMode > 0) {
    // move to next edit field
    menu.editMode--;
    if(menu.menuItem == CLOCK)
      // save changed time
      rtc.adjust(clock);
  } else if(menu.menuItem != HOME) {
    // enable edit mode
    menu.editMode = 1;
  } else {
    // close Edit menu
    menu.editMode = 0;
  }
  menu.showMenu();
}

// Declare buttons events
void buttons_init() {
  // Configure buttons
  rightButton.attachClick( rightButtonClick );
  rightButton.attachLongPressStart( buttonsLongPress );
  leftButton.attachClick( leftButtonClick );
  leftButton.attachLongPressStart( buttonsLongPress );
}

class LcdPanel 
{
public:
  void begin() {
    // init buttons
    buttons_init();
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
