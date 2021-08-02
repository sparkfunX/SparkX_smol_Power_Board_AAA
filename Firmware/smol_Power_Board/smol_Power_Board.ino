/*
  smôl Power Board Firmware
  By: Paul Clark
  Date: July 29th 2021
  Version: 1.0

  This firmware runs on both the smôl Power Board AAA (ATtiny43U) and the smôl Power Board LiPo (ATtiny841).
  Uncomment the "#define POWER_BOARD_LIPO" below to compile for the LiPo board.

  Uses Spence Konde's ATTinyCore boards:
  https://github.com/SpenceKonde/ATTinyCore
  https://github.com/SpenceKonde/ATTinyCore/blob/master/Installation.md
  Add this URL to the Additional Boards Manager URLs in File\Preferences:
  http://drazzy.com/package_drazzy.com_index.json
  and then use the Boards Manager to add support for the ATtiny: http://boardsmanager/All#ATTinyCore

  The Power Board's default I2C address is always 0x50. The address is stored in eeprom and can be changed.
  See the smôl Power Board Arduino Library Example4 for more details.

  smôl Power Board AAA (ATtiny43U)
  ================================

  Set Board to ATtiny43 (No bootloader)
  Set Clock to 4MHz(internal)
  Set millis()/micros() to Disabled
  Set Save EEPROM to EEPROM Not Retained
  Set BOD Level to BOD Disabled (this helps reduce power consumption in low power mode)

  Fuse byte settings (default to a 1MHz clock, the code will increase the clock to 4MHz):
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


  smôl Power Board LiPo (ATtiny841)
  =================================

  Uncomment the "#define POWER_BOARD_LIPO" below to compile for the LiPo board.

  Set Board to ATtiny441/841 (No bootloader)
  Set Chip to ATtiny841
  Set Clock Source to 4MHz (internal, Vcc < 4.5)
  Set Pin Mapping to Clockwise
  Set Wire Mode to Slave Only
  Set millis()/micros() to Disabled
  Set Save EEPROM to EEPROM Not Retained
  Set BOD Mode (Active) to BOD Disabled (this helps reduce power consumption in low power mode)
  Set BOD Mode (Sleep) to BOD Disabled (this helps reduce power consumption in low power mode)

  Fuse byte settings (default to a 1MHz clock, the code will increase the clock to 4MHz):
  Low:  0b01000010 = 0x42 : divide clock by 8, no clock out, slowly rising power, 8MHz internal oscillator
  High: 0b11011111 = 0xDF : reset not disabled, no debug wire, SPI programming enabled, WDT not always on, EEPROM not preserved, BOD 1.8V
  Ext:  0xFF : ULP Osc 32kHz, BOD (Sleep) disabled, BOD (Active) disabled, no self-programming

  Pin Allocation (Physical pins are for the QFN package):
  D0:  Physical Pin 5  (PA0 / A0)          : 3V3 Regulator Enable
  D1:  Physical Pin 4  (PA1 / A1/TXD0)     : TXD0 Test Point
  D2:  Physical Pin 3  (PA2 / A2/RXD0)     : RXD0 Test Point
  D3:  Physical Pin 2  (PA3 / A3)          : Not used
  D4:  Physical Pin 1  (PA4 / A4/SCK/SCL)  : I2C SCL
  D5:  Physical Pin 20 (PA5 / A5/CIPO)     : CIPO - connected to pin 1 of the ISP header
  D6:  Physical Pin 16 (PA6 / A6/COPI/SDA) : I2C SDA
  D7:  Physical Pin 15 (PA7 / A7)          : Not used
  D8:  Physical Pin 14 (PB2 / A8)          : Not used
  D11: Physical Pin 13 (PB3 / A9/!RESET)   : !RESET
  D9:  Physical Pin 12 (PB1 / A10/INT0)    : Not used
  D10: Physical Pin 11 (PB0 / A11)         : Not used

*/

//#define POWER_BOARD_LIPO // Uncomment this line to compile the firmware for the smôl Power Board LiPo (ATtiny841)

#include <Wire.h>

#include <avr/sleep.h> //Needed for sleep_mode
#include <avr/power.h> //Needed for powering down perihperals such as the ADC
#include <avr/wdt.h>

#include <EEPROM.h>

#include "smol_Power_Board_Constants.h"
#include "smol_Power_Board_EEPROM.h"

//Digital pins
#ifdef POWER_BOARD_LIPO
const byte EN_3V3 = 0; // 3V3 Regulator Enable: pull high to enable 3.3V, pull low to disable
const byte SDA_PIN = 6;
const byte SCL_PIN = 4;
#else
const byte EN_3V3 = 11; // 3V3 Regulator Enable: pull high to enable 3.3V, pull low to disable
const byte SDA_PIN = 4;
const byte SCL_PIN = 6;
#endif

//Define the ON and OFF states for each pin
#define EN_3V3__ON   HIGH // 3V3 Regulator Enable: pull high to enable 3.3V, pull low to disable
#define EN_3V3__OFF  LOW  // 3V3 Regulator Enable: pull high to enable 3.3V, pull low to disable

//Global variables
#define TINY_BUFFER_LENGTH 16
struct {
  volatile byte receiveEventRegister = SFE_SMOL_POWER_REGISTER_UNKNOWN; // Most recent receive event register address
  volatile byte receiveEventBuffer[TINY_BUFFER_LENGTH]; // byte array to store the data from the receive event
  volatile byte receiveEventLength = 0; // Indicates how many data bytes were received
} receiveEventData;
volatile byte registerResetReason;
volatile uint16_t registerTemperature;
volatile uint16_t registerVBAT; // Not supported on the 
volatile uint16_t register1V1;
volatile byte registerADCReference = SFE_SMOL_POWER_ADC_REFERENCE_VCC; // Default to using VCC as the ADC reference
volatile uint16_t powerDownDuration;
volatile bool sleepNow = false;

void setup()
{
  //Just in case, make sure the system clock is set to 4MHz (8MHz divided by 2)
  cli(); // Disable interrupts
#ifdef POWER_BOARD_LIPO
  CCP = 0xD8; // Write signature (0xD8) to the Configuration Change Register
#else
  CLKPR = 1 << CLKPCE; //Set the clock prescaler change enable bit
#endif
  CLKPR = 1; //Set clock prescaler CLKPS bits to 1 == divide by 2
  sei(); // Enable interrupts

  pinMode(EN_3V3, OUTPUT); // Enable the 3V3 regulator for the smôl bus
  digitalWrite(EN_3V3, EN_3V3__ON);

  registerResetReason = (MCUSR & 0x0F); // Record the reset reason from MCUSR
  MCUSR = 0; // Clear the MCUSR

  disableWDT(); // Make sure the WDT is disabled

  if (!loadEepromSettings()) // Load the settings from eeprom
  {
    initializeEepromSettings(); // Initialize them if required
    registerResetReason |= SFE_SMOL_POWER_EEPROM_CORRUPT_ON_RESET; // Flag that the eeprom was corrupt and needed to be initialized
  }

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
      case SFE_SMOL_POWER_REGISTER_I2C_ADDRESS: // Does the user want to change the I2C address?
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
      case SFE_SMOL_POWER_REGISTER_RESET_REASON: // Does the user want to read the reset reason?
        if (receiveEventData.receiveEventLength == 1) // Read request should be a single byte
        {
          // Nothing to do here
        }
        break;
      case SFE_SMOL_POWER_REGISTER_TEMPERATURE: // Does the user want to read the CPU temperature?
        if (receiveEventData.receiveEventLength == 1) // Read request should be a single byte
        {
          readCPUTemperature(); // Update registerTemperature
        }
        break;
      case SFE_SMOL_POWER_REGISTER_VBAT: // Does the user want to read the battery voltage?
        if (receiveEventData.receiveEventLength == 1) // Read request should be a single byte
        {
          readVBAT(); // Update registerVBAT
        }
        break;
      case SFE_SMOL_POWER_REGISTER_1V1: // Does the user want to read the 1.1V reference?
        if (receiveEventData.receiveEventLength == 1) // Read request should be a single byte
        {
          read1V1(); // Update register1V1
        }
        break;
      case SFE_SMOL_POWER_REGISTER_ADC_REFERENCE: // Does the user want to set the ADC reference for readVBAT?
        if (receiveEventData.receiveEventLength == 3) // Data should be: register; new reference; CRC
        {
          if (receiveEventData.receiveEventBuffer[1] == computeCRC8(receiveEventData.receiveEventBuffer, 1)) // Is the CRC valid?
          {
            registerADCReference = receiveEventData.receiveEventBuffer[0]; // Update the ADC reference
          }
        }
        break;
      case SFE_SMOL_POWER_REGISTER_WDT_PRESCALER: // Does the user want to change the WDT prescaler?
        if (receiveEventData.receiveEventLength == 3) // Data should be: register; new prescaler;  CRC
        {
          if (receiveEventData.receiveEventBuffer[1] == computeCRC8(receiveEventData.receiveEventBuffer, 1)) // Is the CRC valid?
          {
            eeprom_settings.wdtPrescaler = receiveEventData.receiveEventBuffer[0]; // Update the WDT prescaler in eeprom
            saveEepromSettings(); // Update the address in eeprom
          }
        }
        break;
      case SFE_SMOL_POWER_REGISTER_POWERDOWN_DURATION: // Does the user want to change the power-down duration?
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
      case SFE_SMOL_POWER_REGISTER_POWERDOWN_NOW: // Does the user want to power-down now?
        if (receiveEventData.receiveEventLength == 7) // Data should be: register; "SLEEP"; CRC
        {
          if (receiveEventData.receiveEventBuffer[5] == computeCRC8(receiveEventData.receiveEventBuffer, 5)) // Is the CRC valid?
          {
            if ((receiveEventData.receiveEventBuffer[0] == 'S') && (receiveEventData.receiveEventBuffer[1] == 'L')
                && (receiveEventData.receiveEventBuffer[2] == 'E') && (receiveEventData.receiveEventBuffer[3] == 'E')
                && (receiveEventData.receiveEventBuffer[4] == 'P'))
            {
              sleepNow = true; // Go to sleep
            }
          }
          receiveEventData.receiveEventRegister = SFE_SMOL_POWER_REGISTER_UNKNOWN; // Clear the receive event register - this one is write only
        }
        break;
      case SFE_SMOL_POWER_REGISTER_FIRMWARE_VERSION: // Does the user want to read the firmware version?
        if (receiveEventData.receiveEventLength == 1) // Read request should be a single byte
        {
          // Nothing to do here
        }
        break;
      case SFE_SMOL_POWER_REGISTER_UNKNOWN:
      default:
        // Nothing to do here
        break;
    }
    
    receiveEventData.receiveEventLength = 0; // Clear the event
  }

  // Sleep - turn off the smôl 3.3V regulator and put the ATtiny into as low a power state as possible
  // The WDT will wake the processor once powerDownDuration WDT ticks have expired
  if (sleepNow)
  {
    powerDownDuration = eeprom_settings.powerDownDuration; // Load powerDownDuration with the value from eeprom
    if (powerDownDuration > 0) // Sanity check - don't glitch the power if powerDownDuration is zero
    {
      Wire.end(); // Stop I2C (USI)
      set_sleep_mode(SLEEP_MODE_PWR_DOWN); // Go into power-down mode when sleeping (only WDT Int and INT0 can wake the processor)
#ifdef POWER_BOARD_LIPO
      analogRead(0x80 | 0x0E); //Read the CPU 0V (channel 14)
#else
      analogRead(0x80 | 4); //Read the CPU 0V to make sure channel 6 is disconnected (channel 6 has the VBAT divide-by-2 circuit which will draw current)
#endif
      ADCSRA &= (~(1 << ADEN)); // Disable the ADC
#ifndef POWER_BOARD_LIPO
      byte ACSRstatus = ACSR; // Record the Analog Comparator Status Register
      ACSR &= (~(1 << ACBG)); // Ensure the comparator bandgap is deselected
      ACSR |= (1 << ACD); // Set the comparator disable bit
#endif
#ifdef POWER_BOARD_LIPO
      PRR |= 0xFF; // Power-down the TWI, UART1, UART0, SPI, TC2, TC1, TC0, ADC
#else
      PRR |= 0x0F; // Power-down the ADC, USI and both Timer/Counters
#endif
      digitalWrite(EN_3V3, EN_3V3__OFF); // Turn off the 3.3V regulator
      enableWDT(); // Enable the WDT using the prescaler setting from eeprom
      sleep_enable(); // Set the Sleep Enable (SE) bit in the MCUCR register so sleep is possible
      while (powerDownDuration > 0)
        sleep_cpu(); // Go to sleep. WDT ISR will decrement powerDownDuration
      sleep_disable(); // Clear the Sleep Enable (SE) bit in the MCUCR register
      disableWDT(); // Disable the WDT
      digitalWrite(EN_3V3, EN_3V3__ON); // Turn on the 3.3V regulator
#ifdef POWER_BOARD_LIPO
      PRR &= 0x00; // Power-up the TWI, UART1, UART0, SPI, TC2, TC1, TC0, ADC
#else
      PRR &= 0xF0; // Power-up the ADC, USI and both Timer/Counters
#endif
#ifndef POWER_BOARD_LIPO
      ACSR = ACSRstatus; // Re-enable the comparator if required
#endif
      noIntDelay(4); // Give the clocks time to start
      ADCSRA |= (1 << ADEN); // Re-enable the ADC
#ifdef POWER_BOARD_LIPO
      analogRead(0x80 | 0x0E); //Read the CPU 0V (channel 14)
#else
      analogRead(0x80 | 4); // Read the CPU 0V. When the ADC is turned off and on again, the next conversion will be an extended conversion.
#endif
      startI2C(true); // Re-initialize the USI
    }
    sleepNow = false; // Clear sleepNow
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
  analogReference(SFE_SMOL_POWER_ADC_REFERENCE_1V1); //You must use the internal 1.1v bandgap reference when measuring temperature
#ifdef POWER_BOARD_LIPO
  analogRead(0x80 | 0x0C); //Do a single read to update the ADC MUX. ADC_TEMPERATURE is ADC channel 12
#else
  analogRead(0x80 | 7); //Do a single read to update the ADC MUX. ADC_TEMPERATURE is ADC channel 7
#endif
  noIntDelay(1); // Datasheet says: "After switching to internal voltage reference the ADC requires a settling time of 1ms before measurements are stable."
  
  uint16_t result = 0;
  for (byte x = 0; x < 8; x++)
  {
#ifdef POWER_BOARD_LIPO
    result += analogRead(0x80 | 0x0C); //ADC_TEMPERATURE is ADC channel 12
#else
    result += analogRead(0x80 | 7); //ADC_TEMPERATURE is ADC channel 7
#endif
    noIntDelay(1);
  }
  registerTemperature = result >> 3; // Divide result by 8
}

//Measure VCC by measuring the 1.1V bandgap using VCC as the reference
void read1V1()
{
  analogReference(SFE_SMOL_POWER_ADC_REFERENCE_VCC); //You must use VCC as the reference when measuring the 1.1V reference
  uint16_t result = 0;
  for (byte x = 0; x < 8; x++)
  {
#ifdef POWER_BOARD_LIPO
    result += analogRead(0x80 | 0x0D); //The 1.1V reference is ADC channel 13
#else
    result += analogRead(0x80 | 5); //The 1.1V reference is ADC channel 5
#endif
    noIntDelay(1);
  }
  register1V1 = result >> 3; // Divide result by 8
}

//Measure VBAT - not supported on the Power Board LiPo
void readVBAT()
{
#ifdef POWER_BOARD_LIPO
  registerVBAT = 0;
#else
  analogReference(registerADCReference & 0x01); //Select the desired voltage reference
  if ((registerADCReference & 0x01) == SFE_SMOL_POWER_ADC_REFERENCE_1V1)
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
#endif
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
