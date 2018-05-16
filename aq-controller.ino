#include <Arduino.h>
#include <EEPROM.h>
#include <avr/wdt.h>
#include <Wire.h>

#include <ClickEncoder.h>
#include <TimerOne.h>

#include "MenuItem.h"
#include "Clock.h"
#include "Display.h"
#include "types.h"

#define ENCODER_SW A0
#define ENCODER_DT A1
#define ENCODER_CLK A2

//relay
#define LIGHT_PIN 3
#define CO2_PIN 2
#define COOLER_PIN 4

// thermistor
#define NTC_PIN A3
#define THERM_RESISTANCE 10000
#define THERM_BETA 3950
#define THERM_SERIES_RESISTOR 9980

const double MAX_ADC = 1023.0;
const double ROOM_TEMP = 298.15;   // room temperature in Kelvin

#define SETTINGS_BYTE 0x00

struct Settings {
  int8_t co2 = -30;
  temp_t temperature = 240;
  time_t lightOn = 8 * HOUR + 30 * MINUTE;
  time_t lightOff = 19 * HOUR;
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

class Light : public MenuItem {
  private:
    static const time_t HALF_HOUR = HOUR * 0.5;
    int8_t selected = 0;
    
  public:
    Relay relay = Relay(LIGHT_PIN);
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
  public:
    Relay relay = Relay(CO2_PIN);
    int8_t interval = 0; //minutes

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
    const uint16_t ADC_SAMPLES = 5000;
    
    volatile long adcSamples = 0;
    volatile uint16_t adcCounter = 0;
    volatile int16_t adcAvg = 0;
    
    const short activeDelay = 3;
    
  public:
    Relay relay = Relay(COOLER_PIN);
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
      if (relay.relayEnabled && current <= target) {
        relay.change(false);
      } else if (!relay.relayEnabled && current >= target + activeDelay) {
        relay.change(true);
      }
      
      update();
    }

    void sample() {
      adcSamples += analogRead(NTC_PIN);
      adcCounter++;
      if (adcCounter == ADC_SAMPLES) {
        adcAvg = adcSamples / ADC_SAMPLES;
        adcSamples = 0;
        adcCounter = 0;
      }
    }
    
    temp_t readThermistor() {
      double rThermistor = 0;
      double tKelvin     = 0;
      double tCelsius    = 0;
      
      rThermistor = THERM_SERIES_RESISTOR * ((MAX_ADC / adcAvg) - 1);
      if (rThermistor > 25000 || rThermistor < 1000) return 0;
      
      tKelvin = (THERM_BETA * ROOM_TEMP) / 
                (THERM_BETA + (ROOM_TEMP * log(rThermistor / THERM_RESISTANCE)));
      tCelsius = tKelvin - 273.15;
    
      return tCelsius * 10;
    }

    void update() {
      current = readThermistor();
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
      display.print("Light: ");
      display.println(light.relay.relayEnabled);
      display.print("CO2: ");
      display.println(co2.relay.relayEnabled);
      display.print("Cool: ");
      display.println(temperature.relay.relayEnabled);
      display.print("Errors: ");
      display.print(clock.errors);
    }
} stats;

class Save : public MenuItem {
  public:
    Save() : MenuItem("Save") {
      Settings settings;
      
      if (EEPROM.read(0) == SETTINGS_BYTE) {
        EEPROM.get(1, settings);
      }
      
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

      EEPROM.put(1, settings);
      EEPROM.write(0, SETTINGS_BYTE);

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

      display.setCursor(0, 0);
      if (isOpened) display.print('-');
      display.println(items[selected]->title);
      items[selected]->draw();
      
      display.display();
    }
} menu;

void timerIsr() {
  temperature.sample();
  encoder->service();
}

void relayInit() {
  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(CO2_PIN, OUTPUT);
  pinMode(COOLER_PIN, OUTPUT);
  digitalWrite(LIGHT_PIN, HIGH);
  digitalWrite(CO2_PIN, HIGH);
  digitalWrite(COOLER_PIN, HIGH);
}

void encoderInit() {
  encoder = new ClickEncoder(ENCODER_DT, ENCODER_CLK, ENCODER_SW, 4);

  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);
}

void setup() {
  Wire.begin();
  wdt_enable(WDTO_2S);
  pinMode(13, OUTPUT);
  
  relayInit();
  displayInit();
  rtcInit();
  encoderInit();

  delay(1000);
}

void loop() {
  wdt_reset();
  clock.updateTime();
  handleEncoder();
  menu.draw();
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

