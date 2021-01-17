// Include the libraries we need 
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 2

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup(void) {
  // start serial port
  Serial.begin(9600);
  // Start up the library
  sensors.begin(); 
}
 
void loop(void) {
  sensors.requestTemperatures(); // Send the command to get temperatures
  // After we got the temperatures, we can print them here.
  // We use the function ByIndex, and as an example get the temperature from the first sensor only.
  Serial.println(sensors.getTempCByIndex(0)); 
}
