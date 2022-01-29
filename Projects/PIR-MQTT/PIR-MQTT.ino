#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Comment this
#include "/usr/local/share/arduino.env.h"

#define WIFI_LED_GPIO 2				// Led to switch when WiFi is connected
#define PIR_GPIO 4					// Identify the GPIO pin connected to the PIR sensor
#define LED_PIR_GPIO 16				// LED used to debug PIR on/off
#define MDNS_NAME "MotionSensor" // Name of this board on the local network

byte ACCESSORY_ENABLED = 1; // If set to 0, the board won't perform any action

// State variables
byte pirState = LOW;
byte lightState = 0;

WiFiClient wifiClient;
PubSubClient client(wifiClient);
unsigned long currentTime;

void onMotionDetected()
{
	Serial.println("Motion detected!");
	digitalWrite(LED_PIR_GPIO, LOW);
	client.publish("motion_detected", "1");
}

void onMotionEnded()
{
	Serial.println("Motion ended!");
	digitalWrite(LED_PIR_GPIO, HIGH);
	client.publish("motion_detected", "0");
}

void setupConnection()
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

	client.setServer(MQTT_BROKER, MQTT_PORT);

	Serial.printf("The client %s is connecting to MQTT broker on %s...\n", MDNS_NAME, MQTT_BROKER);

	while (!client.connected())
	{
		if (client.connect(MDNS_NAME, MQTT_USER, MQTT_PASS))
		{
			Serial.println("MQTT broker connected");
		}
		else
		{
			Serial.print("MQTT connection failed: ");
			Serial.print(client.state());

			digitalWrite(WIFI_LED_GPIO, !digitalRead(WIFI_LED_GPIO));
			delay(500);
		}
	}

	client.publish("active", "1");
	digitalWrite(WIFI_LED_GPIO, HIGH);
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
		MDNS.addService("http", "tcp", 80);
		Serial.println("MDNS responder started");
	}
}
#endif

#ifdef SERVER_ENABLED

#endif

// Setup & Loop

void setup()
{
#ifdef SERIAL_PORT
	Serial.begin(115200);
#endif

	pinMode(LED_PIR_GPIO, OUTPUT);
	pinMode(PIR_GPIO, INPUT);
	digitalWrite(LED_PIR_GPIO, HIGH);

	setupConnection();

#ifdef MDNS_ENABLED
	setupMDNS();
#endif
#ifdef OTA_ENABLED
	setupOTA();
#endif
}

void loop()
{
#ifdef OTA_ENABLED
	ArduinoOTA.handle();
#endif

#ifdef MDNS_ENABLED
	MDNS.update();
#endif

	currentTime = millis();
	client.loop();

	if (ACCESSORY_ENABLED)
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
}
