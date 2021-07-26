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

  The firmware register addresses are:

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
struct {
  byte receiveEventRegister = SFE_AAA_REGISTER_UNKNOWN; // Most recent receive event register address
  byte receiveEventBuffer[16]; // byte array to store the data from the receive event
  bool receiveEventDetected = false; // Flag to indicate if a receive event has taken place
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

  //Begin listening on I2C
  startI2C(true); // Skip Wire.end the first time around
}

void loop()
{
  if (LOW_POWER_MODE) // Is low power mode enabled?
  {
    // If low power mode is enabled, put the 841 into power-down mode after sleep_after millis of inactivity
    if (millis() > (last_activity + sleep_after)) // Have we reached the inactivity limit?
    {
      goToSleep(); // Put the 841 into power-down mode
      // ZZZzzz...
      last_activity = millis(); // After waking, update last_activity so we stay awake for at least sleep_after millis
    }
  }
  noIntDelay(1); // Delay for 1msec to avoid thrashing millis()
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
  analogReference(INTERNAL); //You must use the internal 1.1v bandgap reference when measuring temperature
  uint16_t result = 0;
  for (byte x = 0; x < 8; x++)
  {
    result += analogRead(ADC_TEMPERATURE); //ADC_TEMPERATURE is #defined to be the channel for reading the temperature
    noIntDelay(1);
  }
  return (result >> 3); // Divide result by 8
}

//goToSleep adapted from: https://gist.github.com/JChristensen/5616922
//Hardwired for the ATtiny43U (may not work on other ATtiny's)
//Brown Out Detection is disabled via the fuse bits:
//Set BOD Level to BOD Disabled in the board settings.
void goToSleep(void)
{
    byte adcsra = ADCSRA; //save ADCSRA (ADC control and status register A)
    ADCSRA &= ~_BV(ADEN); //disable ADC by clearing the ADEN bit
    
    byte acsr0a = ACSR0A; //save ACSR0A (Analog Comparator 0 control and status register)
    ACSR0A &= ~_BV(ACIE0); //disable AC0 interrupt
    ACSR0A |= _BV(ACD0); //disable ACO by setting the ACD0 bit
    
    byte acsr1a = ACSR1A; //save ACSR1A (Analog Comparator 1 control and status register)
    ACSR1A &= ~_BV(ACIE1); //disable AC1 interrupt
    ACSR1A |= _BV(ACD1); //disable AC1 by setting the ACD1 bit
    
    byte prr = PRR; // Save the power reduction register
    // Disable the ADC, USART1, SPI, Timer1 and Timer2
    // (Leave TWI, USART0 and Timer0 enabled)
    PRR |= _BV(PRADC) | _BV(PRUSART1) | _BV(PRSPI) | _BV(PRTIM1) | _BV(PRTIM2); 
    
    byte mcucr = MCUCR; // Save the MCU Control Register
    // Set Sleep Enable (SE=1), Power-down Sleep Mode (SM1=1, SM0=0), INT0 Falling Edge (ISC01=1, ISC00=0)
    MCUCR = _BV(SE) | _BV(SM1) | _BV(ISC01);

    sleep_cpu(); //go to sleep
    
    MCUCR = mcucr; // Restore the MCU control register
    PRR = prr; // Restore the power reduction register
    ACSR1A = acsr1a; // Restore ACSR1A    
    ACSR0A = acsr0a; // Restore ACSR0A    
    ADCSRA = adcsra; //Restore ADCSRA
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
