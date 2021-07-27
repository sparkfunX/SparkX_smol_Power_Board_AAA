// smôl Power Board AAA WDT functions

#define ATTINY43_WDTCSR_WDE_BIT   3     // Watchdog Enable bit position
#define ATTINY43_WDTCSR_WDE       ( 1 << ATTINY43_WDTCSR_WDE_BIT )
#define ATTINY43_WDTCSR_WDCE_BIT  4     // Watchdog Change Enable bit position
#define ATTINY43_WDTCSR_WDCE      ( 1 << ATTINY43_WDTCSR_WDCE_BIT )
#define ATTINY43_WDTCSR_WDP3_BIT  5     // Watchdog Prescaler bit 3
#define ATTINY43_WDTCSR_WDP3      ( 1 << ATTINY43_WDTCSR_WDP3_BIT )
#define ATTINY43_WDTCSR_WDIE_BIT  6     // Watchdog Timeout Interrupt Enable bit position
#define ATTINY43_WDTCSR_WDIE      ( 1 << ATTINY43_WDTCSR_WDIE_BIT )
#define ATTINY43_WDTCSR_WDIF_BIT  7     // Watchdog Timeout Interrupt Flag bit position
#define ATTINY43_WDTCSR_WDIF      ( 1 << ATTINY43_WDTCSR_WDIF_BIT )
