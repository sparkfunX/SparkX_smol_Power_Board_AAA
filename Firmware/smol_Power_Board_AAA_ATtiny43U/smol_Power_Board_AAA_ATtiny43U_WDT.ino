// smôl Power Board AAA WDT functions

// Based on https://electronics.stackexchange.com/questions/74840/use-avr-watchdog-like-normal-isr

// ISR for watchdog timer
ISR(WDT_vect) {
  sleep_disable(); // Clear the Sleep Enable (SE) bit in the MCUCR register
  // To avoid the Watchdog Reset, WDIE must be set after each interrupt.
  volatile byte wdtBits = ATTINY43_WDTCSR_WDIE | ATTINY43_WDTCSR_WDE | ((eeprom_settings.wdtPrescaler & 0x08) << 2)
                          | (eeprom_settings.wdtPrescaler & 0x07); // WDT Interrupt Enable OR'd with WDE and the four prescaler bits
  WDTCSR |= ATTINY43_WDTCSR_WDCE | ATTINY43_WDTCSR_WDE; // We need to set WDCE when changing the prescaler bits
  WDTCSR = wdtBits; // Enable the WDT interrupt and set the prescaler
  if (powerDownDuration > 0) // Decrement powerDownDuration
    powerDownDuration--;
  sleep_enable(); // Set the Sleep Enable (SE) bit in the MCUCR register so sleep is possible
}

// Disable the WDT
void disableWDT()
{
  cli(); // Disable interrupts
  WDTCSR |= ATTINY43_WDTCSR_WDCE | ATTINY43_WDTCSR_WDE; // In the same operation, write a logic one to WDCE and WDE
  WDTCSR = 0x00; // Within the next four clock cycles, write a logic 0 to WDE. This disables the Watchdog. Also disables the interrupt.
  sei(); // Enable interrupts
}

// Enable the WDT:
// Enable both the Watchdog Timeout Interrupt (WDIE) _and_ the Watchdog itself (WDE)
// The WDT will generate an interrupt after a time-out of prescaler cycles
// The WDIE bit needs to be (re)set by the ISR to prevent the WDT from resetting the CPU at the next time-out
void enableWDT()
{
  cli(); // Disable interrupts
  volatile byte wdtBits = ATTINY43_WDTCSR_WDIE | ATTINY43_WDTCSR_WDE | ((eeprom_settings.wdtPrescaler & 0x08) << 2)
                          | (eeprom_settings.wdtPrescaler & 0x07); // WDT Interrupt Enable OR'd with WDE and the four prescaler bits
  WDTCSR |= ATTINY43_WDTCSR_WDCE | ATTINY43_WDTCSR_WDE; // We need to set WDCE when changing the prescaler bits
  WDTCSR = wdtBits; // Enable the WDT interrupt and set the prescaler
  sei(); // Enable interrupts
}
