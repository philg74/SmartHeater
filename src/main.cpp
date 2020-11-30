#include <Arduino.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>

#define UseDHT 1
#define UseDS18x20 0

////////
// Version of this project
const char* version = "V1.1";
const char* vdesc = "with buttons";

////////
// WiFi
String ssid;
char myIpString[24];
long rssi;
ESP8266WiFiMulti wifi;
unsigned long NextReconnect;
// Serveur WiFi
ESP8266WebServer server(80);

////////
// DHT
// https://github.com/adafruit/DHT-sensor-library
#if (UseDHT == 1)
	#include <DHT.h>
      #define DHTTYPE    DHT22
#endif

////////
// DS18x20
// https://github.com/PaulStoffregen/OneWire
#if (UseDS18x20 == 1)
	#include <OneWire.h>
#endif

#include <OLED.h>

////////
// CONFIGURATION VARIABLES
#define NB_DIGITALPIN 17
#define NB_ANALOGPIN 1
#define NB_TOTALPIN ( NB_DIGITALPIN	+ NB_ANALOGPIN)

const uint8_t DHT_Pin = D7;
const uint8_t LED_Pin = D4;
const uint8_t HEAD_Pin = D5;
const uint8_t BILED_Pin = D4;
const uint8_t MODEBUTTON_Pin = D0;
const uint8_t UPBUTTON_Pin = D6;
const uint8_t DOWNBUTTON_Pin = D8;

enum dspmode
{
      IDDLE,
      SETMODE,
      SETTEMP,
      INFO
};
dspmode activeMode;                    // mode actuel d'affichage 0=iddle 1=displayed info
const uint8_t NUM_DSP_MODES = 4;
unsigned long return_iddle_time;               // time to return in iddle mode
unsigned long blink_time;              // time to make blinking what we are setting

boolean OKButtonState = false;         // variable for reading the pushbutton status

#if (UseDHT == 1)
	DHT myDHT(DHT_Pin, DHTTYPE);
#endif

byte swtch[NB_TOTALPIN];

// valeurs courantes
float setpoint;
float temperature = 20;
float humidity = 50;
boolean readOK = false;

unsigned long ProbeNextRead = millis();
unsigned long LastReadOK = 0;

// cumuls pour calcul moyenne
uint8_t nbStat = 0;
uint8_t nbReadError = 0;
uint8_t nbPowerOn = 0;
float sumTemp = 0.0;
float sumCons = 0.0;

typedef struct average
{
      String time;
      uint8_t nbStat;
      uint8_t nbReadError;
      uint8_t powerOn;
      uint8_t temperature;
      uint8_t consigne;
} average;

// tableaux pour moyenne par ?
#define SCALE 10  // en minute
#define HISTDEPTH 72  // profondeur de l'historique -=- 12h
average avgBy10min[HISTDEPTH];

static uint8_t l;  // last tranche de moyennes

String timef(unsigned long t) {
      uint8_t j = t / (1000 * 60 * 60 * 24);
      uint8_t hh = t / (1000 * 60 * 60) % 24;
      uint8_t mn = (t / (1000 * 60)) % 60;
      uint8_t ss = (t / 1000) % 60;
      String s;
      s = String(j) + ' ';
      if (hh < 10) { s += '0' + String(hh); } else { s += String(hh); }
      s += ':';
      if (mn < 10) { s += '0' + String(mn); } else { s += String(mn); }
      s += ':';
      if (ss < 10) { s += '0' + String(ss); } else { s += String(ss); }
      return s;
}

void probeRead() { //Read probe
      // Lecture température et humidité
      float newHumidity = 0;
      newHumidity = myDHT.readHumidity();
      float newTemperature = 0;
      newTemperature = myDHT.readTemperature();
      if (!isnan(newTemperature)) { 
            temperature = newTemperature;
            LastReadOK = millis();
            readOK = true;
      } else {
            readOK = false;
      }
      if (!isnan(newHumidity)) { humidity = newHumidity; }
/* 
      Serial.print(F("probeRead: Sensor T° :"));
      Serial.print(F("\t"));
      Serial.print(newTemperature, 1);
      Serial.print(F("\tH :\t"));
      Serial.print(newHumidity, 1);
      Serial.print(F("\n"));
 */
}

void updateState() { //Update heat state
      if (setpoint > temperature + 0.5) 
      {
            if (swtch[HEAD_Pin]==0) {
                  digitalWrite(HEAD_Pin, HIGH);
                  swtch[HEAD_Pin]=1;
            }
      } else if (setpoint < temperature - 0.5) {
            if (swtch[HEAD_Pin]==1) {
                  digitalWrite(HEAD_Pin, LOW);
                  swtch[HEAD_Pin]=0;
            }
      }
}

void statistic() {
      // cumuls
      nbStat++;
      if (swtch[HEAD_Pin]) {
            nbPowerOn++;
      }
      if (!readOK) {
            nbReadError++;
      }
      sumCons += setpoint;
      sumTemp += temperature;

      // stockage toutes les 10 mn dans une nouvelle ligne du tableau
      String time = timef(millis());
      Serial.println("time= " + time);
      unsigned long m = ((float) millis() ) / (1000.0 * 60 * SCALE);
      Serial.printf("millis= %lu, scale=%d, m= %lu \n", millis(), (1000 * 60 * SCALE), m);
      uint8_t i = m % HISTDEPTH;
      Serial.printf("i= %d, nbStat= %d, nbReadError= %d, nbPowerOn= %d.\n", i, nbStat, nbReadError, nbPowerOn);
      if (i != l) { // changement de tranche, on stocke
            avgBy10min[l].time = time;
            avgBy10min[l].nbStat = nbStat;
            avgBy10min[l].nbReadError = nbReadError;
            float powerOn = nbPowerOn * 100.0 / nbStat;
            avgBy10min[l].powerOn = (uint8_t) powerOn;
            Serial.printf("===> nb= %d, error= %d, power= %d.\n", avgBy10min[l].nbStat, avgBy10min[l].nbReadError, avgBy10min[l].powerOn);
            avgBy10min[l].consigne = sumCons / nbStat;
            avgBy10min[l].temperature = sumTemp / nbStat;
            l = i;
            // et on ré-initialise
            nbStat = 0;
            nbReadError = 0;
            nbPowerOn = 0;
      }
}

void getData() { //Handler get data from heater
 
      Serial.println(F("=== getData ==="));

      String message = "{";
      message += "\"Wifi\": \""; message += ssid; message += '"';
      message += ",\"timeSinceBoot\": \""; message += timef(millis()); message += '"';
      message += ",\"setpoint\": "; message += setpoint;
      message += ",\"temperature\": "; message += temperature;
      message += ",\"humidity\": "; message += humidity;
      message += ",\"heat\": "; message += swtch[HEAD_Pin];
	message += "}";
 
      server.send(200, "application/json", message);
      Serial.println(message);
}

void getHistory() { //Handler get history from heater
 
      Serial.println(F("=== getHistory ==="));

      String message = "{ \"histo\" : [ ";
      for (int i = 0; i < HISTDEPTH; i++) {
            uint8_t j = ( i + l ) % HISTDEPTH;
            Serial.printf(" histo l=%d  i=%d  j=%d \n", l, i, j);
            message += "{";
            message += "\"timeSinceBoot\" : \""; message += avgBy10min[j].time; message += '"';
            message += ",\"nbLoop\" : "; message += avgBy10min[j].nbStat;
            message += ",\"nbError\" : "; message += avgBy10min[j].nbReadError;
            message += ",\"power\" : "; message += avgBy10min[j].powerOn;
            message += ",\"avgTemperature\" : "; message += avgBy10min[j].temperature;
            message += ",\"avgSetPoint\" : "; message += avgBy10min[j].consigne;
            message += "}";
      }
	message += " ] }";
 
      server.send(200, "application/json", message);
      Serial.println(message);
}

void setOrder() { //Handler order of a new set point for temperature
 
      Serial.println(F("=== setOrder ==="));

      if (server.hasArg("setpoint")== false) {
 
            server.send(200, "text/plain", "setpoint not received");
            return;
 
      }
 
      setpoint = server.arg("setpoint").toFloat();

      String message = "temperature order set to: ";
      message += setpoint;
 
      server.send(200, "text/plain", message);
      Serial.println(message);

      updateState();

}

void handleBody() { //Handler for the body path
 
      Serial.println(F("=== handleBody ==="));

      if (server.hasArg("plain")== false) { //Check if body received
 
            server.send(200, "text/plain", "Body not received");
            return;
 
      }
 
      String message = "Body received:\n";
             message += server.arg("plain");
             message += "\n";
 
      server.send(200, "text/plain", message);
      Serial.println(message);
}

void setup() {

    Serial.begin(115200);
    Serial.setTimeout(5); // Timeout 5ms

    Serial.println(F("SmartHeater is here."));
    Serial.println(F("===================="));

    wifi.addAP("Bbox-4C18966A", "bienvenue");
    wifi.addAP("Linksys", "bienvenue");
    wifi.addAP("NETGEAR22", "Une0range");
 
    Serial.println(F("Waiting to connect..."));
    while (wifi.run() != WL_CONNECTED) {  //Wait for connection
        delay(500);
        Serial.print(F("."));
    }
    Serial.println(F(""));
    Serial.print(F("Connected to "));
    ssid = WiFi.SSID();
    Serial.println(ssid);
    NextReconnect = millis() + 10000;

    IPAddress myIp = WiFi.localIP();
    sprintf(myIpString, "%d.%d.%d.%d", myIp[0], myIp[1], myIp[2], myIp[3]);

    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());  //Print the local IP
 
    pinMode(LED_Pin, OUTPUT);
    pinMode(HEAD_Pin, OUTPUT);
    pinMode(BILED_Pin, OUTPUT);
    pinMode(MODEBUTTON_Pin, INPUT);
    pinMode(UPBUTTON_Pin, INPUT);
    pinMode(DOWNBUTTON_Pin, INPUT);

    Serial.print(F("Create dht object on "));
    Serial.println(DHT_Pin);
    myDHT.begin();

    activeMode = IDDLE;
    setupOLED();

    setpoint = 18.0;  //init consigne à 18 C
 
    server.on("/body", handleBody); // Associate the handler function to the path
    server.on("/order", HTTP_GET, setOrder); //
    server.on("/data", HTTP_GET, getData); //
    server.on("/history", HTTP_GET, getHistory); //
 
    server.begin(); //Start the server
    Serial.println("Server listening");

}

void loop() {
      // pour débug et avoir le temps de voir ce qui se passe
   	//delay(5000);
  	//Serial.println(F("\nBegin loop\n=============="));

      // reconnect every xs for the best network
      if (millis() > NextReconnect) {
            Serial.println();
            Serial.print(F("Waiting to RE-connect..."));
            if (wifi.run() != WL_CONNECTED) {
                  delay(500);
                  Serial.print(F("."));
            }
            Serial.println(F(""));
            ssid = WiFi.SSID();
            Serial.print(F("Connected to "));
            Serial.println(ssid);

            NextReconnect = millis() + 10000;
      }
      server.handleClient(); //Handling of incoming requests
      
/* 	Serial.print(F("ProbeNextRead : "));
	Serial.println(ProbeNextRead);
	Serial.print(F("millis             : "));
	Serial.println(millis());
 */    
      static unsigned long LED_time;

      if (millis() > ProbeNextRead) {
            Serial.println();
		//fait clignoter une LED une fraction de seconde pour montrer que le pgm tourne (LED de contrôle)
		digitalWrite(LED_Pin, HIGH);
            swtch[LED_Pin] = HIGH;
            LED_time = millis() + 900;
		// Serial.println(F("Working LED on"));
            probeRead();
		updateState();
            statistic();
            ProbeNextRead=millis()+3000; // Permet de decaler la lecture entre chaque sonde DHT sinon ne marche pas cf librairie (3000 mini)
      }

      if (swtch[LED_Pin] and millis() > LED_time) {
		//on éteint la LED de contrôle
		digitalWrite(LED_Pin, LOW);
            swtch[LED_Pin] = false;
      }

      OKButtonState = digitalRead(MODEBUTTON_Pin);

      // Gestion changement de mode display
      if (OKButtonState == HIGH) {   // le bouton est appuyé
            if (swtch[MODEBUTTON_Pin] == LOW) {    // on vient d'appuier sur le bouton
                  activeMode = static_cast<dspmode>((activeMode + 1) % NUM_DSP_MODES);
                  Serial.print("Mode actuel:");
                  Serial.println(activeMode);
            }
            digitalWrite(BILED_Pin, LOW);  // allumer build in LED
            swtch[MODEBUTTON_Pin] = HIGH;
      } else {                       // le bouton est relaché
            if (swtch[MODEBUTTON_Pin] == HIGH) {    // on vient dde relacher le bouton
                  return_iddle_time = millis() + 10000;  // retourne en mode veille dans 10s
                  blink_time = millis();
            }
            digitalWrite(BILED_Pin, HIGH);
            swtch[MODEBUTTON_Pin] = LOW;
            if (millis() > return_iddle_time and activeMode != INFO) {     // retourne en mode veille
                  activeMode = IDDLE;
            }
      }

      // Gestion du clignottement pour les réglages (mode, consigne)
      static boolean onoff;
      if (millis() > blink_time) {
            if (!onoff) {    // cycle sur 1s mais plus de on que de off
                  blink_time = millis() + 800;
                  onoff = true;
            } else {
                  blink_time = millis() + 200;
                  onoff = false;
            }
      }

      rssi = WiFi.RSSI();
      //Serial.print("rssi=");
      //Serial.println(rssi);
      // Affichage en focntion du mode display actuel
      switch (activeMode)
      {
      case IDDLE:
            dispTemp("Normal", swtch[HEAD_Pin], setpoint, temperature, rssi);
            break;
      case INFO:
            dispInfo(version, vdesc, ssid, myIpString, rssi);
            break;
      case SETMODE:
            dispTemp("Normal", swtch[HEAD_Pin], setpoint, temperature, onoff, true, rssi);
            break;
      case SETTEMP:
            dispTemp("Normal", swtch[HEAD_Pin], setpoint, temperature, true, onoff, rssi);
            if (digitalRead(UPBUTTON_Pin) == HIGH) {
                  if (swtch[UPBUTTON_Pin] == LOW) {
                        setpoint += 0.5;
                        swtch[UPBUTTON_Pin] = HIGH;
                  }
            } else {
                  swtch[UPBUTTON_Pin] = LOW;
            }
            if (digitalRead(DOWNBUTTON_Pin) == HIGH) {
                  if (swtch[DOWNBUTTON_Pin] == LOW) {
                        setpoint -= 0.5;
                        swtch[DOWNBUTTON_Pin] = HIGH;
                  }
            } else {
                  swtch[DOWNBUTTON_Pin] = LOW;
            }
            break;
      default:
            dispTemp("Normal", swtch[HEAD_Pin], setpoint, temperature, rssi);
            break;
      }
}
