//2018.6.28
#include <dht_nonblocking.h>

#define DHT_SENSOR_TYPE DHT_TYPE_11
//#define DHT_SENSOR_TYPE DHT_TYPE_21
//#define DHT_SENSOR_TYPE DHT_TYPE_22

static const int DHT_SENSOR_PIN = 14;
DHT_nonblocking dht_sensor( DHT_SENSOR_PIN, DHT_SENSOR_TYPE );

void setup( )
{
  Serial.begin( 9600);
  pinMode(5, OUTPUT); // Configurare PIN 5 in OUTPUT
  pinMode(4, OUTPUT); // Configurare PIN 5 in OUTPUT
}

void loop( )
{
  float temperature;
  float humidity;

  if (dht_sensor.measure(&temperature, &humidity)) {
    Serial.print( "T = " );
    Serial.print( temperature, 1 );
    Serial.print( " deg. C, H = " );
    Serial.print( humidity, 1 );
    Serial.println( "%" );

    if (humidity > 45) {
      digitalWrite(4, HIGH);
    } else{
      digitalWrite(4, LOW);
    }

     if (temperature > 30) {
      digitalWrite(5, HIGH);
    } else{
      digitalWrite(5, LOW);
    }
  }
}
