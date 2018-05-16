#include "Clock.h"

#include <Arduino.h>
#include "Display.h"
#include "RTClib.h"
#include "MenuItem.h"

RTC_DS1307 RTC;

void rtcInit() {
  RTC.begin();
  if (!RTC.isrunning()) {
    RTC.adjust(DateTime(__DATE__, __TIME__));
  }
}

void formatTime(time_str_t buffer, time_t time) {
  int8_t seconds = time / SECOND % 60;
  int8_t minutes = time / MINUTE % 60;
  int8_t hours = time / HOUR % 24;
  sprintf(buffer, "%02d:%02d:%02d", hours, minutes, seconds);
}

void Clock::notyfyListeners() {
  for(int8_t i = 0; i < LISTENERS_COUNT; i++) {
    listeners[i]->onTimeChange(time);
  }
}

void Clock::updateTime() {
  DateTime now = RTC.now();
  uint8_t hour = now.hour();
  uint8_t minute = now.minute();
  uint8_t second = now.second();

  if(hour < 24 && minute < 60 && second < 60) {
    time_t currentTime = hour * HOUR + minute * MINUTE + second * SECOND;
    if (currentTime != time) {
      time = currentTime;
      notyfyListeners(); 
    }
  } else {
    errors++;
  }
}

void Clock::onChange(int16_t value) {
  uint32_t unixtime = RTC.now().unixtime();
  RTC.adjust(DateTime(unixtime + (value * MINUTE)));
}

void Clock::subscribe(MenuItem *item, int8_t index) {
  listeners[index] = item;
}

void Clock::draw() {
  time_str_t timeString;
  formatTime(timeString, time);

  display.println(timeString);
}

Clock clock;

