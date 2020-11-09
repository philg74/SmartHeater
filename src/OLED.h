#ifndef OLED_H_INCLUDED
#define OLED_H_INCLUDED

#include <string>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSerifItalic18pt7b.h>
#include <Fonts/FreeSerifItalic12pt7b.h>

void setupOLED();
void dispTemp(String mode, bool chauffe, float consigne, float temp);

#endif // OLED_H_INCLUDED