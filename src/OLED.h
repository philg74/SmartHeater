#ifndef OLED_H_INCLUDED
#define OLED_H_INCLUDED

#include <string>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSerifItalic18pt7b.h>
#include <Fonts/FreeSerifItalic12pt7b.h>

void setupOLED();
void dispTemp(String, bool, float, float, long);
void dispTemp(String, bool, float, float, bool, bool, long);
void dispPower(String, bool, int, long);
void dispPower(String, bool, int, bool, bool, long);
void dispInfo(String, String, String, String, long);
void dispMessage(String);

#endif // OLED_H_INCLUDED