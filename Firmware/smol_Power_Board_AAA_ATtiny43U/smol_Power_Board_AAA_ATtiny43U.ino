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
  Set BOD Level to BOD Enabled 1.8V
  Set millis()/micros():"Enabled" to Enabled
  Set Save EEPROM to EEPROM Not Retained
  Set BOD Level to BOD Disabled (this helps reduce power consumption in low power mode)

  Fuse byte settings:
  Low:  0b11100010 = 0xE2 : no clock divider, no clock out, slowly rising power, 8MHz internal oscillator
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
//#include <avr/power.h> //Needed for powering down perihperals such as the ADC/TWI and Timers

//Define the firmware version
#define POWER_BOARD_AAA_FIRMWARE_VERSION    0x10 // v1.0

//Define the ATtiny43's default I2C address
const uint8_t DEFAULT_I2C_ADDRESS = 0x50;

//Define the WDT prescaler settings
#define SFE_AAA_WDT_PRESCALE_2K             0x00 // 2K cycles = 16ms timeout
#define SFE_AAA_WDT_PRESCALE_4K             0x01 // 4K cycles = 32ms timeout
#define SFE_AAA_WDT_PRESCALE_8K             0x02 // 8K cycles = 64ms timeout
#define SFE_AAA_WDT_PRESCALE_16K            0x03 // 16K cycles = 0.125s timeout
#define SFE_AAA_WDT_PRESCALE_32K            0x04 // 32K cycles = 0.25s timeout
#define SFE_AAA_WDT_PRESCALE_64K            0x05 // 64K cycles = 0.5s timeout
#define SFE_AAA_WDT_PRESCALE_128K           0x06 // 128K cycles = 1.0s timeout
#define SFE_AAA_WDT_PRESCALE_256K           0x07 // 256K cycles = 2.0s timeout
#define SFE_AAA_WDT_PRESCALE_512K           0x08 // 512K cycles = 4.0s timeout
#define SFE_AAA_WDT_PRESCALE_1024K          0x09 // 1024K cycles = 8.0s timeout
#define DEFAULT_WDT_PRESCALER SFE_AAA_WDT_PRESCALE_128K

//Define the firmware register addresses:
#define SFE_AAA_REGISTER_I2C_ADDRESS        0x00 // byte     Read/Write: Stored in eeprom
#define SFE_AAA_REGISTER_RESET_REASON       0x01 // byte     Read only
#define SFE_AAA_REGISTER_TEMPERATURE        0x02 // uint16_t Read only
#define SFE_AAA_REGISTER_VBAT               0x03 // uint16_t Read only
#define SFE_AAA_REGISTER_VCC_VOLTAGE        0x04 // uint16_t Read only
#define SFE_AAA_REGISTER_ADC_REFERENCE      0x05 // byte     Read/Write
#define SFE_AAA_REGISTER_WDT_PRESCALER      0x06 // byte     Read/Write: Stored in eeprom
#define SFE_AAA_REGISTER_POWERDOWN_DURATION 0x07 // uint16_t Read/Write: Stored in eeprom
#define SFE_AAA_REGISTER_POWERDOWN_NOW      0x08 //          Write only (Sequence is: "SLEEP" + CRC)
#define SFE_AAA_REGISTER_FIRMWARE_VERSION   0x09 // byte     Read only

//Digital pins
const byte EN_3V3 = 11; // 3V3 Regulator Enable: pull high to enable 3.3V, pull low to disable

//Define the ON and OFF states for each pin
#define EN_3V3__ON   HIGH // 3V3 Regulator Enable: pull high to enable 3.3V, pull low to disable
#define EN_3V3__OFF  LOW  // 3V3 Regulator Enable: pull high to enable 3.3V, pull low to disable

//Global variables

//WDT Interrupt Service Routine - controls when the ATtiny43U wakes from sleep
void wdtISR() {}

void setup()
{
  pinMode(EN_3V3, OUTPUT); // Disable the 9603N until PGOOD has gone high
  digitalWrite(EN_3V3, EN_3V3__ON);

  //Begin listening on I2C
  startI2C();

  //Initialise last_activity
  last_register_address = millis();
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
void startI2C()
{
  Wire.end(); //Before we can change addresses we need to stop

  Wire.begin(I2C_ADDRESS); //Do the Wire.begin using the defined I2C_ADDRESS

  //The connections to the interrupts are severed when a Wire.begin occurs. So re-declare them.
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
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
