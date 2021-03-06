/* Copyright 2017 Duncan Law (mrdunk@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <PubSubClient.h>      // Include "PubSubClient" library.
#include <mdns.h>              // Include "esp8266_mdns" library.

#include "ESP8266httpUpdate.h"
#include "FS.h"

#include "src/devices.h"
#include "src/mqtt.h"
#include "src/ipv4_helpers.h"
#include "src/secrets.h"
#include "src/mdns_actions.h"
#include "src/host_attributes.h"
#include "src/config.h"
#include "src/http_server.h"
#include "src/serve_files.h"


Config config = {
  "",
  {0,0,0,0},  // Null IP address means use DHCP.
  {255,255,255,0},
  {0,0,0,0},
  {0,0,0,0},
  1883,
  "homeautomation/+",
  "homeautomation/0",
  {},
  "192.168.192.54",
  "/",
  8000,
  "",
  0,
  "",
  "",
};

// Global to track whether access to configuration WebPages should be allowed.
int allow_config;

// Large buffer to be used by MDns and HttpServer.
byte buffer[BUFFER_SIZE];

// mDNS
MdnsLookup brokers(QUESTION_SERVICE);
mdns::MDns my_mdns(NULL,
                   NULL,
                   [](const mdns::Answer* answer){brokers.ParseMDnsAnswer(answer);},
                   buffer,
                   BUFFER_SIZE);

// MQTT
WiFiClient wifiClient;
Mqtt mqtt(wifiClient, &brokers);
void mqttCallback(const char* topic, const byte* payload, const unsigned int length){
  mqtt.callback(topic, payload, length);
}

// IO
Io io(&mqtt);

// Web page configuration interface.
HttpServer http_server((char*)buffer, BUFFER_SIZE, &config, &brokers,
                       &my_mdns, &mqtt, &io, &allow_config);


void setPullFirmware(bool pull){
	bool result = SPIFFS.begin();
  if(!result){
		Serial.println("Unable to use SPIFFS.");
    SPIFFS.end();
    return;
  }
	
  if(pull){
    // Create a file to indicate we should pull new firmware from server after reboot.
    File file = SPIFFS.open("/pullFirmware.flag", "w");
    if (!file) {
      Serial.println("file creation failed");
      SPIFFS.end();
      return;
    }
    file.close();
  } else {
    // Remove file, indicating we should not pull new firmware from server after reboot.
    SPIFFS.remove("/pullFirmware.flag");
  }
  SPIFFS.end();
}

bool testPullFirmware(){
	bool result = SPIFFS.begin();
  if(!result){
		Serial.println("Unable to use SPIFFS.");
    SPIFFS.end();
    return false;
  }

  File file = SPIFFS.open("/pullFirmware.flag", "r");
  if(file){
    // File exists so we should pull firmware.
    file.close();
    SPIFFS.end();
    return true;
  }
  SPIFFS.end();
  return false;
}

// If we boot with the config.pull_firmware bit set in flash we should pull new firmware
// from an HTTP server.
bool pullFirmware(){
  //config.pull_firmware = false;
  //Persist_Data::Persistent<Config> persist_config(&config);
  //persist_config.writeConfig();
  setPullFirmware(false);

  for(int tries = 0; tries < UPLOAD_FIRMWARE_RETRIES; tries++){
    ESPhttpUpdate.rebootOnUpdate(false);
    const String uri(String(config.firmwaredirectory) + String("firmware.bin"));
    t_httpUpdate_return ret = ESPhttpUpdate.update(config.firmwarehost,
                                                   config.firmwareport,
                                                   uri);

    switch(ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s", 
            ESPhttpUpdate.getLastError(),
            ESPhttpUpdate.getLastErrorString().c_str());
        Serial.println();
        break;

      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;

      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        delay(100);
        return true;
    }
    Serial.println("Retry...");
    delay(1000);
  }
  Serial.println("Giving up firmware pull.");
  return false;
}


void setup_network(void) {
  //Serial.setDebugOutput(true);
 
  if(WiFi.SSID() != ssid || WiFi.psk() != pass){
    Serial.println("Reassigning WiFi username and password.");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
  }
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
 
  if(config.ip != IPAddress(0,0,0,0) && config.subnet != IPAddress(0,0,0,0)){
    WiFi.config(config.ip, config.gateway, config.subnet);
  }

  // Wait for connection
  int timer = RESET_ON_CONNECT_FAIL * 100;
  while (WiFi.status() != WL_CONNECTED){
    delay(10);
    if(timer % 100 == 0){
      Serial.print(".");
    }
    if(timer-- == 0){
      timer = RESET_ON_CONNECT_FAIL;
      ESP.reset();
      Serial.println();
    }
  }
  Serial.println("");
  Serial.print("Connected to: ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if(!testPullFirmware()){
    brokers.InsertManual("broker_hint", config.brokerip, config.brokerport);
    brokers.RegisterMDns(&my_mdns);
  }
}

void configInterrupt(){
  Serial.println("configInterrupt");
  allow_config = 100;
}

void setup(void) {
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.println("Reset.");
  Serial.println();

  if(config.load("/config.cfg", true)){
    config.load("/config.cfg");
	}

  if(testPullFirmware()){
    Serial.println("Pull Firmware mode!!");
  } else {
    // Do IO setup early in case an IO pin needs to hold power to esp8266 on.
    pinMode(config.enableiopin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(config.enableiopin), configInterrupt, CHANGE);
    io.registerCallback([]() {io.inputCallback();});  // Inline callback function.
    io.setup();

    if (strlen(config.hostname) == 0){
      uint8_t mac[6];
      WiFi.macAddress(mac);
      String hostname = "esp8266_" + macToStr(mac);
      SetHostname(hostname.c_str());
    }

    mqtt.registerCallback(mqttCallback);

    allow_config = 0;
  }
}

void loop(void) {
  if (WiFi.status() != WL_CONNECTED) {
    setup_network();
  }

  if(testPullFirmware()){
    bool result = pullFirmware();
    if(result){
			Serial.println("Upgrade successful.");
    }
		ESP.reset();
  } else {
    mqtt.loop();
    io.loop();
    my_mdns.loop();
    http_server.loop();
  }
}
