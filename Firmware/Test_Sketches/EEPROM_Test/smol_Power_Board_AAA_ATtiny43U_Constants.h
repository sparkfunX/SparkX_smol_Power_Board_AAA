// smôl Power Board AAA EEPROM constants

//Define the firmware version
#define POWER_BOARD_AAA_FIRMWARE_VERSION    0x10 // v1.0

//Define the ATtiny43's default I2C address
#define DEFAULT_I2C_ADDRESS                 0x50

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
