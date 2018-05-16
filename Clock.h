#ifndef __Clock
#define __Clock

#include "types.h"
#include "MenuItem.h"

const time_t SECOND = 1;
const time_t MINUTE = 60;
const time_t HOUR = 3600;
const time_t MAX_TIME = 24 * HOUR;

class Clock : public MenuItem {
  private:
    static const int8_t LISTENERS_COUNT = 3;
    MenuItem *listeners[LISTENERS_COUNT];
    
    void notyfyListeners();
    
  public:
    time_t time = MAX_TIME / 2;
    int16_t errors;
    
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
