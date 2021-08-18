#!/bin/bash

GCCPATH="$HOME/devel/avr/toolchain/avr8-gnu-toolchain-linux_x86_64/bin"
BPATH="$HOME/devel/avr/toolchain/Atmel.ATtiny_DFP.1.3.229.atpack_FILES/gcc/dev/attiny214"
IPATH="$HOME/devel/avr/toolchain/Atmel.ATtiny_DFP.1.3.229.atpack_FILES/include"

$GCCPATH/avr-gcc -mmcu=attiny214 -B $BPATH -I $IPATH -O3 main.c -o boost_seq.elf
$GCCPATH/avr-size boost_seq.elf
$GCCPATH/avr-objcopy -O ihex boost_seq.elf boost_seq.hex
