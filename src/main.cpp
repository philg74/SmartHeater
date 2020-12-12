#include <Arduino.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>

#define UseDHT 1
#define UseDS18x20 0

////////
// Version of this project
const char* version = "V1.2";
const char* vdesc = "manage power mode";

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
      #define DHTTYPE DHT22
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
#define NB_TOTALPIN ( NB_DIGITALPIN	+ NB_ANALOGPIN )

const uint8_t DHT_Pin = D7;
const uint8_t LED_Pin = D6;
const uint8_t HEAD_Pin = D5;
const uint8_t BILED_Pin = D0;
const uint8_t MODEBUTTON_Pin = D8;
const uint8_t UPBUTTON_Pin = D3;
const uint8_t DOWNBUTTON_Pin = D4;

unsigned long return_iddle_time;               // time to return in iddle mode
unsigned long blink_time;              // time to make blinking what we are setting
enum dspmode
{
      IDDLE,
      SETMODE,
      SETPOINT,
      INFO
};
dspmode activeMode;                    // mode actuel d'affichage 0=iddle 1=displayed info
const uint8_t NUM_DSP_MODES = 4;

enum heatmode {
      Off,
      FrostFree,
      Auto,
      Manu,
      Power
};
heatmode actHeatMode = Off;
const uint8_t NUM_HEAT_MODES = 5;
String heatmodeX[NUM_HEAT_MODES] = {"Off", "Hors gel", "Auto", "Manu", "Power"};

boolean OKButtonState = false;         // variable for reading the pushbutton status

#if (UseDHT == 1)
	DHT myDHT(DHT_Pin, DHT22);
#endif

byte swtch[NB_TOTALPIN];

// valeurs courantes
float setpoint;
uint8_t setpower;
float temperature = 20;
float humidity = 50;
boolean readOK = false;

unsigned long HeatNextChange = 0;         // when in power mode
unsigned long cycleDuration = 1 * 60 * 1000;   // cycle duration in power mode

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
      switch (actHeatMode) { 
      case Power:
            if (millis() > HeatNextChange) {
                  if (swtch[HEAD_Pin]==0) {
                        digitalWrite(HEAD_Pin, HIGH);
                        swtch[HEAD_Pin]=1;
                        // Serial.print("[updateState] Heat ON for: ");
                        // Serial.println(timef(cycleDuration * setpower / 100));
                        HeatNextChange = millis() + cycleDuration * setpower / 100;
                  } else {
                        digitalWrite(HEAD_Pin, LOW);
                        swtch[HEAD_Pin]=0;
                        // Serial.print("[updateState] Heat OFF for: ");
                        // Serial.println(timef(cycleDuration * (100 - setpower) / 100));
                        HeatNextChange = millis() + cycleDuration * (100 - setpower) / 100;
                  }
            }
            break;
      case Auto:
      case Manu:
      case FrostFree:
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
            break;
      default:
            break;
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
      Serial.println("[statistic] time= " + time);
      unsigned long m = ((float) millis() ) / (1000.0 * 60 * SCALE);
      // Serial.printf("[statistic] millis= %lu, scale=%d, m= %lu \n", millis(), (1000 * 60 * SCALE), m);
      uint8_t i = m % HISTDEPTH;
      // Serial.printf("[statistic] i= %d, nbStat= %d, nbReadError= %d, nbPowerOn= %d.\n", i, nbStat, nbReadError, nbPowerOn);
      if (i != l) { // changement de tranche, on stocke
            avgBy10min[l].time = time;
            avgBy10min[l].nbStat = nbStat;
            avgBy10min[l].nbReadError = nbReadError;
            float powerOn = nbPowerOn * 100.0 / nbStat;
            avgBy10min[l].powerOn = (uint8_t) powerOn;
            Serial.printf("[statistic] ===> nb= %d, error= %d, power= %d.\n", avgBy10min[l].nbStat, avgBy10min[l].nbReadError, avgBy10min[l].powerOn);
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
      message += "\"Version\": \""; message += version; message += '"';
      message += ",\"Version description\": \""; message += vdesc; message += '"';
      message += ",\"Wifi\": \""; message += ssid; message += '"';
      message += ",\"RSSI\": "; message += rssi;
      message += ",\"timeSinceBoot\": \""; message += timef(millis()); message += '"';
      message += ",\"mode\": \""; message += heatmodeX[actHeatMode]; message += '"';
      message += ",\"setpower\": "; message += setpower;
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

      // Setting the Mode, if not, it doesn't change
      if (server.hasArg("mode")) {
            String s = server.arg("mode");
            for (int i = 0; i < NUM_HEAT_MODES; i++) {
                  if (s.equals(heatmodeX[i])) {
                        actHeatMode = (heatmode) i;
                  }
            }
      }

      // Setting temp point, mandatory for Auto et Manu modes
      if (server.hasArg("setpoint")) {
            setpoint = server.arg("setpoint").toFloat();
      } else {
            if (actHeatMode == Auto || actHeatMode == Manu) {
                  server.send(200, "text/plain", "setpoint not received");
                  return;
            }
      }

      // Setting power, mandatory for Power mode
      if (server.hasArg("setpower")) {
            setpower = server.arg("setpower").toFloat();
      } else {
            if (actHeatMode == Power) {
                  server.send(200, "text/plain", "setpower not received");
                  return;
            }
      }

      String message = "mode set to: ";
      message += heatmodeX[actHeatMode];
      message += "\ntemperature order set to: ";
      message += setpoint;
      message += "\npower set to: ";
      message += setpower;
 
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

void handleNotFound() { //Handler for no path found
 
      Serial.println(F("=== handleNotFound ==="));

      String message = "ERROR: Path not found\n";
 
      server.send(404, "text/plain", message);
      Serial.println(message);
}

void display(bool on_mode, bool on_consigne) {
      if (actHeatMode == Power) {
            dispPower(heatmodeX[actHeatMode], swtch[HEAD_Pin], setpower, on_mode, on_consigne, rssi);      
      } else {
            dispTemp(heatmodeX[actHeatMode], swtch[HEAD_Pin], setpoint, temperature, on_mode, on_consigne, rssi);
      }
}

void display() {
      if (actHeatMode == Power) {
            dispPower(heatmodeX[actHeatMode], swtch[HEAD_Pin], setpower, rssi);      
      } else {
            dispTemp(heatmodeX[actHeatMode], swtch[HEAD_Pin], setpoint, temperature, rssi);
      }
}

void setup() {

      Serial.begin(115200);
      Serial.setTimeout(5); // Timeout 5ms

      activeMode = IDDLE;
      setupOLED();

      dispMessage(F("SmartHeater is here."));
      dispMessage(F("===================="));

      wifi.addAP("Bbox-4C18966A", "bienvenue");
      wifi.addAP("Linksys", "bienvenue");
      wifi.addAP("NETGEAR22", "Une0range");

      String msg = F("Connecting ...");
      dispMessage(msg);
      delay(500);
      while (wifi.run() != WL_CONNECTED) {  //Wait for connection
            delay(250);
            Serial.print(F("."));
      }
      ssid = WiFi.SSID();
      msg = F("Connected to ");
      msg += ssid;
      dispMessage(msg);

      NextReconnect = millis() + 10000;

      IPAddress myIp = WiFi.localIP();
      sprintf(myIpString, "%d.%d.%d.%d", myIp[0], myIp[1], myIp[2], myIp[3]);

      dispMessage(F("IP address: "));
      dispMessage(myIpString); //Print the local IP

      pinMode(LED_Pin, OUTPUT);
      pinMode(HEAD_Pin, OUTPUT);
      pinMode(BILED_Pin, OUTPUT);
      pinMode(MODEBUTTON_Pin, INPUT);
      pinMode(UPBUTTON_Pin, INPUT);
      pinMode(DOWNBUTTON_Pin, INPUT);

      dispMessage(F("Create dht object on "));
      dispMessage((String) DHT_Pin);
      myDHT.begin();

      setpoint = 18.0;  //init consigne à 18 C
      setpower = 0;

      server.on("/body", handleBody); // Associate the handler function to the path
      server.on("/order", HTTP_GET, setOrder); //
      server.on("/data", HTTP_GET, getData); //
      server.on("/history", HTTP_GET, getHistory); //
      server.onNotFound(handleNotFound);

      server.begin(); //Start the server
      dispMessage("Server listening");

      delay(5000);
}

void loop() {
      // pour débug et avoir le temps de voir ce qui se passe
   	//delay(5000);
  	//Serial.println(F("\nBegin loop\n=============="));

      // reconnect every xs for the best network
      if (millis() > NextReconnect) {
            Serial.println();
            Serial.print(F("[loop] Waiting to RE-connect..."));
            if (wifi.run() != WL_CONNECTED) {
                  delay(500);
                  Serial.print(F("."));
            }
            Serial.println(F(""));
            ssid = WiFi.SSID();
            Serial.print(F("Connected to "));
            Serial.println(ssid);

            NextReconnect = millis() + 60000;
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
            if (swtch[MODEBUTTON_Pin] == HIGH) {    // on vient de relacher le bouton
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
            display();
            break;
      case INFO:
            dispInfo(version, vdesc, ssid, myIpString, rssi);
            break;
      case SETMODE:
            display(onoff, true);
            if (digitalRead(UPBUTTON_Pin) == HIGH) {
                  if (swtch[UPBUTTON_Pin] == LOW) {
                        actHeatMode = static_cast<heatmode>((actHeatMode + 1) % NUM_HEAT_MODES);
                        swtch[UPBUTTON_Pin] = HIGH;
                        return_iddle_time = millis() + 10000;
                  }
            } else {
                  swtch[UPBUTTON_Pin] = LOW;
            }
            if (digitalRead(DOWNBUTTON_Pin) == HIGH) {
                  if (swtch[DOWNBUTTON_Pin] == LOW) {
                        actHeatMode = static_cast<heatmode>((actHeatMode - 1 + NUM_HEAT_MODES) % NUM_HEAT_MODES);
                        swtch[DOWNBUTTON_Pin] = HIGH;
                        return_iddle_time = millis() + 10000;
                  }
            } else {
                  swtch[DOWNBUTTON_Pin] = LOW;
            }
            break;
      case SETPOINT:
            display(true, onoff);
            if (digitalRead(UPBUTTON_Pin) == HIGH) {
                  if (swtch[UPBUTTON_Pin] == LOW) {
                        if (actHeatMode == Power) {
                              setpower += 10;
                        } else {
                              setpoint += 0.5;
                        }
                        swtch[UPBUTTON_Pin] = HIGH;
                        return_iddle_time = millis() + 10000;
                  }
            } else {
                  swtch[UPBUTTON_Pin] = LOW;
            }
            if (digitalRead(DOWNBUTTON_Pin) == HIGH) {
                  if (swtch[DOWNBUTTON_Pin] == LOW) {
                        if (actHeatMode == Power) {
                              setpower -= 10;
                        } else {
                              setpoint -= 0.5;
                        }
                        swtch[DOWNBUTTON_Pin] = HIGH;
                        return_iddle_time = millis() + 10000;
                  }
            } else {
                  swtch[DOWNBUTTON_Pin] = LOW;
            }
            break;
      default:
            display();
            break;
      }
}
