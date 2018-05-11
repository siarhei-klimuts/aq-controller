#include "MenuItem.h"

#include <Arduino.h>
#include "types.h"
    
MenuItem::MenuItem() {};
MenuItem::MenuItem(const char *title) : title(title) {};

void MenuItem::onChange(int16_t value) {}

bool MenuItem::onClick(bool isOpen) {
  return isOpen;
}

void MenuItem::onTimeChange(time_t time) {}

