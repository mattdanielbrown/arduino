#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <SoftwareSerial.h>

#define FLASH_PIN 0
#define MQTT_CONN_RETRY_INTERVAL 60000

#define DHT_ENABLED 1
#define DHT_PIN 16
#define DHT_POLLING_TIMEOUT 30000

#define PIR_ENABLED 1
#define PIR_PIN 4

// #define SERVO_ENABLED 1
#define SERVO_PIN 14

#define BOARD_PREFIX "esp8266-gpio"

SoftwareSerial SerialDebug(3, 1);

unsigned long currentTime = millis();

char board_id[24];

char mqtt_server[64];
char mqtt_username[64];
char mqtt_password[64];

#define AVAILABILITY_ONLINE "online"
#define AVAILABILITY_OFFLINE "offline"

char MQTT_TOPIC_AVAILABILITY[80];
char MQTT_TOPIC_CALLBACK[80];
char MQTT_TOPIC_DEBUG[80];

char MQTT_TOPIC_TEMPERATURE[80];
char MQTT_TOPIC_HUMIDITY[80];
char MQTT_TOPIC_MOTION[80];
char MQTT_TOPIC_SERVO[80];

WiFiManager wifiManager;
WiFiClient wifiClient;
PubSubClient mqttClient;

WiFiManagerParameter wifi_param_mqtt_server("mqtt_server", "MQTT server", mqtt_server, sizeof(mqtt_server));
WiFiManagerParameter wifi_param_mqtt_username("mqtt_user", "MQTT username", mqtt_username, sizeof(mqtt_username));
WiFiManagerParameter wifi_param_mqtt_password("mqtt_pass", "MQTT password", mqtt_password, sizeof(mqtt_password));

unsigned long lastMqttConnectionAttempt = millis();

void log(char *format, ...)
{
	char buff[128];
	va_list args;
	va_start(args, format);
	vsnprintf(buff, sizeof(buff), format, args);
	va_end(args);
	buff[sizeof(buff) / sizeof(buff[0]) - 1] = '\0';

	SerialDebug.printf(buff);
	SerialDebug.printf("\n");

	sendMQTTMessage(MQTT_TOPIC_DEBUG, buff, false);
}

void saveConfig()
{
	SerialDebug.println("Saving config...");

	DynamicJsonDocument json(512);
	json["mqtt_server"] = wifi_param_mqtt_server.getValue();
	json["mqtt_username"] = wifi_param_mqtt_username.getValue();
	json["mqtt_password"] = wifi_param_mqtt_password.getValue();
	;

	File configFile = SPIFFS.open("/config.json", "w");
	if (!configFile)
	{
		SerialDebug.println("Failed to open config file for writing");
		return;
	}

	SerialDebug.printf("Saving JSON: %s", json.as<String>().c_str());

	serializeJson(json, configFile);
	configFile.close();

	SerialDebug.println("Config saved, please reboot");
}

void loadConfig()
{
	SerialDebug.println("Loading config");

	if (!SPIFFS.begin())
	{
		SerialDebug.println("Failed to open SPIFFS");
		return;
	}

	if (!SPIFFS.exists("/config.json"))
	{
		SerialDebug.println("Config file not found, please configure the ESP by connecting to its Wi-Fi hotspot");
		return;
	}

	File configFile = SPIFFS.open("/config.json", "r");
	if (!configFile)
	{
		SerialDebug.println("Failed to open config file");
		return;
	}

	const size_t size = configFile.size();
	std::unique_ptr<char[]> buf(new char[size]);

	configFile.readBytes(buf.get(), size);
	DynamicJsonDocument json(512);

	if (DeserializationError::Ok != deserializeJson(json, buf.get()))
	{
		SerialDebug.println("Failed to parse config fileDebug");
		return;
	}

	strcpy(mqtt_server, json["mqtt_server"]);
	strcpy(mqtt_username, json["mqtt_username"]);
	strcpy(mqtt_password, json["mqtt_password"]);

	wifi_param_mqtt_server.setValue(mqtt_server, sizeof(mqtt_server));
	wifi_param_mqtt_username.setValue(mqtt_username, sizeof(mqtt_username));
	wifi_param_mqtt_password.setValue(mqtt_password, sizeof(mqtt_password));

	SerialDebug.printf("Config JSON: %s", json.as<String>().c_str());
}

void setupGeneric()
{
	SerialDebug.begin(9600);
	loadConfig();

	pinMode(FLASH_PIN, INPUT_PULLUP);

	snprintf(board_id, sizeof(board_id), "%s-%X", BOARD_PREFIX, ESP.getChipId());
	SerialDebug.printf("Board Identifier: %s", board_id);

	snprintf(MQTT_TOPIC_AVAILABILITY, 80, "%s/availability", board_id);
	snprintf(MQTT_TOPIC_CALLBACK, 80, "%s/callback", board_id);
	snprintf(MQTT_TOPIC_DEBUG, 80, "%s/debug", board_id);

	snprintf(MQTT_TOPIC_TEMPERATURE, 80, "%s/temperature", board_id);
	snprintf(MQTT_TOPIC_HUMIDITY, 80, "%s/humidity", board_id);
	snprintf(MQTT_TOPIC_MOTION, 80, "%s/motion", board_id);
	snprintf(MQTT_TOPIC_SERVO, 80, "%s/servo", board_id);
}

bool portalRunning = false;

void setupWifi()
{
	wifiManager.setConfigPortalBlocking(false);
	wifiManager.setDebugOutput(false);
	wifiManager.setSaveParamsCallback(saveConfig);

	wifiManager.addParameter(&wifi_param_mqtt_server);
	wifiManager.addParameter(&wifi_param_mqtt_username);
	wifiManager.addParameter(&wifi_param_mqtt_password);

	if (wifiManager.autoConnect(board_id))
	{
		WiFi.mode(WIFI_STA);
		wifiManager.startWebPortal();
	}
	else
	{
		SerialDebug.println("Failed to connect to WiFi, starting AP");
	}
}

void loopWifi()
{
	wifiManager.process();

	if (digitalRead(FLASH_PIN) == LOW && !portalRunning)
	{
		portalRunning = true;

		SerialDebug.println("Starting Config Portal");
		wifiManager.startConfigPortal();
	}
}

void setupMQTT()
{
	mqttClient.setClient(wifiClient);

	mqttClient.setServer(mqtt_server, 1883);
	mqttClient.setKeepAlive(10);
	mqttClient.setBufferSize(2048);
	mqttClient.setCallback(mqttCallback);

	mqttEnsureConnected();
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
	char payloadText[length + 1];
	snprintf(payloadText, length + 1, "%s", payload);
	log("MQTT callback with topic <%s> and payload <%s>", topic, payloadText);

	DynamicJsonDocument commandJson(256);
	DeserializationError err = deserializeJson(commandJson, payloadText);

	if (err)
	{
		log("Error deserializing JSON");
		return;
	}

	String command = commandJson["command"].as<String>();
#ifdef SERVO_ENABLED
	if (command == "servo")
	{
		handleServo(commandJson);
		return;
	}
#endif

	log("Unknown callback command: %s", command.c_str());
}

void sendMQTTMessage(const char *topic, const char *message, bool retained)
{
	SerialDebug.printf("MQTT message - topic: <%s>, message: <%s> -> ", topic, message, retained);

	if (mqttClient.publish(topic, message))
	{
		SerialDebug.println("sent");
	}
	else
	{
		SerialDebug.println("error");
	}
}

void mqttEnsureConnected()
{
	if (mqttClient.connect(board_id, mqtt_username, mqtt_password, MQTT_TOPIC_AVAILABILITY, 1, true, AVAILABILITY_OFFLINE))
	{
		mqttClient.subscribe(MQTT_TOPIC_CALLBACK);
		sendMQTTMessage(MQTT_TOPIC_AVAILABILITY, AVAILABILITY_ONLINE, true);
	}
	else
	{
		SerialDebug.println("Unable to connect to MQTT broker");
	}
}

void loopMQTT()
{
	mqttClient.loop();
	if (mqttClient.connected())
		return;
	if (currentTime - lastMqttConnectionAttempt < MQTT_CONN_RETRY_INTERVAL)
		return;

	SerialDebug.println("Connection to MQTT lost, reconnecting...");
	lastMqttConnectionAttempt = currentTime;

	mqttEnsureConnected();
}

void setupOTA()
{
	ArduinoOTA.onStart([]() {});
	ArduinoOTA.onEnd([]() {});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {});
	ArduinoOTA.onError([](ota_error_t error) {});
	ArduinoOTA.setHostname(board_id);
	ArduinoOTA.setPassword(board_id);
	ArduinoOTA.begin();
}

void loopOTA()
{
	ArduinoOTA.handle();
}

void setupMDNS()
{
	MDNS.begin(board_id);
}

void loopMDNS()
{
	MDNS.update();
}

#ifdef DHT_ENABLED
#include "DHTesp.h"

DHTesp dht;
unsigned long lastTimeDHTTime = 0;

void setupDHT()
{
	dht.setup(DHT_PIN, DHTesp::DHT11);
}

void loopDHT()
{
	if (currentTime - lastTimeDHTTime < DHT_POLLING_TIMEOUT)
		return;

	log("DHT polling tick");

	lastTimeDHTTime = currentTime;

	TempAndHumidity values = dht.getTempAndHumidity();

	if (!isnan(values.temperature))
	{
		sendMQTTMessage(MQTT_TOPIC_TEMPERATURE, String(values.temperature).c_str(), false);
	}
	else
	{
		log("DHT temperature error: %s", dht.getStatusString());
	}

	if (!isnan(values.humidity))
	{
		sendMQTTMessage(MQTT_TOPIC_HUMIDITY, String(values.humidity).c_str(), false);
	}
	else
	{
		log("DHT humidity error: %s", dht.getStatusString());
	}
}

#endif

#ifdef PIR_ENABLED
boolean pirState = false;

void setupPIR()
{
	pinMode(PIR_PIN, INPUT);
}

void loopPIR()
{
	if (digitalRead(PIR_PIN) == HIGH)
	{
		if (pirState == false)
		{
			pirState = true;
			sendMQTTMessage(MQTT_TOPIC_MOTION, "1", false);
		}
	}
	else
	{
		if (pirState == true)
		{
			pirState = false;
			sendMQTTMessage(MQTT_TOPIC_MOTION, "0", false);
		}
	}
}
#endif

#ifdef SERVO_ENABLED
#include <Servo.h>

Servo servo;

void setupServo()
{
	servo.attach(SERVO_PIN);
	servo.write(0);
}

void loopServo()
{
}

void handleServo(DynamicJsonDocument json)
{
	String method = json["method"].as<String>();
	int degree = json["degree"].as<int>();

	if (!degree)
		degree = 30;

	if (method == "degree")
	{
		servo.write(degree);
		sendMQTTMessage(MQTT_TOPIC_SERVO, "1", false);
		return;
	}

	if (method == "push")
	{
		int delayMs = json["delayMs"].as<int>();
		int cycles = json["cycles"].as<int>();
		int delayMsBetweenCycles = json["delayMsBetweenCycles"].as<int>();

		if (!delayMs)
			delayMs = 600;
		if (!cycles)
			cycles = 1;
		if (!delayMsBetweenCycles)
			delayMsBetweenCycles = 1000;

		for (int i = 0; i < cycles; i++)
		{
			servo.write(degree);
			delay(delayMs);
			servo.write(0);

			if ((i + 1) != cycles)
			{
				delay(delayMsBetweenCycles);
			}
		}

		sendMQTTMessage(MQTT_TOPIC_SERVO, "1", false);
		return;
	}

	log("Unknown servo method: %s", method.c_str());
}

#endif

// Setup & Loop
void setup()
{
	setupGeneric();

	setupWifi();
	setupMDNS();
	setupOTA();
	setupMQTT();

#ifdef PIR_ENABLED
	setupPIR();
#endif
#ifdef DHT_ENABLED
	setupDHT();
#endif
#ifdef SERVO_ENABLED
	setupServo();
#endif
}

void loop()
{
	currentTime = millis();

	loopWifi();
	loopMDNS();
	loopOTA();
	loopMQTT();

#ifdef PIR_ENABLED
	loopPIR();
#endif
#ifdef DHT_ENABLED
	loopDHT();
#endif
#ifdef SERVO_ENABLED
	loopServo();
#endif
}
