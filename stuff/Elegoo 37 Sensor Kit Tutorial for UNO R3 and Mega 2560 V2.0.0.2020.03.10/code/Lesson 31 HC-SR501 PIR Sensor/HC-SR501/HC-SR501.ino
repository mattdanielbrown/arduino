#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

const char* WIFI_SSID = "Otto Home - 2GHz";
const char* WIFI_PASS = "rispondiunoodue";
const String HUE_URL =  "http://10.0.0.251/api/koAmbhqvs3oHkKTDhNkDNgaCEPc1PnhHLjVc-6fD/lights/2";

int inputPin = 4;               // choose the input pin (for PIR sensor)
int pirState = LOW;             // we start, assuming no motion detected
bool lightState = false;
 
void setup() {
  Serial.begin(115200);
  
  pinMode(LED_BUILTIN, OUTPUT);      // declare LED as output
  pinMode(inputPin, INPUT);     // declare sensor as input
   
  Serial.print("Connecting to Wi-Fi...");
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
}

bool getCurrentLightState() {
  WiFiClient client;
  HTTPClient http;
  
  http.begin(client, HUE_URL);
  int httpCode = http.GET();
  if (httpCode > 0) {
    const size_t capacity = 4*JSON_OBJECT_SIZE(2) + 2*JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(7) + JSON_OBJECT_SIZE(13) + 620;
    DynamicJsonDocument doc(capacity);
    String payload = http.getString();    
    Serial.println("[getCurrentLightState] response");
    Serial.println(httpCode);
    Serial.println(payload);
    deserializeJson(doc, payload);
    http.end();
    return doc["state"]["on"];
  }

  return NULL;
}

void onMotionDetected() {
  Serial.println("Motion detected!");
  digitalWrite(LED_BUILTIN, HIGH);  // turn LED ON

  bool currentLightState = getCurrentLightState();
  if (currentLightState == NULL) {
    Serial.println("Unable to get current light state, assuming previous value");
  } else {
    lightState = currentLightState;
  }

  // Flip the light state
  lightState = !lightState;

  WiFiClient client;
  HTTPClient http;
  http.begin(client, HUE_URL + "/state");
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = 0;
  if (lightState) {
    httpResponseCode = http.PUT("{\"on\":true}");
  } else {
    httpResponseCode = http.PUT("{\"on\":false}");
  }
  
  if (httpResponseCode > 0) {
    const String payload = http.getString();
    Serial.print("Server responded: ");
    Serial.println(httpResponseCode);
    Serial.println(payload);
   } else {
    Serial.print("Error while sending request");
   }
   http.end();
}

void onMotionEnded() {
  Serial.println("Motion ended!");
  digitalWrite(LED_BUILTIN, LOW); // turn LED OFF
}
 
void loop(){
  if (digitalRead(inputPin) == HIGH) {            // check if the input is HIGH
    if (pirState == LOW) {
      onMotionDetected();
      pirState = HIGH;
    }
  } else {
    if (pirState == HIGH){
      onMotionEnded();
      pirState = LOW;
    }
  }
}
