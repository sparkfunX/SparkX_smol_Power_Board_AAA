@echo Programming the SparkX smôl Power Board AAA. If this looks incorrect, abort and retry.
@pause
:loop
@echo Flashing bootloader and  firmware...
@avrdude -C avrdude.conf -ptattiny43u -cusbtiny -e -Uefuse:w:0xFF:m -Uhfuse:w:0xDF:m -Ulfuse:w:0x62:m -Uflash:w:smolPAAA.hex:i
@echo Done programming! Move on to the next board.
@pause
goto loop
