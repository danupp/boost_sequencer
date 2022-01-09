
#!/bin/bash

~/devel/avr/toolchain/pyupdi/pyupdi.py -d tiny214 -c /dev/ttyUSB0 -f boost_seq.hex

# Set BODLEVEL 4.2V
#~/devel/avr/toolchain/pyupdi/pyupdi.py -d tiny814 -c /dev/ttyUSB0 -fs 1:0xe5

