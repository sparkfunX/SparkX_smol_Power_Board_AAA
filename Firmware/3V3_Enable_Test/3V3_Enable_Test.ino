// smôl Power Board AAA : 3V3 Enable Test
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
  
  pinMode(11, OUTPUT); // TPS61200 EN pin is connected to ATtiny43 D11 
  digitalWrite(11, LOW); // Disable the TPS61200 by pulling the EN pin low
}

void loop() {
  while (Serial.available())
  {
    char ch = Serial.read();
    if (ch == '0')
    {
      Serial.println("TPS61200 is disabled");
      digitalWrite(11, LOW); // Disable the TPS61200 by pulling the EN pin low
    }
    else if (ch == '1')
    {
      Serial.println("TPS61200 is enabled");
      digitalWrite(11, HIGH); // Enable the TPS61200 by pulling the EN pin high
    }
  }
}
