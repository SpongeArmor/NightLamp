#include <Adafruit_NeoPixel.h>
#include "AuthenticationInfo.h"
#include <Thread.h>
#include <ThreadController.h>
#include <string.h>

/*Web Updater Related*/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;


/*LED Control*/
#define CENTER_PIN  D2
#define SIDES_PIN   D1

#define CENTER_NUMPIXELS 9
#define SIDES_NUMPIXELS 10

Adafruit_NeoPixel center(CENTER_NUMPIXELS, CENTER_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel sides(SIDES_NUMPIXELS, SIDES_PIN, NEO_GRB + NEO_KHZ800);

#define DELAYVAL 500

void setAll_Sides(char, char, char, bool);
void setAll_Center(char, char, char, bool);
void colorFade_center(uint8_t, uint8_t, uint8_t);
void colorFade_sides(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
void setupNetwork();
void setupOTA();
void mqtt_connection(const char* topics[]);
void publisher();
void sides_subscriber(String, String);
void center_subscriber(String, String);
void left_subscriber(String, String);
void right_subscriber(String, String);


/*MQTT*/
#include "PubSubClient.h"
#include "PubSubClientTools.h"
const char* topics[] = {"/nightLamp/sides",
                        "/nightLamp/left",
                        "/nightLamp/right",
                        "/nightLamp/center",
                        "/nightLamp/debug"};

#define TOPICS_COUNT  4
#define SIDES         topics[0]
#define CENTER        topics[3]
#define LEFT          topics[1]        
#define RIGHT         topics[2]
#define DEBUG         topics[4]

uint8_t startR, startG, startB;

WiFiClient nightLamp;   //DO NOT use different object idetifiers. The WiFi object MUST BE passed to the client.
PubSubClient mqttclient(nightLamp);
PubSubClientTools mqtt(mqttclient);

ThreadController threadControl = ThreadController();
Thread thread = Thread();

int value = 0;
const String s = "";

void setup() {
  center.begin();
  sides.begin();

  center.clear();
  sides.clear();
  center.show();
  sides.show();

  setupNetwork();

  setupOTA();

  mqttclient.setServer(mqtt_server, mqtt_port);

  mqtt_connection(topics);

  thread.onRun(publisher);
  thread.setInterval(10000);
  threadControl.add(&thread);
}

void loop() {
  httpServer.handleClient();
  MDNS.update();

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    colorFade_sides(50, 0, 0, 0, 9);
    Serial.println("WiFi disconnected, retrying.");
    WiFi.begin(ssid, password);
    delay(1000);
    colorFade_sides(0, 0, 0, 0, 9);
    delay(1000);
  }

  if (!mqttclient.connected()) {    //MQTT client handling
    mqtt_connection(topics);
  }

  mqttclient.loop();
  threadControl.run();
}

void setAll_Center(char R, char G, char B, bool fade){
  if(fade){
    colorFade_center(R, G, B);
  } else {
    center.fill(10*center.Color(R, G, B));
    center.show();
  }
  
}

void setAll_Sides(char R, char G, char B, bool fade){
  if(fade){
    colorFade_sides(R, G, B, 0, 9);
  } else {
    sides.fill(sides.Color(R, G, B));
    sides.show(); 
  }
}

void colorFade_center(uint8_t r, uint8_t g, uint8_t b) {
  bool start = 1;
  uint8_t startR, startG, startB;
  uint32_t startColor;
  while(start == 1){
    for(uint8_t i = 0; i < 9; i++) {
      
      startColor = center.getPixelColor(i); // get the current colour
      startB = startColor & 0xFF;
      startG = (startColor >> 8) & 0xFF;
      startR = (startColor >> 16) & 0xFF;  // separate into RGB components

      if ((startR != r) || (startG != g) || (startB != b)){  // while the curr color is not yet the target color
        if (startR < r) startR++; else if (startR > r) startR--;  // increment or decrement the old color values
        if (startG < g) startG++; else if (startG > g) startG--;
        if (startB < b) startB++; else if (startB > b) startB--;
        center.setPixelColor(i, startR, startG, startB);  // set the color
        center.show();
        delay(1);  // add a delay if its too fast
      } else {
        start = 0;
      }
  }
  }
}

void colorFade_sides(uint8_t r, uint8_t g, uint8_t b, uint8_t from, uint8_t to) {
  bool start = 1;
  uint8_t startR=0, startG=0, startB=0;
  uint32_t startColor=0;
  while(start == 1){
    for(uint8_t i = from; i <= to; i++) {
      
      startColor = sides.getPixelColor(i); // get the current colour
      startB = startColor & 0xFF;
      startG = (startColor >> 8) & 0xFF;
      startR = (startColor >> 16) & 0xFF;  // separate into RGB components

      if ((startR != r) || (startG != g) || (startB != b)){  // while the curr color is not yet the target color
        if (startR < r) startR++; else if (startR > r) startR--;  // increment or decrement the old color values
        if (startG < g) startG++; else if (startG > g) startG--;
        if (startB < b) startB++; else if (startB > b) startB--;
        sides.setPixelColor(i, startR, startG, startB);  // set the color
        sides.show();
        delay(1);  // add a delay if its too fast
      } else {
        start = 0;
      }
  }
  }
}

void publish_message(const char* topic, char* message) {
  mqttclient.publish(topic, message, 1);
  mqttclient.loop();
}

void setupNetwork(){
  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting Sketch...");

  WiFi.mode(WIFI_AP_STA);
  IPAddress ip(192, 168, 1, 50); //change for a static IP. Comment these 3 lines out if you want DHCP  to handle IP assignment
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.config(ip, gateway, subnet);
  WiFi.hostname("NightLamp");
  // WiFi.begin(ssid, password);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    colorFade_sides(50, 0, 0, 0, 9);
    WiFi.begin(ssid, password);
    Serial.println("Attempting WiFi Connection..");
    delay(1000);
    colorFade_sides(0, 0, 0, 0, 9);
    delay(1000);
  }

  colorFade_sides(0, 50, 0, 0, 9);
  delay(1000);
  colorFade_sides(0, 0, 0, 0, 9);
  
  Serial.print("Wifi Connected. IP : ");
  Serial.println(WiFi.localIP());
}

void setupOTA(){
  MDNS.begin(host);

  httpUpdater.setup(&httpServer, update_path, update_username, update_password);
  httpServer.begin();

  MDNS.addService("http", "tcp", 80);
  Serial.println("HTTPUpdateServer ready! Open IP/update page for OTA uploads.");
}

void sides_subscriber(String topic, String message){
  Serial.println(s + "Message arrived in [" + topic + "] " + message);
  unsigned long hex = strtoul(message.c_str()+1, NULL, 16);
  Serial.println(hex);

  startB = hex & 0xFF;
  startG = (hex >> 8) & 0xFF;
  startR = (hex >> 16) & 0xFF;

  Serial.print("R : ");
  Serial.println(startR);
  Serial.print("G : ");
  Serial.println(startG);
  Serial.print("B : ");
  Serial.println(startB);
  // colorFade_sides(startR, startG, startB, 0, 9);
  setAll_Sides(startR, startG, startB, true);
  mqtt.publish(LEFT, message);
  mqtt.publish(RIGHT, message);
}

void center_subscriber(String topic, String message){
  Serial.println(s + "Message arrived in [" + topic + "] " + message);
  unsigned long hex = strtoul(message.c_str()+1, NULL, 16);
  Serial.println(hex);

  startB = hex & 0xFF;
  startG = (hex >> 8) & 0xFF;
  startR = (hex >> 16) & 0xFF;
  
  Serial.print("R : ");
  Serial.println(startR);
  Serial.print("G : ");
  Serial.println(startG);
  Serial.print("B : ");
  Serial.println(startB);
  // colorFade_centerset(startR, startG, startB);
  setAll_Center(startR, startG, startB, true);
}

void left_subscriber(String topic, String message){
  Serial.println(s + "Message arrived in [" + topic + "] " + message);
  unsigned long hex = strtoul(message.c_str()+1, NULL, 16);
  Serial.println(hex);

  startB = hex & 0xFF;
  startG = (hex >> 8) & 0xFF;
  startR = (hex >> 16) & 0xFF;
  
  Serial.print("R : ");
  Serial.println(startR);
  Serial.print("G : ");
  Serial.println(startG);
  Serial.print("B : ");
  Serial.println(startB);
  colorFade_sides(startR, startG, startB, 5, 9);
}

void right_subscriber(String topic, String message){
  Serial.println(s + "Message arrived in [" + topic + "] " + message);
  unsigned long hex = strtoul(message.c_str()+1, NULL, 16);
  Serial.println(hex);

  startB = hex & 0xFF;
  startG = (hex >> 8) & 0xFF;
  startR = (hex >> 16) & 0xFF;
  
  Serial.print("R : ");
  Serial.println(startR);
  Serial.print("G : ");
  Serial.println(startG);
  Serial.print("B : ");
  Serial.println(startB);
  colorFade_sides(startR, startG, startB, 0, 4);
}

void mqtt_connection(const char* topics[]){
  bool attempt = 0;
  while(!mqttclient.connected()){
    attempt = 1;
    Serial.println("Attempting MQTT Connection..");
    mqttclient.connect("NighLamp", mqtt_username, mqtt_password);
    delay(3000);
  }

  if(attempt){
    Serial.println("Subscribing to Topics");
    mqtt.subscribe(SIDES, sides_subscriber);
    mqtt.subscribe(CENTER, center_subscriber);
    mqtt.subscribe(LEFT, left_subscriber);
    mqtt.subscribe(RIGHT, right_subscriber);
    Serial.println("Subscribed.");
    mqtt.publish(DEBUG, "Ready!", 0);
  }
}

void publisher() {
  mqtt.publish(DEBUG, "Night Lamp is up.", 0);
}

