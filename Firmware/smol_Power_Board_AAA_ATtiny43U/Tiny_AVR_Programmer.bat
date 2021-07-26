@echo Programming the SparkX Qwiic Iridium. If this looks incorrect, abort and retry.
@pause
:loop
@echo Flashing bootloader and  firmware...
@avrdude -C avrdude.conf -ptattiny43u -cusbtiny -e -Uefuse:w:0xFF:m -Uhfuse:w:0xDF:m -Ulfuse:w:0xE2:m -Uflash:w:smol_Power_Board_AAA_ATtiny43U.ino.hex:i
@echo Done programming! Move on to the next board.
@pause
goto loop
