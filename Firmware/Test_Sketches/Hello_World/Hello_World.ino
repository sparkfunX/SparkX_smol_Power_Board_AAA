// smôl Power Board AAA : Serial Tx Test
//
// Use Spence Konde's ATtinyCore: http://boardsmanager/All#ATTinyCore
// Select ATtiny43 (No bootloader)

void setup() {
  Serial.begin(115200);
}

void loop() {
  Serial.println("Hello World!");
  delay(1000);
}
