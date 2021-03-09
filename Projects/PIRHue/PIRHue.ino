#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <ESPHue.h>

// Comment this
#include "../../env.h"

#ifndef SERIAL_PORT
#define SERIAL_PORT 115200
#endif

#ifndef WIFI_ENABLED
#define WIFI_ENABLED 0
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASS
#define WIFI_PASS ""
#endif

#ifndef WIFI_LED_GPIO
#define WIFI_LED_GPIO 16
#endif

#ifndef OTA_ENABLED
#define OTA_ENABLED 0
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD "arduino"
#endif

#ifndef OTA_PORT
#define OTA_PORT 8266
#endif

#ifndef MDNS_ENABLED
#define MDNS_ENABLED 0
#endif

#ifndef HUE_API_KEY
#define HUE_API_KEY ""
#endif

#ifndef HUE_HOST
#define HUE_HOST ""
#endif

#define LED_PIR_GPIO 16
#define PIR_GPIO 4
#define HUE_GROUP_ID 3
#define REFRESH_LIGHT_STATE_TIMEOUT 10000
#define MDNS_NAME "pirhue"

WiFiClient wifiClient;

#if OTA_ENABLED == 1
void setupOTA()
{
  ArduinoOTA.setPort(OTA_PORT);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
    {
      Serial.println("Auth Failed");
    }
    else if (error == OTA_BEGIN_ERROR)
    {
      Serial.println("Begin Failed");
    }
    else if (error == OTA_CONNECT_ERROR)
    {
      Serial.println("Connect Failed");
    }
    else if (error == OTA_RECEIVE_ERROR)
    {
      Serial.println("Receive Failed");
    }
    else if (error == OTA_END_ERROR)
    {
      Serial.println("End Failed");
    }
  });

  ArduinoOTA.begin();
}
#endif

#if WIFI_ENABLED == 1
void setupWifi()
{
  Serial.print("Connecting to Wi-Fi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  pinMode(WIFI_LED_GPIO, OUTPUT);
  while (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(WIFI_LED_GPIO, !digitalRead(WIFI_LED_GPIO));
    delay(100);
  }

  digitalWrite(WIFI_LED_GPIO, HIGH);

  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
}
#endif

#if MDNS_ENABLED == 1
void setupMDNS()
{
  if (MDNS.begin(MDNS_NAME))
  {
    MDNS.addService("http", "tcp", 80);
    Serial.println("MDNS responder started");
  }
}
#endif

// State variables
byte pirState = LOW;
byte lightState = 0;

unsigned long currentTime;
unsigned long lastRefreshLightState = -REFRESH_LIGHT_STATE_TIMEOUT;

ESPHue myHue = ESPHue(wifiClient, HUE_API_KEY, HUE_HOST, 80);

void refreshLightState()
{
  lightState = myHue.getGroupState(HUE_GROUP_ID);
  Serial.printf("Refreshed light state: %d\n", lightState);
}

void onMotionDetected()
{
  Serial.println("Motion detected!");
  digitalWrite(LED_PIR_GPIO, LOW);

  Serial.printf("Current light state: %d\n", lightState);

  lightState = !lightState;
  myHue.setGroupPower(HUE_GROUP_ID, lightState);
}

void onMotionEnded()
{
  Serial.println("Motion ended!");
  digitalWrite(LED_PIR_GPIO, HIGH);
}

void setupPIR()
{
  pinMode(LED_PIR_GPIO, OUTPUT);
  pinMode(PIR_GPIO, INPUT);
  digitalWrite(LED_PIR_GPIO, HIGH);
}

// Setup & Loop

void setup()
{
#if SERIAL_PORT > 0
  Serial.begin(115200);
#endif
#if WIFI_ENABLED == 1
  setupWifi();
#endif
#if MDNS_ENABLED == 1
  setupMDNS();
#endif
#if OTA_ENABLED == 1
  setupOTA();
#endif
  setupPIR();
}

void loop()
{
  currentTime = millis();

  if (lastRefreshLightState + REFRESH_LIGHT_STATE_TIMEOUT < currentTime)
  {
    refreshLightState();
    lastRefreshLightState = currentTime;
  }

  if (digitalRead(PIR_GPIO) == HIGH)
  {
    if (pirState == LOW)
    {
      onMotionDetected();
      pirState = HIGH;
    }
  }
  else
  {
    if (pirState == HIGH)
    {
      onMotionEnded();
      pirState = LOW;
    }
  }
}
