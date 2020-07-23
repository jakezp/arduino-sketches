/*------------------------------------------------------------------------------------------------------------------  
Temperature sensor with OLED display and secure (TLS) MQTT connection
This was created based on input from:
  - https://github.com/bkpsu/NodeMCU-Home-Automation-Sensor
  - https://preview.tinyurl.com/y2wwg2q2
  - bits & pieces from various other sources
--------------------------------------------------------------------------------------------------------------------*/
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
//#include <Wire.h>               // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306.h"              // OLED Lib for I2C version
#include <OLEDDisplayUi.h>
#include "DHT.h"
#include "images.h"
#include "secure_credentials.h"

/******************************** HARDWARE CONFIG (CHANGE THESE FOR YOUR SENSOR SETUP) ******************************/
#define REMOTE          // Define if wifi and mqtt connnection should be established
#define FLIP_SCREEN     // Uncomment top flip display

/********************************************** WIFI & MQTT INFORMATION *********************************************/
#define location "Location"                        // Temperature sensor location (will be printer on screen)
#define wifi_hostname "hostname"                   // Network hostname
#define wifi_ssid "your_wifi__ssid"                // WIFI SSID
#define wifi_password "your_wifi_password"         // WIFI Password

#define mqtt_device "location"                     // MQTT device
#define mqtt_server "you.mqtt.server"              // MQTT server address.
#define mqtt_user "mqtt_username"                  // MQTT username
#define mqtt_password "mqtt_password"              // MQTT password
#define MQTT_SOCKET_TIMEOUT 120

String mqtt_client_id = mqtt_device;                // MQTT device

/************************************ MQTT TOPICS (change these topics as you wish) *********************************/
#define temperaturepub "temp_sensor/location/temperature"
#define humiditypub "temp_sensor/location/humidity"
#define tempindexpub "temp_sensor/location/temperatureindex"
#define motionpub "temp_sensor/location/motion"
#define presspub  "temp_sensor/location/pressure"


/********************************************* DHT 22 Calibration settings ******************************************/
float temp_offset = 0;
float hum_offset = 0;

/******************************************* Define DHT related settings ********************************************/

#define DHTPIN 12          // (D6)
#define DHTTYPE DHT22      // DHT 22/11 (AM2302), AM2321
DHT dht(DHTPIN, DHTTYPE);

/******************************************* Define OLED related settings ********************************************/
SSD1306 display(0x3c, 0 /*D3*/, 2 /*D4*/);
OLEDDisplayUi ui ( &display );

/**************************************** Define WiFi & MQTT related settings ****************************************/
X509List caCertX509(caCert);          // X.509 parsed CA Cert
WiFiClientSecure espClient;
PubSubClient client(espClient);

unsigned long currentMillis = 60001;  // Set so temperature's read on first scan of program
unsigned long previousMillis = 0;
unsigned long interval = 10000;       // Temperature update interval
float h,t,h2,t2;

void setup_wifi() {
  delay(10);
  WiFi.hostname("");
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.print(wifi_ssid);
  
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 10, "Connecting to " + String(wifi_ssid));
    display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbol : inactiveSymbol);
    display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbol : inactiveSymbol);
    display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbol : inactiveSymbol);
    display.display();
    Serial.print(".");
    counter++;
  }
  
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(60,52, "OK");
  display.display();
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  delay(1500);
}

bool verifytls() {
  bool success = false;
  Serial.print("Verifying TLS connection to ");
  Serial.println(mqtt_server);

  success = espClient.connect(mqtt_server, 8883);

  if (success) {
    Serial.println("TLS connection to MQTT server completed. Certificate & fingerprint valid");
  }
  else {
    Serial.println("TLS connection to MQTT server failed!");
  }
  return (success);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_client_id.c_str(), mqtt_user, mqtt_password)) {
      client.subscribe(temperaturepub);
      client.subscribe(humiditypub);
      client.subscribe(tempindexpub);
      client.subscribe(motionpub);
      client.subscribe(presspub);

      Serial.println("connected");
      Serial.print("Client id: ");
      Serial.println(String(mqtt_client_id));
      Serial.println();
    } 
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void drawDHT(float h, float t, float hic)
{
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0,2, String(location));
  
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(60,20, String(t,1) + " °C");
  
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0,53, "Hum: " + String(h,1) + " %");
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(120,53, "HI: " + String(hic,1) + " °C");
}

void setup() {
  Serial.begin(9600);
  dht.begin();

  ui.setTargetFPS(60);
  ui.setActiveSymbol(activeSymbol);
  ui.setInactiveSymbol(inactiveSymbol);
  ui.init();
  display.init();
  #ifdef FLIP_SCREEN
    display.flipScreenVertically();
  #endif
  
  #ifdef REMOTE
    setup_wifi();
    espClient.setTrustAnchors(&caCertX509);         // Load CA cert into trust store
    espClient.allowSelfSignedCerts();               //. Enable self-signed cert support
    espClient.setFingerprint(mqttCertFingerprint);  // Load SHA1 mqtt cert fingerprint for connection validation
    verifytls();
    mqtt_client_id += String("-") + String(random(0xffff), HEX);
    client.setServer(mqtt_server, 8883);
  #else
    WiFi.mode(WIFI_OFF);
  #endif
}

void loop() {
  char strCh[10];
  String str;
  #ifdef REMOTE
    if (!client.loop()) {
      reconnect();
    }
  #endif
  
  if(currentMillis - previousMillis > interval) {
      previousMillis = currentMillis;
      // Read temperature & humidity
      h = dht.readHumidity();
      t = dht.readTemperature();
      // Check if any reads failed and exit early (to try again).
      if (isnan(h) || isnan(t)) {
        Serial.println("Failed to read from DHT sensor!");
        //Writes previous read values if the read attempt failed
        t=t2; 
        h=h2;
      }
      else { //add offsets, if any
        t = t + temp_offset;
        h = h + hum_offset;
        //Store values encase next read fails
        h2=h; 
        t2=t;
      }

      // Compute heat index in Celsius
      float hic = dht.computeHeatIndex(t, h, false);
      
      str = String(t,1);
      str.toCharArray(strCh,9);
      client.publish(temperaturepub, strCh);
      
      str = String(h,1);
      str.toCharArray(strCh,9);
      client.publish(humiditypub, strCh);
      
      str = String(hic,2);
      str.toCharArray(strCh,9);
      client.publish(tempindexpub, strCh);
      
      client.loop();

      display.clear();
      drawDHT(h,t,hic);
      display.display();
      
      Serial.print("Humidity: ");
      Serial.print(h,1);
      Serial.print(" %\t");
      Serial.print("Temperature: ");
      Serial.print(t,1);
      Serial.print(" °C ");
      Serial.print("Heat index: ");
      Serial.print(hic,1);
      Serial.print(" °C ");
      Serial.println(" in/H2O");
  }
  currentMillis = millis();
}
