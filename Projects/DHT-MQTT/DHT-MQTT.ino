#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"

// Comment this
#include "/usr/local/share/arduino.env.h"

#define WIFI_LED_GPIO 2				// Led to switch when WiFi is connected
#define MDNS_NAME "TemperatureSensor" // Name of this board on the local network
#define DHTPIN 16						// Pin connected to the DHT sensor				
#define POLLING_TIMEOUT	5000			// Timeout for polling the DHT sensor

WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);

unsigned long currentTime;
unsigned long lastTime;

DHTesp dht;

void sendMQTTMessage(String topic, String message)
{
	if (!pubSubClient.publish(topic.c_str(), message.c_str()))
	{
		Serial.println("MQTT message not sent");
		return;
	}

	Serial.print("MQTT message sent: ");
	Serial.print(message);
	Serial.print(" to topic: ");
	Serial.print(topic);
	Serial.println("");
}

void MQTTLoop()
{
	if (pubSubClient.connected())
	{
		pubSubClient.loop();
		return;
	}

	Serial.printf("The client %s is connecting to MQTT broker on %s...\n", MDNS_NAME, MQTT_BROKER);

	pubSubClient.setServer(MQTT_BROKER, MQTT_PORT);

	while (!pubSubClient.connected())
	{
		if (pubSubClient.connect(MDNS_NAME, MQTT_USER, MQTT_PASS))
		{
			Serial.println("MQTT broker connected");
			pubSubClient.publish("active", "1");
			digitalWrite(WIFI_LED_GPIO, HIGH);
		}
		else
		{
			Serial.print("MQTT connection failed: ");
			Serial.print(pubSubClient.state());

			digitalWrite(WIFI_LED_GPIO, !digitalRead(WIFI_LED_GPIO));
			delay(500);
		}
	}
}

void setupWifi()
{
	Serial.print("Connecting to Wi-Fi...");

	WiFi.mode(WIFI_STA);
	WiFi.begin(WIFI_SSID, WIFI_PASS);
	WiFi.setAutoReconnect(true);
	WiFi.persistent(true);

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

#ifdef MDNS_ENABLED
#include <ESP8266mDNS.h>

void setupMDNS()
{
	if (MDNS.begin(MDNS_NAME))
	{
		Serial.println("MDNS responder started");
	}
}
#endif

// Setup & Loop
void setup()
{
	Serial.begin(9600);

	setupWifi();
#ifdef MDNS_ENABLED
	setupMDNS();
#endif
#ifdef OTA_ENABLED
	setupOTA();
#endif

	dht.setup(DHTPIN, DHTesp::DHT11);
}

void loop()
{
	currentTime = millis();

#ifdef OTA_ENABLED
	ArduinoOTA.handle();
#endif

#ifdef MDNS_ENABLED
	MDNS.update();
#endif

	MQTTLoop();

	if (currentTime - lastTime > POLLING_TIMEOUT) 
	{	
		TempAndHumidity values = dht.getTempAndHumidity();
		if (isnan(values.temperature) || isnan(values.humidity))
		{
			Serial.println("Failed to read from DHT sensor!");
			sendMQTTMessage("error", "read_failed");
		}
		else
		{
			Serial.printf("Temperature: %.2f C\n", values.temperature);
			Serial.printf("Humidity: %.2f %%\n", values.humidity);

			sendMQTTMessage("temperature", String(values.temperature));
			sendMQTTMessage("humidity", String(values.humidity));
		}

		lastTime = currentTime;
	}
}
