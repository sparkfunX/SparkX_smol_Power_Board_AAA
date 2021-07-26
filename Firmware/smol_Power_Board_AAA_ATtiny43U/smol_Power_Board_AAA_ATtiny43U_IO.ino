// smôl Power Board AAA I2C interrupt routines

// I2C receiveEvent
// ================
void receiveEvent(int numberOfBytesReceived)
{
  if (numberOfBytesReceived > 0) // Check that we received some data (!) (hopefully redundant!)
  {
    byte incoming = Wire.read(); // Read the first byte
    receiveEventData.receiveEventRegister = incoming; // Store this byte so we know what to do during the next requestEvent

    if (numberOfBytesReceived > 1) // Did we receive more than one byte?
    {
      int i;
      for (i = 1; (i < numberOfBytesReceived) && (i < TINY_BUFFER_LENGTH); i++) // If we did, store it
      {
        receiveEventData.receiveEventBuffer[i - 1] = Wire.read();
      }
      i--;
      receiveEventData.receiveEventBuffer[i] = 0; // NULL_terminate the data
    }

    receiveEventData.receiveEventLength = numberOfBytesReceived;
  }
}

// I2C requestEvent
// ================
void requestEvent()
{
  byte buff[2];
  switch (receiveEventData.receiveEventRegister)
  {
    case SFE_AAA_REGISTER_I2C_ADDRESS: // Does the user want to read the I2C address?
      Wire.write(eeprom_settings.i2cAddress);
      receiveEventData.receiveEventRegister = SFE_AAA_REGISTER_UNKNOWN; // Clear the event
      break;
    case SFE_AAA_REGISTER_RESET_REASON: // Does the user want to read the reset reason?
      Wire.write(registerResetReason);
      receiveEventData.receiveEventRegister = SFE_AAA_REGISTER_UNKNOWN; // Clear the event
      break;
    case SFE_AAA_REGISTER_TEMPERATURE: // Does the user want to read the CPU temperature?
      buff[0] = registerTemperature & 0xFF; // Little endian
      buff[1] = registerTemperature >> 8;
      Wire.write(buff, 2);
      receiveEventData.receiveEventRegister = SFE_AAA_REGISTER_UNKNOWN; // Clear the event
      break;
    case SFE_AAA_REGISTER_VBAT: // Does the user want to read the battery voltage?
      buff[0] = registerVBAT & 0xFF; // Little endian
      buff[1] = registerVBAT >> 8;
      Wire.write(buff, 2);
      receiveEventData.receiveEventRegister = SFE_AAA_REGISTER_UNKNOWN; // Clear the event
      break;
    case SFE_AAA_REGISTER_VCC_VOLTAGE: // Does the user want to read the VCC voltage?
      buff[0] = registerVCCVoltage & 0xFF; // Little endian
      buff[1] = registerVCCVoltage >> 8;
      Wire.write(buff, 2);
      receiveEventData.receiveEventRegister = SFE_AAA_REGISTER_UNKNOWN; // Clear the event
      break;
    case SFE_AAA_REGISTER_ADC_REFERENCE: // Does the user want to get the ADC reference?
      Wire.write(registerADCReference);
      receiveEventData.receiveEventRegister = SFE_AAA_REGISTER_UNKNOWN; // Clear the event
      break;
    case SFE_AAA_REGISTER_WDT_PRESCALER: // Does the user want to read the WDT prescaler?
      Wire.write(eeprom_settings.wdtPrescaler);
      receiveEventData.receiveEventRegister = SFE_AAA_REGISTER_UNKNOWN; // Clear the event
      break;
    case SFE_AAA_REGISTER_POWERDOWN_DURATION: // Does the user want to read the power-down duration?
      buff[0] = eeprom_settings.powerDownDuration & 0xFF; // Little endian
      buff[1] = eeprom_settings.powerDownDuration >> 8;
      Wire.write(buff, 2);
      receiveEventData.receiveEventRegister = SFE_AAA_REGISTER_UNKNOWN; // Clear the event
      break;
    case SFE_AAA_REGISTER_FIRMWARE_VERSION: // Does the user want to read the firmware version?
      Wire.write(POWER_BOARD_AAA_FIRMWARE_VERSION);
      receiveEventData.receiveEventRegister = SFE_AAA_REGISTER_UNKNOWN; // Clear the event
      break;
    case SFE_AAA_REGISTER_UNKNOWN:
      // Nothing to do here - maybe debug?
      break;
    default:
      receiveEventData.receiveEventRegister = SFE_AAA_REGISTER_UNKNOWN; // Clear the event
      break;
  }
}
