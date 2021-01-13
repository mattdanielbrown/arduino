#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <ESPHue.h>
#include "./env.h"

// State variables
byte pirState = LOW;
byte lightState = 0;

unsigned long currentTime;
unsigned long lastRefreshLightState = -REFRESH_LIGHT_STATE_TIMEOUT;

ESP8266WiFiMulti WiFiMulti;
WiFiClient client;

ESPHue myHue = ESPHue(client, HUE_API_KEY, HUE_HOST, 80);

void setup()
{
  Serial.begin(115200);

  pinMode(LED_PIR_GPIO, OUTPUT);
  pinMode(PIR_GPIO, INPUT);

  digitalWrite(LED_PIR_GPIO, HIGH);

  Serial.print("Connecting to Wi-Fi...");

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(WIFI_SSID, WIFI_PASS);

  pinMode(WIFI_LED_GPIO, OUTPUT);
  while (WiFiMulti.run() != WL_CONNECTED)
  {
    digitalWrite(WIFI_LED_GPIO, !digitalRead(WIFI_LED_GPIO));
    Serial.print(".");
    delay(200);
  }

  digitalWrite(WIFI_LED_GPIO, HIGH);

  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.print(WiFi.localIP());

  Serial.println();
}

void refreshLightState()
{
  lightState = myHue.getLightState(HUE_LIGHT_ID);
  Serial.printf("Refreshed light state: %d\n", lightState);
}

void onMotionDetected()
{
  Serial.println("Motion detected!");
  digitalWrite(LED_PIR_GPIO, LOW);

  Serial.printf("Current light state: %d\n", lightState);

  lightState = !lightState;
  myHue.setLightPower(HUE_LIGHT_ID, lightState);
}

void onMotionEnded()
{
  Serial.println("Motion ended!");
  digitalWrite(LED_PIR_GPIO, HIGH);
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
