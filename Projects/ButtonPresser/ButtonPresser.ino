#include <Servo.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

#if __has_include("../../env.h")
#include "../../env.h"
#endif

#ifndef SERIAL_PORT
#define SERIAL_PORT 0
#endif

#ifndef WIFI_ENABLED
#define WIFI_ENABLED 0
#define WIFI_SSID ""
#define WIFI_PASS ""
#define WIFI_LED_GPIO 2
#endif

#ifndef OTA_ENABLED
#define OTA_ENABLED 0
#define OTA_PASSWORD "arduino"
#define OTA_PORT 8266
#endif

#ifndef MDNS_ENABLED
#define MDNS_ENABLED 0
#define MDNS_NAME "buttonpresser"
#endif

#define SERVO_GPIO 2

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

Servo myservo;
void setupServo()
{
  myservo.attach(SERVO_GPIO);
  myservo.write(0);
}

ESP8266WebServer server(80);

void serverHandleRoot()
{
  server.send(200, "text/html", "<html>\
  <head>\
    <meta name='viewport' content='width=device-width, initial-scale=1'>\
  </head>\
  <body>\
    <label>degree</label>\
    <input id='degree' type='number' value='45'><br>\
    <label>delay</label>\
    <input id='delay' type='number' value='200'><br>\
    <label>times</label>\
    <input id='times' type='number' value='1'><br>\
    <label>delaytimes</label>\
    <input id='delaytimes' type='number' value='1000'><br>\
    <button id='degreeSend'>Send degree</button>\
    <button id='pushSend'>Send push</button>\
    <script>\
    const fetchBase = { method: 'post', headers: { 'content-type': 'application/x-www-form-urlencoded' } };\
    degreeSend.addEventListener('click', () => {\
      fetch('/api', {... fetchBase, body : `method=degree&degree=${degree.value}` });\
    });\
    pushSend.addEventListener('click', () => {\
      fetch('/api', {... fetchBase, body : `method=push&degree=${degree.value}&delay=${delay.value}&times=${times.value}&delaytimes=${delaytimes.value}` });\
    });\
    </script>\
  </body>\
</html>");
}

void serverHandleApi()
{
  if (server.method() != HTTP_POST)
  {
    server.send(405, "text/plain", "HTTP Method Not Allowed");
    return;
  }

  const String method = server.arg("method");
  int _degree = server.arg("degree").toInt();
  int _delay = server.arg("delay").toInt();
  int _times = server.arg("times").toInt();
  int _delaytimes = server.arg("delaytimes").toInt();

  if (method.equals("degree"))
  {
    if (!_degree)
    {
      server.send(400, "text/plain", "Degree value not provided");
      return;
    }
    myservo.write(_degree);
    server.send(200, "text/plain", "OK");
    return;
  }

  if (method.equals("push"))
  {
    if (!_degree)
      _degree = 45;
    if (!_delay)
      _delay = 200;
    if (!_times)
      _times = 1;
    if (!_delaytimes)
      _delaytimes = 1000;

    for (int i = 0; i < _times; i++)
    {
      myservo.write(_degree);
      delay(_delay);
      myservo.write(0);
      if ((i + 1) != _times)
      {
        delay(_delaytimes);
      }
    }

    server.send(200, "text/plain", "OK");
    return;
  }

  server.send(405, "text/plain", "Method Not Allowed");
}

void setupServer()
{
  server.on("/", serverHandleRoot);
  server.on("/api", serverHandleApi);
  server.begin();
  Serial.println("HTTP server started");
}

void setup()
{
#if SERIAL_PORT > 0
  Serial.begin(115200);
#endif
#if WIFI_ENABLED == 1
  setupWifi();
#endif
#if MDNS_ENABLEDD == 1
  setupMDNS();
#endif
#if OTA_ENABLED == 1
  setupOTA();
#endif
  setupServo();
  setupServer();
}

void loop()
{
#if OTA_ENABLED == 1
  ArduinoOTA.handle();
#endif
#if MDNS_ENABLED == 1
  MDNS.update();
#endif
  server.handleClient();
}
