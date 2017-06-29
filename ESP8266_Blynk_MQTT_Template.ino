#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <BlynkSimpleEsp8266.h>
#include <WiFiManager.h>
#include <EEPROM.h>

#define PIN_SETUP    D5   // setting this pin LOW erases EEPROM and forces WiFi config mode
#define PORT_SPEED   9600 // speed to use for the serial monitor

// Constants
// This is the access point to look for when hitting the config web server from your phone/PC
#define AP_NAME "Blynk_Project_Name"
// change salt to force settings to be re-written into EEPROM
#define EEPROM_SALT 0 // change this to force new settings in EEPROM
#define mqttClientName "Some-ESP8266-Client-Name" // the client ID provided to the MQTT server

// these are the defaults to try connecting with
typedef struct {
  char  bootState[4]      = "off";
  char  blynkToken[33]    = "e5ed22bfca2a499491e0bf26c17d0dc2"; // replace w/your own
  char  blynkServer[33]   = "blynk-cloud.com";
  char  blynkPort[6]      = "8442";
  char  mqttHost[33]      = "broker.hivemq.com"; // great for testing (only!)
  int   mqttPort          = 1883;
  char  mqttInTopic[33]   = "ESP8266MQTT/test topic"; // using same topic for in and out in demo
  char  mqttOutTopic[33]  = "ESP8266MQTT/test topic"; // so we will receive the messages we send
  int   salt              = EEPROM_SALT;
} WMSettings;

WMSettings settings;
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;
bool shouldSaveConfig = false;

void setup() {
  const char *apName = AP_NAME;
  WiFiManager wifiManager;

  // Begin app setup before WiFi

  // End app setup before WiFi
  
  Serial.begin(PORT_SPEED);

  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setMinimumSignalQuality(50);

  EEPROM.begin(512);
  EEPROM.get(0, settings);
  EEPROM.end();

  if (settings.salt != EEPROM_SALT) {
    Serial.println("Invalid settings in EEPROM, trying with defaults");
    WMSettings defaults;
    settings = defaults;
    shouldSaveConfig = true;
  }

  WiFiManagerParameter custom_blynk_token("blynk-token", "<br/>Blynk Token:<br/>", settings.blynkToken, 33);
  wifiManager.addParameter(&custom_blynk_token);

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  if (digitalRead(PIN_SETUP) == LOW) { // force setup web server
    Serial.println("PIN_SETUP is LOW, erasing previous WiFi info to force config mode");

    WiFi.disconnect();

    delay(1000);
  }

  if (!wifiManager.autoConnect(apName)) { //reset and try again, or maybe put it to deep sleep
    Serial.println("Auto connect failed, resetting and delaying to try again");
    ESP.reset();
    delay(5000);
  }

  Serial.print("Connected to access point: ");
  Serial.println(wifiManager.getSSID());

  // If you got here, you connected one way or another

  if (shouldSaveConfig) {
    // write out config to EEPROM
    Serial.println("Saving configuration to EEPROM");
    strcpy(settings.blynkToken, custom_blynk_token.getValue());

    EEPROM.begin(512);
    EEPROM.put(0, settings);
    EEPROM.end();
  }

  // Setup MQTT connection
  client.setServer(settings.mqttHost, settings.mqttPort);
  client.setCallback(mqttCallback);
  mqttReconnect(); // connect to MQTT server

  // Start up Blynk connection
  Serial.println("Starting Blynk config");
  Blynk.config(settings.blynkToken, settings.blynkServer, atoi(settings.blynkPort));

  // Begin app setup after WiFi

  // End app setup after WiFi

  Serial.println("Setup complete. Entering main loop");
}

void loop() {
  long now = millis();

  if (now - lastMsg > 2000) {
    lastMsg = now;
    ++value;
    snprintf (msg, 75, "ESP8266 MQTT Demo #%ld", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish(settings.mqttOutTopic, msg);
  }

  if (!client.connected()) {
    // when reconnecting, we are not receiving commands from Blynk
    mqttReconnect();
  }
  client.loop();
  
  Blynk.run();
  
}

