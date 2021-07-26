// smôl Power Board AAA EEPROM storage and fuctions

struct struct_eeprom_settings {
  byte sizeOfSettings = 0; //sizeOfSettings **must** be the first entry and must be byte
  byte firmwareVersion = POWER_BOARD_AAA_FIRMWARE_VERSION; // firmwareVersion **must** be the second entry
  byte i2cAddress = DEFAULT_I2C_ADDRESS;
  byte wdtPrescaler = DEFAULT_WDT_PRESCALER;
  uint16_t powerDownDuration = 1; // Default to power-down of 1 WDT interrupt
  byte CRC;
} eeprom_settings;
