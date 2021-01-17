#include <Servo.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "./env.h"

#define SERVO_GPIO 2

void setupWifi()
{
  Serial.print("Connecting to Wi-Fi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  pinMode(WIFI_LED_GPIO, OUTPUT);
  while (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(WIFI_LED_GPIO, !digitalRead(WIFI_LED_GPIO));
    Serial.print(".");
    delay(200);
  }

  digitalWrite(WIFI_LED_GPIO, HIGH);

  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin(MDNS_NAME))
  {
    Serial.println("MDNS responder started");
  }
}

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
  <body>\
    <label>degree</label>\
    <input id='degree' type='number' value='0'><br>\
    <label>delay</label>\
    <input id='delay' type='number' value='0'><br>\
    <button id='degreeSend'>Send degree</button>\
    <button id='pushSend'>Send push</button>\
    <script>\
    const fetchBase = { method: 'post', headers: { 'content-type': 'application/x-www-form-urlencoded' } };\
    degreeSend.addEventListener('click', () => {\
      fetch('/api', {... fetchBase, body : `method=degree&degree=${degree.value}` });\
    });\
    pushSend.addEventListener('click', () => {\
      fetch('/api', {... fetchBase, body : `method=push&degree=${degree.value}&delay=${delay.value}` });\
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
      _delay = 1000;
    myservo.write(_degree);
    delay(_delay);
    myservo.write(0);
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
  Serial.begin(115200);
  setupWifi();
  setupServo();
  setupServer();
}

void loop()
{
  server.handleClient();
  MDNS.update();
}
