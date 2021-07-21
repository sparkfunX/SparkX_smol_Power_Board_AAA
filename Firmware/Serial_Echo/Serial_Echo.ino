// smôl Power Board AAA : Serial Echo Test
//
// Use Spence Konde's ATtinyCore: http://boardsmanager/All#ATTinyCore
// Select ATtiny43 (No bootloader)
// Select Clock Source: 8HMz Internal
//
// Fuse byte settings:
// Low:  0b11100010 = 0xE2 : no clock divider, no clock out, slowly rising power, 8MHz internal oscillator
// High: 0b11011111 = 0xDF : reset not disabled, no debug wire, SPI programming enabled, WDT not always on, EEPROM not preserved, BOD disabled
// Ext:  0xFF : no self programming

void setup() {
  Serial.begin(115200);
}

void loop() {
  while (Serial.available())
    Serial.write(Serial.read());
}
