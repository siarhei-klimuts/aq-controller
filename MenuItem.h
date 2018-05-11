#ifndef __MenuItem
#define __MenuItem

#include <Arduino.h>
#include "types.h"

class MenuItem {
  public:
    const char *title;
    
    MenuItem();
    MenuItem(const char *title);
    
    virtual void onChange(int16_t value);
    virtual bool onClick(bool isOpen);
    virtual void draw() = 0;
    virtual void onTimeChange(time_t time);
};

#endif 
