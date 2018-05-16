#include "Display.h"

#include <Adafruit_GFX.h>

// OLED display TWI address
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(-1);  // -1 = no reset pin

void displayInit() {
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(10, 40);
  display.print("Hello Siarhei!");
  display.display();
}

