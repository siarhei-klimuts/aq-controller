#ifndef __Clock
#define __Clock

#include "types.h"
#include "MenuItem.h"

#define SECOND 1UL
#define MINUTE 60UL
#define HOUR 3600UL
#define MAX_TIME 86400UL 

class Clock : public MenuItem {
  private:
    static const int8_t LISTENERS_COUNT = 3;
    MenuItem *listeners[LISTENERS_COUNT];
    
    void notyfyListeners();
    
  public:
    time_t time = MAX_TIME / 2;
    
    Clock() : MenuItem("Time") {};
    
    void updateTime();
    void onChange(int16_t value);
    void subscribe(MenuItem *item, int8_t index);
    void draw();
};

extern Clock clock;

void rtcInit();

void formatTime (time_str_t buffer, time_t time);

#endif
