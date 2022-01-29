#include <Servo.h>

// Comment this
#include "/usr/local/share/arduino.env.h"

#define WIFI_LED_GPIO 2   // Led to switch when WiFi is connected
#define SERVO_GPIO 14     // Identify the GPIO pin connected to the servo
#define SERVO_LED_GPIO 16 // Identify the LED indicating an operation
#define MDNS_NAME "Nisse" // Name of this board on the local network

Servo myservo;
void setupServo()
{
  myservo.attach(SERVO_GPIO);
  myservo.write(0);

  pinMode(SERVO_LED_GPIO, OUTPUT);
  digitalWrite(SERVO_LED_GPIO, HIGH);
}

#ifdef OTA_ENABLED
#include <ArduinoOTA.h>
void setupOTA()
{
  ArduinoOTA.setPort(OTA_PORT);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });

  ArduinoOTA.onError([](ota_error_t error)
                     {
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

#ifdef WIFI_ENABLED
#include <ESP8266WiFi.h>

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

#ifdef MDNS_ENABLED
#include <ESP8266mDNS.h>

void setupMDNS()
{
  if (MDNS.begin(MDNS_NAME))
  {
    MDNS.addService("http", "tcp", 80);
    Serial.println("MDNS responder started");
  }
}
#endif

#ifdef SERVER_ENABLED
#include <ESP8266WebServer.h>

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
    server.send(405, "application/json", "{\"error\":\"HTTP Method Not Allowed\"}");
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
      server.send(400, "application/json", "{\"error\":\"Degree value not provided\"}");
      return;
    }
    myservo.write(_degree);
    server.send(200, "application/json", "{\"success\":true}");
    return;
  }

  if (method.equals("push"))
  {
    if (!_degree)
      _degree = 30;
    if (!_delay)
      _delay = 600;
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

    server.send(200, "application/json", "{\"success\":true}");
    return;
  }

  server.send(404, "application/json", "{\"error\":\"Method not found\"}");
}

void serverHandleApiWithDebugLed()
{
  digitalWrite(SERVO_LED_GPIO, LOW);
  serverHandleApi();
  digitalWrite(SERVO_LED_GPIO, HIGH);
}

void setupServer()
{
  server.on("/", serverHandleRoot);
  server.on("/api", serverHandleApiWithDebugLed);
  server.begin();
  Serial.println("HTTP server started");
}
#endif

void setup()
{
#ifdef SERIAL_PORT
  Serial.begin(115200);
#endif
#ifdef WIFI_ENABLED
  setupWifi();
#endif
#ifdef MDNS_ENABLED
  setupMDNS();
#endif
#ifdef OTA_ENABLED
  setupOTA();
#endif
#ifdef SERVER_ENABLED
  setupServer();
#endif
  setupServo();
}

void loop()
{
#ifdef OTA_ENABLED
  ArduinoOTA.handle();
#endif
#ifdef MDNS_ENABLED
  MDNS.update();
#endif
#ifdef SERVER_ENABLED
  server.handleClient();
#endif
}
