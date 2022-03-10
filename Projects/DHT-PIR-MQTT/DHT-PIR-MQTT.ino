
// Comment this
#include "/usr/local/share/arduino.env.h"

#define STATUS_PIN 16
#define DEBUG_PIN 2
#define MDNS_NAME "DHTPIRSensor" 

#define DHT_PIN 16
#define PIR_PIN 4

#define DHT_POLLING_TIMEOUT	5000

unsigned long currentTime;

#ifdef WIFI_ENABLED
#include <ESP8266WiFi.h>
WiFiClient wifiClient;

void setupWifi()
{
	Serial.print("Connecting to Wi-Fi...");

	WiFi.mode(WIFI_STA);
	WiFi.begin(WIFI_SSID, WIFI_PASS);
	WiFi.setAutoConnect(true);
	WiFi.setAutoReconnect(true);
	WiFi.persistent(true);

	pinMode(DEBUG_PIN, OUTPUT);
	while (WiFi.status() != WL_CONNECTED)
	{
		digitalWrite(DEBUG_PIN, !digitalRead(DEBUG_PIN));
		delay(100);
	}

	digitalWrite(DEBUG_PIN, HIGH);

	Serial.println();
	Serial.print("Connected! IP address: ");
	Serial.println(WiFi.localIP());
}

void loopWifi() {

}
#endif


#ifdef MQTT_ENABLED
#include <PubSubClient.h>

PubSubClient pubSubClient(wifiClient);

void setupMQTT() {
	pubSubClient.setServer(MQTT_BROKER, MQTT_PORT);
	
	while (!pubSubClient.connected())
	{
		Serial.printf("The client %s is connecting to MQTT broker on %s...\n", MDNS_NAME, MQTT_BROKER);
		if (pubSubClient.connect(MDNS_NAME, MQTT_USER, MQTT_PASS))
		{
			Serial.println("MQTT broker connected");
			pubSubClient.publish("active", "1");
			digitalWrite(DEBUG_PIN, HIGH);
		}
		else
		{
			Serial.print("MQTT connection failed: ");
			Serial.print(pubSubClient.state());

			digitalWrite(DEBUG_PIN, !digitalRead(DEBUG_PIN));
			delay(500);
		}
	}
}

void sendMQTTMessage(String topic, String message)
{
	if (!pubSubClient.publish(topic.c_str(), message.c_str()))
	{
		Serial.println("MQTT message not sent");
		return;
	}

	Serial.print("MQTT message sent '");
	Serial.print(message);
	Serial.print("' to topic '");
	Serial.print(topic);
	Serial.println("'");
}

void loopMQTT()
{
	if (pubSubClient.connected())
	{
		pubSubClient.loop();
	}
	else 
	{
		setupMQTT();
	}	
}
#endif

#if DHT_PIN > 0
#include "DHTesp.h"

DHTesp dht;
unsigned long lastTimeDHTTime;

void setupDHT() {
	dht.setup(DHT_PIN, DHTesp::DHT11);
}

void loopDHT() {
	if (currentTime - lastTimeDHTTime > DHT_POLLING_TIMEOUT) 	
	{	
		TempAndHumidity values = dht.getTempAndHumidity();
		if (isnan(values.temperature) || isnan(values.humidity))
		{
			Serial.println("Failed to read from DHT sensor!");
			sendMQTTMessage("error", "dht_read_failed");
		}
		else
		{
			Serial.printf("Temperature: %.2f C\n", values.temperature);
			Serial.printf("Humidity: %.2f %%\n", values.humidity);

			sendMQTTMessage("temperature", String(values.temperature));
			sendMQTTMessage("humidity", String(values.humidity));
		}

		lastTimeDHTTime = currentTime;
	}
}
#endif

#if PIR_PIN > 0
byte pirState = LOW;

void setupPIR() {
	pinMode(PIR_PIN, INPUT);
	digitalWrite(STATUS_PIN, HIGH); // pin
}


void loopPIR() 
{
	if (digitalRead(PIR_PIN) == HIGH)
	{
		if (pirState == LOW)
		{
			pirState = HIGH;
			Serial.println("Motion detected!");
			digitalWrite(STATUS_PIN, LOW);
			sendMQTTMessage("motion_detected", "1");
		}
	}
	else
	{
		if (pirState == HIGH)
		{
			pirState = LOW;
			Serial.println("Motion ended!");
			digitalWrite(STATUS_PIN, HIGH);
			sendMQTTMessage("motion_detected", "0");
		}
	}
}
#endif


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

void loopOTA() {
	ArduinoOTA.handle();
}
#endif

#ifdef MDNS_ENABLED
#include <ESP8266mDNS.h>

void setupMDNS()
{
	if (MDNS.begin(MDNS_NAME))
	{
		Serial.println("MDNS responder started");
	}
}

void loopMDNS() {
	MDNS.update();
}
#endif

// Setup & Loop
void setup()
{
	Serial.begin(9600);
	pinMode(STATUS_PIN, OUTPUT);
	pinMode(DEBUG_PIN, OUTPUT);

#if PIR_PIN > 0
	setupPIR();
#endif
	
#if DHT_PIN > 0
	setupDHT();
#endif

#ifdef WIFI_ENABLED
	setupWifi();
#endif

#ifdef OTA_ENABLED
	setupOTA();
#endif

#ifdef MQTT_ENABLED
	setupMQTT();
#endif

#ifdef MDNS_ENABLED
	setupMDNS();
#endif
}

void loop()
{
	currentTime = millis();
#ifdef OTA_ENABLED
	loopOTA();
#endif
#ifdef MDNS_ENABLED
	loopMDNS();
#endif
#ifdef MQTT_ENABLED
	loopMQTT();
#endif
#if DHT_PIN > 0
	loopDHT();
#endif
#if PIR_PIN > 0
	loopPIR();
#endif
}
