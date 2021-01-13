#include <Servo.h>

const byte SERVO_PIN = 2;
const byte SAMPLE_TIME = 50;

unsigned int sample;
unsigned long startMillis;

Servo myservo;

void setup()
{
    Serial.begin(115200);
    myservo.attach(SERVO_PIN);
}

void loop()
{
    startMillis = millis(); // Start of sample window

    unsigned int signalMax = 0;
    unsigned int signalMin = 1024;

    while (millis() - startMillis < SAMPLE_TIME)
    {
        sample = analogRead(A0);
        if (sample > signalMax)
        {
            signalMax = sample;
        }
        else if (sample < signalMin)
        {
            signalMin = sample;
        }
    }

    float peakToPeak = signalMax - signalMin;
    Serial.println(peakToPeak);

    int radius = map(peakToPeak, 0, 50, 0, 180);
    myservo.write(180 - radius);

    delay(10);
}
