

#define UseDHT 1
#define UseDS18x20 0

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

ESP8266WebServer server(80);
 
const char* ssid = "Bbox-4C18966A";
const char* password =  "bienvenue";

////////
// DHT
// https://github.com/adafruit/DHT-sensor-library
#if (UseDHT == 1)
	#include <DHT.h>
#endif

////////
// DS18x20
// https://github.com/PaulStoffregen/OneWire
#if (UseDS18x20 == 1)
	#include <OneWire.h>
#endif

#if (UseDHT == 1)
	DHT *myDHT;
#endif

#include <OLED.h>

////////
// CONFIGURATION VARIABLES
#define NB_DIGITALPIN 17
#define NB_ANALOGPIN 1
#define NB_TOTALPIN ( NB_DIGITALPIN	+ NB_ANALOGPIN)


unsigned long ProbeNextRead = millis();
uint8_t DHT_Pin = D7;
uint8_t LED_Pin = D6;
uint8_t HEAD_Pin = D5;

byte swtch[NB_TOTALPIN];

float setpoint;
float temperature = 20;
float humidity = 50;

void probeRead() { //Read probe
      // Lecture température et humidité
      float newHumidity = 0;
      newHumidity = myDHT->readHumidity();
      float newTemperature = 0;
      newTemperature = myDHT->readTemperature();
      if (!isnan(newTemperature)) { temperature = newTemperature; }
      if (!isnan(newHumidity)) { humidity = newHumidity; }

      Serial.print(F("probeRead: Sensor T° :"));
      Serial.print(F("\t"));
      Serial.print(temperature, 1);
      Serial.print(F("\tH :\t"));
      Serial.print(humidity, 1);
      Serial.print(F("\n"));
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
	dispTemp("Normal", swtch[HEAD_Pin], setpoint, temperature);
}

void getData() { //Handler get data from heater
 
      Serial.println(F("=== getData ==="));

      String message = "{";
      message += "\"setpoint\": "; message += setpoint;
      message += ",\"temperature\": "; message += temperature;
      message += ",\"humidity\": "; message += humidity;
      message += ",\"heat\": "; message += swtch[HEAD_Pin];
	message += "}";
 
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
    Serial.print(F("Connecting to "));
    Serial.println(ssid);

    WiFi.begin(ssid, password);  //Connect to the WiFi network
 
    Serial.println(F("Waiting to connect..."));
    while (WiFi.status() != WL_CONNECTED) {  //Wait for connection
        delay(500);
		Serial.print(F("."));
    }
    Serial.println(F(""));

    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());  //Print the local IP
 
    pinMode(LED_Pin, OUTPUT);
    pinMode(HEAD_Pin, OUTPUT);

    Serial.print(F("Create dht object on "));
    Serial.println(DHT_Pin);
    myDHT = new DHT(DHT_Pin, 22);	// DHT22

    setupOLED();
    setpoint = 18;  //init consigne à 18 C

 
    server.on("/body", handleBody); //Associate the handler function to the path
    server.on("/order", HTTP_GET, setOrder); //
    server.on("/data", HTTP_GET, getData); //
 
    server.begin(); //Start the server
    Serial.println("Server listening");

}

void loop() {
    // pour débug et avoir le temps de voir ce qui se passe
   	//delay(5000);

   	//Serial.println(F("\nBegin loop\n=============="));

    server.handleClient(); //Handling of incoming requests

/* 	Serial.print(F("ProbeNextRead : "));
	Serial.println(ProbeNextRead);
	Serial.print(F("millis             : "));
	Serial.println(millis());
 */    
      if (millis() > ProbeNextRead) {
		//fait clignoter une LED une fraction de seconde pour montrer que le pgm tourne (LED de contrôle)
		digitalWrite(LED_Pin, HIGH);
		// Serial.println(F("Working LED on"));
            probeRead();
		updateState();
		//on éteint la LED de contrôle
		delay(500);
		digitalWrite(LED_Pin, LOW);
            ProbeNextRead=millis()+3000; // Permet de decaler la lecture entre chaque sonde DHT sinon ne marche pas cf librairie (3000 mini)
      }
 
}
