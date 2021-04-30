#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <ESPHue.h>
#include <ESP8266WebServer.h>

// Comment this
#include "/usr/local/share/arduino.env.h"

#ifndef SERIAL_PORT
#define SERIAL_PORT 115200
#endif

#ifndef WIFI_ENABLED
#define WIFI_ENABLED 0
#define WIFI_SSID ""
#define WIFI_PASS ""
#endif

#ifndef OTA_ENABLED
#define OTA_ENABLED 0
#define OTA_PASSWORD "arduino"
#define OTA_PORT 8266
#endif

#ifndef MDNS_ENABLED
#define MDNS_ENABLED 0
#endif

#ifndef SERVER_ENABLED
#define SERVER_ENABLED 0
#endif

#ifndef HUE_API_KEY
#define HUE_API_KEY ""
#endif

// Led to switch when WiFi is connected
#define WIFI_LED_GPIO 2

// Identify the GPIO pin connected to the PIR sensor
#define PIR_GPIO 4

// LED used to debug PIR on/off
#define LED_PIR_GPIO 16

// ID of HUE group lights
#define HUE_GROUP_ID 3

// How often the system should pull the correct data from HUE backend
#define REFRESH_LIGHT_STATE_TIMEOUT 10000

// Name of this board on the local network
#define MDNS_NAME "PirHue"

// If set to 0, the board won't perform any action
byte ENABLED = 1;

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

#if SERVER_ENABLED == 1
ESP8266WebServer server(80);

void serverHandleRoot()
{
  String html = "<html>\
  <head>\
    <meta name='viewport' content='width=device-width, initial-scale=1'>\
  </head>\
  <body>\
    <label>Enabled</label>\
    <input id='enabled' type='checkbox'";
  html += (ENABLED == 1 ? " checked " : "");
  html += "' />\
    <script>\
    const fetchBase = { method: 'post', headers: { 'content-type': 'application/x-www-form-urlencoded' } };\
    enabled.addEventListener('change', () => {\
      fetch('/api', {... fetchBase, body : `method=enable&value=${Number(enabled.checked)}` });\
    });\
    </script>\
  </body>\
  </html>";
  server.send(200, "text/html", html);
}

void serverHandleApi()
{
  if (server.method() != HTTP_POST)
  {
    server.send(405, "application/json", "{\"error\":\"HTTP Method Not Allowed\"}");
    return;
  }

  const String method = server.arg("method");

  if (method.equals("enable"))
  {
    int _value = server.arg("value").toInt();
    ENABLED = _value;

    Serial.print("New value for enabled: ");
    Serial.println(ENABLED);

    String json = "{\"\"success\":true,\"value\":";
    json += (ENABLED == 1 ? "true" : "false");
    json += "}";

    server.send(200, "application/json", json);
    return;
  }

  server.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}");
}

void setupServer()
{
  server.on("/", serverHandleRoot);
  server.on("/api", serverHandleApi);
  server.begin();
  Serial.println("HTTP server started");
}
#endif

// State variables
byte pirState = LOW;
byte lightState = 0;

unsigned long currentTime;
unsigned long lastRefreshLightState = -REFRESH_LIGHT_STATE_TIMEOUT;

ESPHue myHue = ESPHue(wifiClient, HUE_API_KEY, "philips-hue.local", 80);

void refreshLightState()
{
  lightState = myHue.getGroupState(HUE_GROUP_ID);
  Serial.printf("Light state from HUE backend: %d\n", lightState);
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
  setupWifi();
#if MDNS_ENABLED == 1
  setupMDNS();
#endif
#if OTA_ENABLED == 1
  setupOTA();
#endif
#if SERVER_ENABLED == 1
  setupServer();
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

  if (ENABLED)
  {
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

#if SERVER_ENABLED == 1
  server.handleClient();
#endif
#if OTA_ENABLED == 1
  ArduinoOTA.handle();
#endif
#if MDNS_ENABLED == 1
  MDNS.update();
#endif
}
