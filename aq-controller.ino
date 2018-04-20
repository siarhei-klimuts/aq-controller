#include <Arduino.h>
#include <EEPROM.h>

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSans9pt7b.h>

#include <ClickEncoder.h>
#include <TimerOne.h>

#include "thermistor.h"

// OLED display TWI address
#define OLED_ADDR 0x3C

#define ENCODER_SW A0
#define ENCODER_DT A1
#define ENCODER_CLK A2

//relay
#define LIGHT_PIN 8
#define CO2_PIN 9
#define COOLER_PIN 10

// thermistor
#define NTC_PIN A3
#define THERM_RESISTANCE 10230
#define THERM_BETA 3950
#define THERM_SERIES_RESISTOR 9770

#define SECOND 1000
#define MINUTE 60000
#define HOUR 3600000
#define MAX_TIME 86400000

typedef long time_t;
typedef char time_str_t[9];

typedef int16_t temp_t;
typedef char temp_str_t[7];

struct Settings {
  int8_t co2 = 0;
  temp_t temperature = 0;
  time_t lightOn = 0;
  time_t lightOff = 0;
};

struct Relay {
  bool relayEnabled = false;
  int8_t pin;
  
  Relay(int8_t pin) : pin(pin) {}
  
  void change(bool enabled) {
      if (enabled && !relayEnabled) {
        digitalWrite(pin, LOW);
      } else if(!enabled && relayEnabled) {
        digitalWrite(pin, HIGH);
      }

      relayEnabled = enabled;
  }
};

Adafruit_SSD1306 display(-1);  // -1 = no reset pin
THERMISTOR thermistor(NTC_PIN, THERM_RESISTANCE, THERM_BETA, THERM_SERIES_RESISTOR);
ClickEncoder *encoder;

time_t changeTime(time_t time, time_t value) {
  time += value;
  
  if (time >= MAX_TIME) {
    time = time - MAX_TIME;
  }
  if (time < 0) {
    time = MAX_TIME + time;
  }

  return time;
}

void formatTime(time_str_t buffer, time_t time) {
  int8_t seconds = time / SECOND % 60;
  int8_t minutes = time / MINUTE % 60;
  int8_t hours = time / HOUR % 24;
  sprintf(buffer, "%02d:%02d:%02d", hours, minutes, seconds);
}

class MenuItem {
  public:
    const char *title;
    
    MenuItem() {};
    MenuItem(const char *title) : title(title) {};
    
    virtual void onChange(int16_t value) {}
    virtual bool onClick(bool isOpen) {
      return isOpen;
    }
    virtual void draw() = 0;
    virtual void onTimeChange(time_t time) {}
};

class Clock : public MenuItem {
  private:
    static const int8_t LISTENERS_COUNT = 3;
    MenuItem *listeners[LISTENERS_COUNT];
    
    void notyfyListeners() {
      for(int8_t i = 0; i < LISTENERS_COUNT; i++) {
        listeners[i]->onTimeChange(time);
      }
    }
    
    void change(long value) {
      long prevSecs = time / SECOND;
      time = changeTime(time, value);

      if ((time / SECOND) != prevSecs) {
        notyfyListeners();
      }
    }
    
  public:
    volatile time_t time = MAX_TIME / 2;
    
    Clock() : MenuItem("Time") {};
    
    void updateTime() {
      change(1);
    }
    
    void onChange(int16_t value) {
      change(value * MINUTE);
    }
    
    void subscribe(MenuItem *item, int8_t index) {
      listeners[index] = item;
    }
    
    void draw() {
      time_str_t timeString;
      formatTime(timeString, time);

      display.println(timeString);
    }
} clock;

class Light : public MenuItem {
  private:
    static const long HALF_HOUR = 1800000;
    int8_t selected = 0;
    Relay relay = Relay(LIGHT_PIN);
    
  public:
    time_t on;
    time_t off;

    Light() : MenuItem("Light") {};
    
    void onChange(int16_t value) {
      if (selected == 0) {
        on = changeTime(on, value * HALF_HOUR);
      } else {
        off = changeTime(off, value * HALF_HOUR);
      }
    }
    
    bool onClick(bool isOpen) {
      if (isOpen) return true;

      selected++;

      if (selected > 1) {
        selected = 0;
        return false;
      }

      return true;
    }
    
    void onTimeChange(time_t time) {
      relay.change(time >= on && time < off);
    }
    
    void draw() {
      time_str_t str;

      formatTime(str, on);
      display.print(selected == 0 ? "-" : " ");
      display.println(str);

      formatTime(str, off);
      display.print(selected == 1 ? "-" : " ");
      display.println(str);

    }
} light;

class Co2 : public MenuItem {
  private:
    Relay relay = Relay(CO2_PIN);
    
  public:
    int8_t interval = -30; //minutes

    Co2() : MenuItem("CO2") {};

    void onTimeChange(time_t time) {
      time_t timeShift = time - interval * MINUTE;
      relay.change(timeShift >= light.on && timeShift < light.off);
    }
    
    void onChange(int16_t value) {
      interval += value;
    }
    
    void draw() {
      display.print(interval);
      display.println("m");
    }
} co2;

class Temperature : public MenuItem {
  private:
    Relay relay = Relay(COOLER_PIN);
    
  public:
    temp_t target = 0;
    temp_t current = 0;

    Temperature() : MenuItem("Temperature") {};

    static void format(temp_str_t buffer, temp_t value) {
      sprintf(buffer, "%02d.%01d C", (int)value / 10, value % 10);
    }
    
    void onChange(int16_t value) {
      target += value;
    }

    void onTimeChange(time_t time) {
      relay.change(current > target);
    }

    void update() {
      current = thermistor.read();
    }
    
    void draw() {
      temp_str_t str;
      format(str, target);

      display.println(str);
    }
} temperature;

class Stats : public MenuItem {
  public:
    Stats() : MenuItem("Stats") {};
    
    void draw() {
      time_str_t timeStr;
      temp_str_t tempStr;
      
      formatTime(timeStr, clock.time);
      temperature.format(tempStr, temperature.current);

      display.println(timeStr);
      display.println(tempStr);
    }
} stats;

class Save : public MenuItem {
  public:
    Save() : MenuItem("Save") {
      Settings settings;
      EEPROM.get(0, settings);
      
      co2.interval = settings.co2;
      temperature.target = settings.temperature;
      light.on = settings.lightOn;
      light.off = settings.lightOff;
    }
    
    bool onClick(bool isOpen) {
      Settings settings;
      
      settings.co2 = co2.interval;
      settings.temperature = temperature.target;
      settings.lightOn = light.on;
      settings.lightOff = light.off;
      
      EEPROM.put(0, settings);

      return false;
    }
    void draw() {}
} save;

class Menu {
    static const uint8_t MENU_SIZE = 6;
    MenuItem *items[MENU_SIZE];
    int8_t selected = 0;
    bool isOpened = false;

  public:
    Menu() {
      items[0] = &stats;
      items[1] = &clock;
      items[2] = &co2;
      items[3] = &temperature;
      items[4] = &light;
      items[5] = &save;

      clock.subscribe(&co2, 0);
      clock.subscribe(&light, 1);
      clock.subscribe(&temperature, 2);
    }

    void onChange(int16_t value) {
      if (isOpened) {
        items[selected]->onChange(value);
      } else {
        selected += value;
        if (selected < 0) selected = MENU_SIZE + selected;
        if (selected >= MENU_SIZE) selected = selected - MENU_SIZE;
      }
    }
    
    void onClick() {
      isOpened = items[selected]->onClick(!isOpened);
    }
    
    void draw() {
      display.clearDisplay();

      display.setCursor(0, 12);
      if (isOpened) display.print('-');
      display.println(items[selected]->title);
      items[selected]->draw();
      
      display.display();
    }
} menu;

void timerIsr() {
  encoder->service();
  clock.updateTime();
}

void setup() {
  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(CO2_PIN, OUTPUT);
  pinMode(COOLER_PIN, OUTPUT);
  digitalWrite(LIGHT_PIN, HIGH);
  digitalWrite(CO2_PIN, HIGH);
  digitalWrite(COOLER_PIN, HIGH);
  
  encoder = new ClickEncoder(ENCODER_DT, ENCODER_CLK, ENCODER_SW, 4);

  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);

  // initialize display
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  display.setTextSize(1);
  display.setFont(&FreeSans9pt7b);
  display.setTextColor(WHITE);
  display.setCursor(10, 40);
  display.print("Hello Siarhei!");
  display.display();
  delay(SECOND);
}

void loop() {
  handleEncoder();
  menu.draw();
  temperature.update();
}

void handleEncoder() {
  int16_t value = encoder->getValue();

  ClickEncoder::Button b = encoder->getButton();
  if (b != ClickEncoder::Open && b == ClickEncoder::Clicked) {
    menu.onClick();
  }

  if (value != 0) {
    menu.onChange(value);
  }
}

