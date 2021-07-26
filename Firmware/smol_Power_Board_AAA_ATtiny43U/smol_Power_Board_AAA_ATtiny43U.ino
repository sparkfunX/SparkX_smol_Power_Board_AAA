/*
  smôl Power Board AAA Firmware
  By: Paul Clark
  Date: July 26th 2021
  Version: 1.0

  goToSleep adapted from Jack Christensen's AVR Sleep example for the ATtinyX4:
  https://gist.github.com/JChristensen/5616922

  Uses Spence Konde's ATTinyCore boards:
  https://github.com/SpenceKonde/ATTinyCore
  https://github.com/SpenceKonde/ATTinyCore/blob/master/Installation.md
  Add this URL to the Additional Boards Manager URLs in File\Preferences:
  http://drazzy.com/package_drazzy.com_index.json
  and then use the Boards Manager to add support for the ATtiny: http://boardsmanager/All#ATTinyCore

  Set Board to ATtiny43 (No bootloader)
  Set Clock to 4MHz(internal)
  Set millis()/micros() to Disabled
  Set Save EEPROM to EEPROM Not Retained
  Set BOD Level to BOD Disabled (this helps reduce power consumption in low power mode)

  Fuse byte settings:
  Low:  0b01100010 = 0x62 : divide clock by 8, no clock out, slowly rising power, 8MHz internal oscillator
  High: 0b11011111 = 0xDF : reset not disabled, no debug wire, SPI programming enabled, WDT not always on, EEPROM not preserved, BOD disabled
  Ext:  0xFF : no self programming

  Pin Allocation (Physical pins are for the QFN package):
  D0:  Physical Pin 19 (PB0)               : Not used
  D1:  Physical Pin 20 (PB1)               : Not used
  D2:  Physical Pin 1  (PB2)               : Not used
  D3:  Physical Pin 2  (PB3)               : Not used
  D4:  Physical Pin 3  (PB4 / DI/COPI/SDA) : I2C SDA
  D5:  Physical Pin 4  (PB5 / CIPO)        : CIPO - connected to pin 1 of the ISP header
  D6:  Physical Pin 5  (PB6 / SCK/SCL)     : I2C SCL
  D7:  Physical Pin 6  (PB7 / SS/INT0)     : Not used
  D8:  Physical Pin 11 (PA0 / A0)          : Not used
  D9:  Physical Pin 12 (PA1 / A1)          : Not used
  D10: Physical Pin 13 (PA2 / A2)          : Not used
  D11: Physical Pin 14 (PA3 / A3)          : 3V3 Regulator Enable
  D12: Physical Pin 15 (PA4 / TXD)         : TXD Test Point
  D13: Physical Pin 16 (PA5 / RXD)         : RXD Test Point
  D14: Physical Pin 17 (PA6)               : Not used
  D15: Physical Pin 18 (PA7 / !RESET)      : !RESET

  The ATtiny43U's default I2C address is 0x50

  TO DO: Start the WDT. Implement power-down

*/

#include <Wire.h>

#include <avr/sleep.h> //Needed for sleep_mode
#include <avr/power.h> //Needed for powering down perihperals such as the ADC

#include <EEPROM.h>

#include "smol_Power_Board_AAA_ATtiny43U_Constants.h"
#include "smol_Power_Board_AAA_ATtiny43U_EEPROM.h"

//Digital pins
const byte EN_3V3 = 11; // 3V3 Regulator Enable: pull high to enable 3.3V, pull low to disable

//Define the ON and OFF states for each pin
#define EN_3V3__ON   HIGH // 3V3 Regulator Enable: pull high to enable 3.3V, pull low to disable
#define EN_3V3__OFF  LOW  // 3V3 Regulator Enable: pull high to enable 3.3V, pull low to disable

//Global variables
#define TINY_BUFFER_LENGTH 16
struct {
  byte receiveEventRegister = SFE_AAA_REGISTER_UNKNOWN; // Most recent receive event register address
  byte receiveEventBuffer[TINY_BUFFER_LENGTH]; // byte array to store the data from the receive event
  byte receiveEventLength = 0; // Indicates how many data bytes were received
} receiveEventData;
byte registerResetReason;
uint16_t registerTemperature;
uint16_t registerVBAT;
uint16_t registerVCCVoltage;
byte registerADCReference = SFE_AAA_ADC_REFERENCE_VCC; // Default to using VCC as the ADC reference

//WDT Interrupt Service Routine - controls when the ATtiny43U wakes from sleep
void wdtISR() {}

void setup()
{
  //Just in case, make sure the system clock is set to 4MHz (8MHz divided by 2)
  CLKPR = 1 << CLKPCE; //Set the clock prescaler change enable bit
  CLKPR = 1; //Set clock prescaler CLKPS bits to 1 == divide by 2

  pinMode(EN_3V3, OUTPUT); // Enable the 3V3 regulator for the smôl bus
  digitalWrite(EN_3V3, EN_3V3__ON);

  registerResetReason = MCUSR & 0x0F; // Record the reset reason from MCUSR

  if (!loadEepromSettings()) // Load the settings from eeprom
  {
    initializeEepromSettings(); // Initialize them if required
    registerResetReason |= SFE_AAA_EEPROM_CORRUPT_ON_RESET; // Flag that the eeprom was corrupt and needed to be initialized
  }

  WDTCSR = (WDTCSR & 0xD8) | ((eeprom_settings.wdtPrescaler & 0x08) << 2) | (eeprom_settings.wdtPrescaler & 0x07); // Update the WDT prescaler (bits 5,2,1,0)

  //Begin listening on I2C
  startI2C(true); // Skip Wire.end the first time around
}

void loop()
{
  //Check if a receive event has taken place
  switch (receiveEventData.receiveEventRegister)
  {
    case SFE_AAA_REGISTER_I2C_ADDRESS: // Does the user want to change the I2C address?
      if (receiveEventData.receiveEventLength >= 2) // Data should be the new address followed by a CRC
      {
        if (receiveEventData.receiveEventBuffer[1] == computeCRC8(receiveEventData.receiveEventBuffer, 1)) // Is the CRC valid?
        {
          eeprom_settings.i2cAddress = receiveEventData.receiveEventBuffer[0]; // Update the I2C address in eeprom
          saveEepromSettings(); // Update the address in eeprom
          startI2C(false); // Restart I2C comms
        }
      }
      receiveEventData.receiveEventLength = 0; // Clear the event
      break;
    case SFE_AAA_REGISTER_RESET_REASON: // Does the user want to read the reset reason?
      if (receiveEventData.receiveEventLength >= 1) // Read request should be a single byte
      {
        // Nothing to do here
        receiveEventData.receiveEventLength = 0; // Clear the event
      }
      break;
    case SFE_AAA_REGISTER_TEMPERATURE: // Does the user want to read the CPU temperature?
      if (receiveEventData.receiveEventLength >= 1) // Read request should be a single byte
      {
        readTemperature(); // Update registerTemperature
        receiveEventData.receiveEventLength = 0; // Prevent multiple readTemperature's
      }
      break;
    case SFE_AAA_REGISTER_VBAT: // Does the user want to read the battery voltage?
      if (receiveEventData.receiveEventLength >= 1) // Read request should be a single byte
      {
        readVBAT(); // Update registerVBAT
        receiveEventData.receiveEventLength = 0; // Prevent multiple readVBAT's
      }
      break;
    case SFE_AAA_REGISTER_VCC_VOLTAGE: // Does the user want to read the VCC voltage?
      if (receiveEventData.receiveEventLength >= 1) // Read request should be a single byte
      {
        readVCCVoltage(); // Update registerVCCVoltage
        receiveEventData.receiveEventLength = 0; // Prevent multiple readVCCVoltage's
      }
      break;
    case SFE_AAA_REGISTER_ADC_REFERENCE: // Does the user want to set the ADC reference for readVBAT?
      if (receiveEventData.receiveEventLength >= 2) // Data should be the new reference followed by a CRC
      {
        if (receiveEventData.receiveEventBuffer[1] == computeCRC8(receiveEventData.receiveEventBuffer, 1)) // Is the CRC valid?
        {
          registerADCReference = receiveEventData.receiveEventBuffer[0]; // Update the ADC reference
        }
      }
      receiveEventData.receiveEventLength = 0; // Clear the event
      break;
    case SFE_AAA_REGISTER_WDT_PRESCALER: // Does the user want to change the WDT prescaler?
      if (receiveEventData.receiveEventLength >= 2) // Data should be the new prescaler followed by a CRC
      {
        if (receiveEventData.receiveEventBuffer[1] == computeCRC8(receiveEventData.receiveEventBuffer, 1)) // Is the CRC valid?
        {
          eeprom_settings.wdtPrescaler = receiveEventData.receiveEventBuffer[0]; // Update the WDT prescaler in eeprom
          saveEepromSettings(); // Update the address in eeprom
          WDTCSR = (WDTCSR & 0xD8) | ((eeprom_settings.wdtPrescaler & 0x08) << 2) | (eeprom_settings.wdtPrescaler & 0x07); // Update the WDT prescaler (bits 5,2,1,0)
        }
      }
      receiveEventData.receiveEventLength = 0; // Clear the event
      break;
    case SFE_AAA_REGISTER_POWERDOWN_DURATION: // Does the user want to change the power-down duration?
      if (receiveEventData.receiveEventLength >= 3) // Data should be the new prescaler (uint16_t little endian) followed by a CRC
      {
        if (receiveEventData.receiveEventBuffer[2] == computeCRC8(receiveEventData.receiveEventBuffer, 2)) // Is the CRC valid?
        {
          eeprom_settings.powerDownDuration = ((uint16_t)receiveEventData.receiveEventBuffer[1] << 8) | ((uint16_t)receiveEventData.receiveEventBuffer[0]); // Update the WDT prescaler in eeprom. Data is little endian.
          saveEepromSettings(); // Update the address in eeprom
        }
      }
      receiveEventData.receiveEventLength = 0; // Clear the event
      break;
    case SFE_AAA_REGISTER_POWERDOWN_NOW: // Does the user want to power-down now?
      if (receiveEventData.receiveEventLength >= 7) // Data should be "SLEEP" followed by a CRC
      {
        if (receiveEventData.receiveEventBuffer[5] == computeCRC8(receiveEventData.receiveEventBuffer, 5)) // Is the CRC valid?
        {
          if ((receiveEventData.receiveEventBuffer[0] == 'S') && (receiveEventData.receiveEventBuffer[1] == 'L')
              && (receiveEventData.receiveEventBuffer[2] == 'E') && (receiveEventData.receiveEventBuffer[3] == 'E')
              && (receiveEventData.receiveEventBuffer[4] == 'P'))
          {
            // TO DO: Add a call to the power-down function here
            digitalWrite(EN_3V3, !digitalRead(EN_3V3)); // Test: toggle the 3V3 enable
          }
        }
      }
      receiveEventData.receiveEventRegister = SFE_AAA_REGISTER_UNKNOWN; // Clear the event
      receiveEventData.receiveEventLength = 0; // Clear the event
      break;
    case SFE_AAA_REGISTER_FIRMWARE_VERSION: // Does the user want to read the firmware version?
      if (receiveEventData.receiveEventLength >= 1) // Read request should be a single byte
      {
        // Nothing to do here
        receiveEventData.receiveEventLength = 0; // Clear the event
      }
      break;
    case SFE_AAA_REGISTER_UNKNOWN:
      // Nothing to do here
      break;
    default:
      receiveEventData.receiveEventRegister = SFE_AAA_REGISTER_UNKNOWN; // Clear the event
      receiveEventData.receiveEventLength = 0; // Clear the event
      break;
  }
}

//Begin listening on I2C bus as I2C peripheral using the global I2C_ADDRESS
void startI2C(bool skipWireEnd)
{
  if (skipWireEnd == false)
    Wire.end(); //Before we can change addresses we need to stop

  Wire.begin(eeprom_settings.i2cAddress); //Do the Wire.begin using the defined I2C_ADDRESS

  //The connections to the interrupts are severed when a Wire.begin occurs. So re-declare them.
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
}

//Measure the CPU temperature
void readTemperature()
{
  analogReference(SFE_AAA_ADC_REFERENCE_1V1); //You must use the internal 1.1v bandgap reference when measuring temperature
  uint16_t result = 0;
  for (byte x = 0; x < 8; x++)
  {
    result += analogRead(ADC_TEMPERATURE); //ADC_TEMPERATURE is ADC channel 7
    noIntDelay(1);
  }
  registerTemperature = result >> 3; // Divide result by 8
}

//Measure VCC by measuring the 1.1V bandgap using VCC as the reference
void readVCCVoltage()
{
  analogReference(SFE_AAA_ADC_REFERENCE_VCC); //You must use VCC as the reference when measuring temperature
  uint16_t result = 0;
  for (byte x = 0; x < 8; x++)
  {
    result += analogRead(ADC_TEMPERATURE - 2); //The 1.1V reference is channel 5. ADC_TEMPERATURE is ADC channel 7
    noIntDelay(1);
  }
  registerVCCVoltage = result >> 3; // Divide result by 8
}

//Measure VBAT
void readVBAT()
{
  analogReference(registerADCReference); //Select the desired voltage reference
  uint16_t result = 0;
  for (byte x = 0; x < 8; x++)
  {
    result += analogRead(ADC_TEMPERATURE - 1); //VBAT is ADC channel 6. ADC_TEMPERATURE is ADC channel 7
    noIntDelay(1);
  }
  analogRead(ADC_TEMPERATURE); //Read the CPU temperature to de-select channel 6 and disconnect the VBAT divide-by-2 circuit
  registerVBAT = result >> 3; // Divide result by 8
}

//Software delay. Does not rely on internal timers.
void noIntDelay(byte amount)
{
  for (volatile byte y = 0 ; y < amount ; y++)
  {
    //ATtiny84 at 4MHz
    for (volatile unsigned int x = 0 ; x < 175 ; x++) //1ms at 4MHz
    {
      __asm__("nop\n\t");
    }
  }
}
