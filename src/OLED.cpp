
#include "OLED.h"

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

String rssix(long rssi) {
  String s;
  s = "";
  if (rssi > -80) { s = "."; }
  if (rssi > -70) { s = ".."; }
  if (rssi > -60) { s = "..."; }
  return s;
}

void setupOLED() {

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  Serial.println("SSD1306 OK\n");

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds
}

void dispTemp(String mode, boolean chauffe, float consigne, float temp, long rssi) {
  dispTemp(mode, chauffe, consigne, temp, true, true, rssi);
}

void dispTemp(String mode, boolean chauffe, float consigne, float temp, boolean on_mode, boolean on_consigne, long rssi) {
  // Serial.println("dispTemp: Consigne = " + (String) consigne + ", température = " + temp);

  char c[4];
  char t[4];

  // converti float en texte  consigne -> c ; temp -> t 
  dtostrf(temp, 4, 1, t);
  dtostrf(consigne, 4, 1, c);

/*   String s = "dispTemp: Consigne = ";
  s += c[0];
  s += c[1];
  s += c[2];
  s += c[3];
  s += ", température = ";
  s += t[0];
  s += t[1];
  s += t[2];
  s += t[3];
  Serial.println(s);
  Serial.print("dispTemp: Consigne = ");
  Serial.println(c);
  Serial.print("dispTemp: température = ");
  Serial.println(t);
  Serial.print("Size of c: ");
  Serial.print(sizeof(c));
  Serial.print(" t: ");
  Serial.print(sizeof(t));
  Serial.println();
 */
  // Clear the buffer
  display.clearDisplay();
  // mode
  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);
  if (!on_mode) {mode = "";}
  display.println("mode: " + mode);

  display.setCursor(110,0);
  display.println(rssix(rssi));

  // Etat de chauffe
  if (chauffe) {
    display.setCursor(120,15);
    display.println(F("F"));
  }

  // Consigne
  if (on_consigne) {
    display.setFont(&FreeSerifItalic12pt7b);
    display.setCursor(5,50);
    if (consigne < 10) {
      display.setCursor(11,50);
    }
    display.println(c);
    display.setFont();
    display.setTextSize(1);
    display.setCursor(48,35);
    display.println(F("C"));
  }
  display.setFont();
  display.setCursor(5,55);
  display.println(F("Consig."));

  // Température
  display.setFont(&FreeSerifItalic18pt7b);
  display.setCursor(60,50);
  if (temp < 10) {
    display.setCursor(78,50);
  }
  display.println(t);
  display.setFont();
  display.setTextSize(1);
  display.setCursor(122,30);
  display.println(F("C"));
  display.setCursor(65,55);
  display.println(F("Temp."));

  display.display();
}

void dispInfo(String version, String vdesc, String ssid, String ip, long rssi) {
  // Serial.println("dispInfo: ssid = " + ssid);
  // Clear the buffer
  display.clearDisplay();
  // mode
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setFont(&FreeSerifItalic12pt7b);
  display.setCursor(0,15);
  display.println("SmartHeater");
  display.setFont();
  display.setTextSize(1);
  display.setCursor(0,20);
  display.println("version: " + version);
  display.setCursor(0,30);
  display.println(vdesc);
  display.setCursor(0,40);
  display.println("ssid: " + ssid);
  display.setCursor(110,40);
  display.println(rssix(rssi));
  display.setCursor(0,50);
  display.println("ip add.: " + ip);
  display.setCursor(90,20);
  display.print(rssi);
  display.print("dBm");
  display.display();
}
