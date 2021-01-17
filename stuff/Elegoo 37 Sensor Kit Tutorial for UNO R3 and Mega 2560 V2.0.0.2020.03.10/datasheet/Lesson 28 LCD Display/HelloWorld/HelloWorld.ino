#include <LiquidCrystal.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(4,0,12,13,15,3);

void setup() {
  lcd.begin(16, 2);
}

int i = 0;
void loop() {
  lcd.setCursor(i++ % 16, 0);
  lcd.print("Valentina");
  delay(100);
}
