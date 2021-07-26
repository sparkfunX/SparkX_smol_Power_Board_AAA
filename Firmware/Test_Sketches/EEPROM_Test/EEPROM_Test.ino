/*
  smôl Power Board AAA EEPROM Test
  By: Paul Clark
  Date: July 26th 2021
  Version: 1.0

  Uses Spence Konde's ATTinyCore boards:
  https://github.com/SpenceKonde/ATTinyCore
  https://github.com/SpenceKonde/ATTinyCore/blob/master/Installation.md
  Add this URL to the Additional Boards Manager URLs in File\Preferences:
  http://drazzy.com/package_drazzy.com_index.json
  and then use the Boards Manager to add support for the ATtiny: http://boardsmanager/All#ATTinyCore

  Set Board to ATtiny43 (No bootloader)
  Set Clock Source to 4MHz(internal)
  Set millis()/micros() to Disabled
  Set Save EEPROM to EEPROM Not Retained
  Set BOD Level to BOD Disabled (this helps reduce power consumption in low power mode)

  Fuse byte settings:
  Low:  0b01100010 = 0x62 : divide clock by 8, no clock out, slowly rising power, 8MHz internal oscillator
  High: 0b11011111 = 0xDF : reset not disabled, no debug wire, SPI programming enabled, WDT not always on, EEPROM not preserved, BOD disabled
  Ext:  0xFF : no self programming

*/

#include <EEPROM.h>

#include "smol_Power_Board_AAA_ATtiny43U_Constants.h"
#include "smol_Power_Board_AAA_ATtiny43U_EEPROM.h"

void setup()
{
  //Just in case, make sure the system clock is set to 4MHz (8MHz divided by 2)
  CLKPR = 1 << CLKPCE; //Set the clock prescaler change enable bit
  CLKPR = 1; //Set clock prescaler CLKPS bits to 1 == divide by 2

  Serial.begin(9600);
  Serial.println(F("smôl Power Board AAA EEPROM Test"));

  if(loadEepromSettings())
  {
    Serial.println(F("EEPROM settings are valid:"));
    printEepromSettings();

    Serial.println(F("Incrementing powerDownDuration"));
    eeprom_settings.powerDownDuration++;
    saveEepromSettings();
    
    if(loadEepromSettings())
    {
      Serial.println(F("EEPROM settings are valid:"));
      printEepromSettings();
    }
    else
    {
      Serial.println(F("EEPROM settings are no longer valid! Something bad must have happened..."));
    }
  }
  else
  {
    Serial.println(F("EEPROM settings are NOT valid!"));
    Serial.println(F("EEPROM settings will be initialized"));
    initializeEepromSettings();
    noIntDelay(250);

    if(loadEepromSettings())
    {
      Serial.println(F("EEPROM settings are valid:"));
      printEepromSettings();
    }
    else
    {
      Serial.println(F("EEPROM settings are still NOT valid! Something bad must have happened..."));
    }
  }
}

void printEepromSettings()
{
  Serial.print(F("sizeOfSettings    : ")); Serial.println(eeprom_settings.sizeOfSettings);
  Serial.print(F("firmwareVersion   : 0x")); Serial.println(eeprom_settings.firmwareVersion, HEX);
  Serial.print(F("i2cAddress        : 0x")); Serial.println(eeprom_settings.i2cAddress, HEX);
  Serial.print(F("wdtPrescaler      : ")); Serial.println(eeprom_settings.wdtPrescaler);
  Serial.print(F("powerDownDuration : ")); Serial.println(eeprom_settings.powerDownDuration);
  Serial.print(F("CRC               : 0x")); Serial.println(eeprom_settings.CRC, HEX);
}

void loop() // Test noIntDelay - should print a period every 250ms if the clock has been set correctly to 4MHz
{
  noIntDelay(250);
  Serial.println();
}

//Software delay. Does not rely on internal timers.
void noIntDelay(byte amount)
{
  for (volatile byte y = 0 ; y < amount ; y++)
  {
    //ATtiny84 at 4MHz
    for (volatile unsigned int x = 0 ; x < 202 ; x++) //1ms at 4MHz
    {
      __asm__("nop\n\t");
    }
  }
}
