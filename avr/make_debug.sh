#!/bin/bash

GCCPATH="$HOME/devel/avr/microchip_toolchain/avr8-gnu-toolchain-linux_x86_64/bin"
BPATH="$HOME/devel/avr/Atmel.ATtiny_DFP.1.3.229.atpack/gcc/dev/attiny402"
IPATH="$HOME/devel/avr/Atmel.ATtiny_DFP.1.3.229.atpack/include"

$GCCPATH/avr-gcc -mmcu=attiny402 -B $BPATH -I $IPATH -D UART_DEBUG -O3 main.c nfc.c -o pulsecount.elf
$GCCPATH/avr-size pulsecount.elf
$GCCPATH/avr-objcopy -O ihex pulsecount.elf pulsecount.hex
