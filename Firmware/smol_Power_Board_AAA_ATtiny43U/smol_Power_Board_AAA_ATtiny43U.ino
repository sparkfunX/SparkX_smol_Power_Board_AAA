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
  volatile byte receiveEventRegister = SFE_AAA_REGISTER_UNKNOWN; // Most recent receive event register address
  volatile byte receiveEventBuffer[TINY_BUFFER_LENGTH]; // byte array to store the data from the receive event
  volatile byte receiveEventLength = 0; // Indicates how many data bytes were received
} receiveEventData;
volatile byte registerResetReason;
volatile uint16_t registerTemperature;
volatile uint16_t registerVBAT;
volatile uint16_t register1V1;
volatile byte registerADCReference = SFE_AAA_ADC_REFERENCE_VCC; // Default to using VCC as the ADC reference

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
  MCUSR = 0; // Clear the MCUSR

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
  if (receiveEventData.receiveEventLength > 0)
  {
    switch (receiveEventData.receiveEventRegister)
    {
      case SFE_AAA_REGISTER_I2C_ADDRESS: // Does the user want to change the I2C address?
        if (receiveEventData.receiveEventLength == 3) // Data should be: register; new address; CRC
        {
          if (receiveEventData.receiveEventBuffer[1] == computeCRC8(receiveEventData.receiveEventBuffer, 1)) // Is the CRC valid?
          {
            eeprom_settings.i2cAddress = receiveEventData.receiveEventBuffer[0]; // Update the I2C address in eeprom
            saveEepromSettings(); // Update the address in eeprom
            startI2C(false); // Restart I2C comms
          }
        }
        break;
      case SFE_AAA_REGISTER_RESET_REASON: // Does the user want to read the reset reason?
        if (receiveEventData.receiveEventLength == 1) // Read request should be a single byte
        {
          // Nothing to do here
        }
        break;
      case SFE_AAA_REGISTER_TEMPERATURE: // Does the user want to read the CPU temperature?
        if (receiveEventData.receiveEventLength == 1) // Read request should be a single byte
        {
          readCPUTemperature(); // Update registerTemperature
        }
        break;
      case SFE_AAA_REGISTER_VBAT: // Does the user want to read the battery voltage?
        if (receiveEventData.receiveEventLength == 1) // Read request should be a single byte
        {
          readVBAT(); // Update registerVBAT
        }
        break;
      case SFE_AAA_REGISTER_1V1: // Does the user want to read the 1.1V reference?
        if (receiveEventData.receiveEventLength == 1) // Read request should be a single byte
        {
          read1V1(); // Update register1V1
        }
        break;
      case SFE_AAA_REGISTER_ADC_REFERENCE: // Does the user want to set the ADC reference for readVBAT?
        if (receiveEventData.receiveEventLength == 3) // Data should be: register; new reference; CRC
        {
          if (receiveEventData.receiveEventBuffer[1] == computeCRC8(receiveEventData.receiveEventBuffer, 1)) // Is the CRC valid?
          {
            registerADCReference = receiveEventData.receiveEventBuffer[0]; // Update the ADC reference
          }
        }
        break;
      case SFE_AAA_REGISTER_WDT_PRESCALER: // Does the user want to change the WDT prescaler?
        if (receiveEventData.receiveEventLength == 3) // Data should be: register; new prescaler;  CRC
        {
          if (receiveEventData.receiveEventBuffer[1] == computeCRC8(receiveEventData.receiveEventBuffer, 1)) // Is the CRC valid?
          {
            eeprom_settings.wdtPrescaler = receiveEventData.receiveEventBuffer[0]; // Update the WDT prescaler in eeprom
            saveEepromSettings(); // Update the address in eeprom
            WDTCSR = (WDTCSR & 0xD8) | ((eeprom_settings.wdtPrescaler & 0x08) << 2) | (eeprom_settings.wdtPrescaler & 0x07); // Update the WDT prescaler (bits 5,2,1,0)
          }
        }
        break;
      case SFE_AAA_REGISTER_POWERDOWN_DURATION: // Does the user want to change the power-down duration?
        if (receiveEventData.receiveEventLength == 4) // Data should be: register; new prescaler (uint16_t little endian); CRC
        {
          if (receiveEventData.receiveEventBuffer[2] == computeCRC8(receiveEventData.receiveEventBuffer, 2)) // Is the CRC valid?
          {
            eeprom_settings.powerDownDuration = ((uint16_t)receiveEventData.receiveEventBuffer[1] << 8)
                                                | ((uint16_t)receiveEventData.receiveEventBuffer[0]); // Update the WDT prescaler in eeprom. Data is little endian.
            saveEepromSettings(); // Update the address in eeprom
          }
        }
        break;
      case SFE_AAA_REGISTER_POWERDOWN_NOW: // Does the user want to power-down now?
        if (receiveEventData.receiveEventLength == 7) // Data should be: register; "SLEEP"; CRC
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
          receiveEventData.receiveEventRegister = SFE_AAA_REGISTER_UNKNOWN; // Clear the receive event register - this one is write only
        }
        break;
      case SFE_AAA_REGISTER_FIRMWARE_VERSION: // Does the user want to read the firmware version?
        if (receiveEventData.receiveEventLength == 1) // Read request should be a single byte
        {
          // Nothing to do here
        }
        break;
      case SFE_AAA_REGISTER_UNKNOWN:
      default:
        // Nothing to do here
        break;
    }
    
    receiveEventData.receiveEventLength = 0; // Clear the event
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
void readCPUTemperature()
{
  analogReference(SFE_AAA_ADC_REFERENCE_1V1); //You must use the internal 1.1v bandgap reference when measuring temperature
  analogRead(0x80 | 7); //Do a single read to update the ADC MUX. ADC_TEMPERATURE is ADC channel 7
  noIntDelay(1); // Datasheet says: "After switching to internal voltage reference the ADC requires a settling time of 1ms before measurements are stable."
  
  uint16_t result = 0;
  for (byte x = 0; x < 8; x++)
  {
    result += analogRead(0x80 | 7); //ADC_TEMPERATURE is ADC channel 7
    noIntDelay(1);
  }
  registerTemperature = result >> 3; // Divide result by 8
}

//Measure VCC by measuring the 1.1V bandgap using VCC as the reference
void read1V1()
{
  analogReference(SFE_AAA_ADC_REFERENCE_VCC); //You must use VCC as the reference when measuring the 1.1V reference
  uint16_t result = 0;
  for (byte x = 0; x < 8; x++)
  {
    result += analogRead(0x80 | 5); //The 1.1V reference is ADC channel 5
    noIntDelay(1);
  }
  register1V1 = result >> 3; // Divide result by 8
}

//Measure VBAT
void readVBAT()
{
  analogReference(registerADCReference & 0x01); //Select the desired voltage reference
  if ((registerADCReference & 0x01) == SFE_AAA_ADC_REFERENCE_1V1)
  {
    analogRead(0x80 | 6); //Do a single read to update the ADC MUX. VBAT is ADC channel 6
    noIntDelay(1); // Datasheet says: "After switching to internal voltage reference the ADC requires a settling time of 1ms before measurements are stable."
  }

  uint16_t result = 0;
  for (byte x = 0; x < 8; x++)
  {
    result += analogRead(0x80 | 6); //VBAT is ADC channel 6
    noIntDelay(1);
  }
  
  analogRead(0x80 | 4); //Read the CPU 0V to de-select channel 6 and disconnect the VBAT divide-by-2 circuit
  
  registerVBAT = result >> 3; // Divide result by 8
}

// Compute the CRC8 for the provided data
// This is a duplicate (for volatile byte*) to keep the compiler happy
byte computeCRC8(volatile byte *data, byte len)
{
  byte crc = 0xFF; //Init with 0xFF

  for (byte x = 0; x < len; x++)
  {
    crc ^= data[x]; // XOR-in the next input byte

    for (byte i = 0; i < 8; i++)
    {
      if ((crc & 0x80) != 0)
        crc = (byte)((crc << 1) ^ 0x31);
      else
        crc <<= 1;
    }
  }

  return crc; //No output reflection
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
