// Targets an ATtiny214

#define F_CPU 3333333
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define PTT PIN3_bm // PB3
#define PA PIN2_bm // PB2
#define TX_INH PIN1_bm // PA1
#define RX PIN2_bm // PA2
#define RELAY PIN3_bm // PA3
#define BUCK_GATE PIN5_bm // PA5
#define BUCK_FB PIN7_bm // PA7 
#define REL_FB PIN6_bm // PA6
#define JP1 PIN0_bm // PB0
#define JP2 PIN1_bm // PB1
#define JP3 PIN4_bm // PA4

#define PTT_ACTIVE ( ((PORTB.IN & PTT) && (jp3_soldered == 0)) || (!(PORTB.IN & PTT) && jp3_soldered) )

volatile uint8_t overload_flag;
volatile uint16_t cyclecount;

ISR(RTC_PIT_vect) { // 8 Hz - check overload

  RTC.PITINTFLAGS = 0x01; // clear flag
  if (overload_flag) {
    if (AC0.STATUS & AC_STATE_bm) // to make sure that we are not stuck above the set point voltage
      TCA0.SINGLE.CTRLA = 0b00000000; // DISABLE switching.
  }
  else {
    overload_flag = 0x01;
    AC0.INTCTRL = 0x01; // enable interrupt
  }
}

ISR(AC0_AC_vect) {
  AC0.STATUS = 0x01; // clear flag
  AC0.INTCTRL = 0x00; // disable interrupt
  overload_flag = 0x00;
}

ISR(PORTA_PORT_vect) {
  PORTA.INTFLAGS = 0x20; // clear flag
  cyclecount++;
}

void main () {

  uint8_t jp1_soldered, jp2_soldered, jp3_soldered;
  
  PORTA.DIR = TX_INH | RX | RELAY | BUCK_GATE; // outputs
  PORTA.PIN7CTRL = 0x04; // digital input disable for AINP0/PA7
  PORTA.OUT = 0x00;
  PORTB.DIR = PA;
  PORTB.PIN0CTRL = 0x08; // enable pull-up for JP1 - solder to disable relay power monitoring
  PORTB.PIN1CTRL = 0x08; // enable pull-up for JP2 - solder to make TX signal INH
  PORTA.PIN4CTRL = 0x08; // enable pull-up for JP3 - solder if PTT active high (n.b. BJT inverter)
  PORTB.PIN3CTRL = 0x08; // enable pull-up for PTT
  PORTA.PIN6CTRL = 0x08; // enable pull-up for REL_FB
  PORTB.OUT = 0x00;

  PORTMUX.CTRLA = 0x01; // Enable EVOUT0
  
  _delay_ms(200);

  if (PORTB.IN & JP1)
    jp1_soldered = 0;
  else
    jp1_soldered = 1;

  if (PORTB.IN & JP2)
    jp2_soldered = 0;
  else
    jp2_soldered = 1;

  if (PORTA.IN & JP3)
    jp3_soldered = 0;
  else
    jp3_soldered = 1;

  TCA0.SINGLE.PER = 67; // 20us period -> 50 kHz
  TCA0.SINGLE.CTRLA = 0b00000001; // DIV1 -> 0.3us/tick, ENABLE
    
  TCB0.CTRLB = 0x16; // Single shot, output enable
  TCB0.EVCTRL = 0x01; // Enable input capture, start counter at pos edge
  TCB0.CCMP = 30; // 10us for 12V in and 3.6A current in 33uH
  TCB0.CTRLA = 0x01; // ENABLE

  AC0.MUXCTRLA = 0b10000011; // Inverted output, DAC to neg in
  VREF.CTRLA = 0x03; // 4.3V
  DAC0.DATA = 151; // 151/255*4.3*(100+10)/10 = 28V
  
  DAC0.CTRLA = 0x01; // ENABLE

  AC0.CTRLA = 0b00100001; // Interrupt at neg edge, ENABLE
  
  EVSYS.SYNCCH0 = 0x02; // TCA0_OVF
  EVSYS.ASYNCCH0 = 0x01; // CCL_LUT0
  //EVSYS.ASYNCUSER8 = 0x01; // SYNCCH0 -> EVOUT0/PA2 to see max switch freq 
  EVSYS.ASYNCUSER2 = 0x01; // SYNCCH0 -> LUT0
  EVSYS.ASYNCUSER0 = 0x03; // ASYNCCH0 -> TCB0
  
  CCL.LUT0CTRLB = 0x63; // AC0 + EVENT0
  CCL.TRUTH0 = 0b10001000; // AND
  CCL.LUT0CTRLA = 0x01; // ENABLE
  CCL.CTRLA = 0x01; // ENABLE

  _delay_ms(100); // wait 100ms after start before enabling overload detection
  
  overload_flag = 0x00;
  
  while(RTC.STATUS);
  RTC.CLKSEL = 0x00; // Use 32 kHz from OSC32K
  while(RTC.PITSTATUS);
  RTC.PITINTCTRL = 0x01; // enable interrupt
  while(RTC.PITSTATUS);
  RTC.PITCTRLA = 0b01011001; // Enable, interrupt after 4096 cycles => 8Hz

  PORTA.OUT |= RX;
  if (jp2_soldered)
    PORTA.OUT |= TX_INH;
  
  sei();
  
  while(1) {
    if (PTT_ACTIVE) {
	_delay_us(10); // filter noise spikes
	if (PTT_ACTIVE) {
	    // go from Receive to Transmit
	    PORTA.OUT &= ~RX;
	    //_delay_ms(10);
	    PORTA.OUT |= RELAY;
	    _delay_ms(25);
	    cyclecount = 0;
	    PORTA.PIN5CTRL = 0x02; // Interrupt at rising edge on gate drive pin
	    _delay_ms(10); // measured to 15ms with the interrupts
	    PORTA.PIN5CTRL = 0x00; // disable interrupt
	    if ( ( ((cyclecount > (uint16_t)38) && (cyclecount < (uint16_t)600)) || jp1_soldered) && !(PORTA.IN & REL_FB)) {
	  // Continue if 4-80 % load on relay or JP1 soldered, and REL_FB low
	      _delay_ms(10);
	      PORTB.OUT |= PA;
	      _delay_ms(10);
	      if (jp2_soldered)
		PORTA.OUT &= ~TX_INH;
	      else
		PORTA.OUT |= TX_INH;
	    }
	    while (PTT_ACTIVE) {
		_delay_ms(1);
	      }
    
	    // from Transmit to Receive
	    if (jp2_soldered)
	      PORTA.OUT |= TX_INH;
	    else
	      PORTA.OUT &= ~TX_INH;
	    _delay_ms(10);
	    PORTB.OUT &= ~PA;
	    _delay_ms(10);
	    PORTA.OUT &= ~RELAY;
	    _delay_ms(20);
	    PORTA.OUT |= RX;
    
	  }
      }
  }
}
