// Targets an ATtiny416

//#define DEBUG

#define F_CPU 5000000
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>

#ifdef DEBUG
//#include <stdio.h>
//#include <string.h>
#endif

#define PTT PIN3_bm // PB3
#define PA PIN1_bm // PC1
#define TX_INH PIN0_bm // PC0
#define RX PIN5_bm // PB5
#define RELAY PIN1_bm // PA1
#define REL_L_TX PIN2_bm // PA2
#define REL_L_RX PIN3_bm // PA3
#define BUCK_GATE PIN5_bm // PA5
#define STOP_NTC PIN6_bm // PA6
#define BUCK_FB PIN7_bm // PA7 
#define REL_FB_TX PIN3_bm // PC3
#define REL_FB_RX PIN2_bm // PC2
#define FAN PIN1_bm // PB1
#define JP1 PIN4_bm // PA4    -- PTT active high
#define JP2 PIN2_bm // PB2    -- TX / INH
#define JP3 PIN4_bm // PB4    -- Require relay load
#define JP4 PIN0_bm // PB0    -- Precise relay load monitoring
#ifdef DEBUG
#define TXD PIN2_bm // PB2    -- debug only
#endif

#define LOADPOINT_EEADDR (int*) 0

enum states {
  alloff,
  rx,
  relay_activate,
  relay_fail,
  relay_deactivate,
  pa,
  tx,
  stopped
};

volatile uint8_t rellatch_timer, ntc_timer = 0;
volatile uint8_t ntc_flag;
volatile int cyclecount;
volatile uint16_t FAN_timer;
uint8_t jp1_soldered, jp2_soldered, jp3_soldered, jp4_soldered;

#ifdef DEBUG
void USART_Transmit_String(char *buffer) {
  uint8_t n = 0;
  while(buffer[n]) {
    while (!(USART0.STATUS & USART_DREIF_bm));
    USART0.TXDATAL = buffer[n];
    n++;
  }
}

uint8_t inttostr(char *buff,uint16_t val) {  // to 9999 format
  uint8_t dig_1 = (uint8_t)(val/1000);
  uint8_t dig_2 = (uint8_t)((val-dig_1*1000)/100);
  uint8_t dig_3 = (uint8_t)((val-dig_1*1000-dig_2*100)/10);
  uint8_t dig_4 = (uint8_t)(val-dig_1*1000-dig_2*100-dig_3*10);
  uint8_t p = 0;
  
  if (dig_1) {
    *buff = (char)(48+dig_1);
    p++;
  }
  if (dig_1 || dig_2) {
    *(buff+p) = (char)(48+dig_2);
    p++;
  }
  if (dig_1 || dig_2 || dig_3) {
    *(buff+p) = (char)(48+dig_3);
    p++;
  }
  *(buff+p) = (char)(48+dig_4);
  p++;
  *(buff+p) = (char)(0x00);
  return p; // num characters
}
#endif


/* *** Simple pin-related functions *** */
/* *** As #define lines saves mem *** */
#define PIN_FUNCTIONS_AS_DEFINE

#ifdef PIN_FUNCTIONS_AS_DEFINE
#define PTT_ACTIVE() ( ((PORTB.IN & PTT) && !jp1_soldered) || (!(PORTB.IN & PTT) && jp1_soldered) )
#else
uint8_t PTT_ACTIVE () {  // Note BJT inv between PTT and pin on MCU
  return ((PORTB.IN & PTT) && !jp1_soldered) || (!(PORTB.IN & PTT) && jp1_soldered);
}
#endif

#ifdef PIN_FUNCTIONS_AS_DEFINE
#define RX_on() PORTB.OUT |= RX
#else
void RX_on() {
  PORTB.OUT |= RX;
}
#endif

#ifdef PIN_FUNCTIONS_AS_DEFINE
#define RX_off() PORTB.OUT &= ~RX
#else
void RX_off() {
  PORTB.OUT &= ~RX;
}
#endif

#ifdef PIN_FUNCTIONS_AS_DEFINE
#define REL_on() PORTA.OUT |= RELAY
#else
void REL_on() {
  PORTA.OUT |= RELAY;
}
#endif

#ifdef PIN_FUNCTIONS_AS_DEFINE
#define REL_off() PORTA.OUT &= ~RELAY
#else
void REL_off() {
  PORTA.OUT &= ~RELAY;
}
#endif

#ifdef PIN_FUNCTIONS_AS_DEFINE
#define RELLATCH_RX() PORTA.OUT |= REL_L_RX; PORTA.OUT &= ~REL_L_TX; rellatch_timer = 0
#else
void RELLATCH_RX() {
  PORTA.OUT |= REL_L_RX;
  PORTA.OUT &= ~REL_L_TX;
  rellatch_timer = 0;
}
#endif

#ifdef PIN_FUNCTIONS_AS_DEFINE
#define RELLATCH_TX() PORTA.OUT |= REL_L_TX; PORTA.OUT &= ~REL_L_RX; rellatch_timer = 0
#else
void RELLATCH_TX() {
  PORTA.OUT |= REL_L_TX;
  PORTA.OUT &= ~REL_L_RX;
  rellatch_timer = 0;
}
#endif

#ifdef PIN_FUNCTIONS_AS_DEFINE
#define RELLATCH_OFF() PORTA.OUT &= ~(REL_L_TX | REL_L_RX)
#else
void RELLATCH_OFF() {
  PORTA.OUT &= ~(REL_L_TX | REL_L_RX);
}
#endif

#ifdef PIN_FUNCTIONS_AS_DEFINE
#define PA_on() PORTC.OUT |= PA
#else
void PA_on() {
  PORTC.OUT |= PA;
}
#endif

#ifdef PIN_FUNCTIONS_AS_DEFINE
#define PA_off() PORTC.OUT &= ~PA
#else
void PA_off() {
  PORTC.OUT &= ~PA;
}
#endif

#ifdef PIN_FUNCTIONS_AS_DEFINE
#define TX_on() if (jp2_soldered) PORTC.OUT &= ~TX_INH; else PORTC.OUT |= TX_INH
#else
void TX_on() {
  if (jp2_soldered)
    PORTC.OUT &= ~TX_INH;
  else
    PORTC.OUT |= TX_INH;
}
#endif

#ifdef PIN_FUNCTIONS_AS_DEFINE
#define TX_off() if (jp2_soldered) PORTC.OUT |= TX_INH; else PORTC.OUT &= ~TX_INH
#else
void TX_off() {
  if (jp2_soldered)
    PORTC.OUT |= TX_INH;
  else
    PORTC.OUT &= ~TX_INH;
}
#endif

#ifdef PIN_FUNCTIONS_AS_DEFINE
#define FAN_run() PORTB.OUT |= FAN; FAN_timer = 0
#else
void FAN_run() {
  PORTB.OUT |= FAN;
  FAN_timer = 0;
}
#endif

#ifdef PIN_FUNCTIONS_AS_DEFINE
#define FAN_off() PORTB.OUT &= ~FAN
#else
void FAN_off() {
  PORTB.OUT &= ~FAN;
}
#endif

/* *** Support functions *** */

void RTC_delay(uint16_t deltime) { // units of 1/32768 s
  static uint16_t timer;
  timer = RTC.CNT;
  while (RTC.CNT - timer < deltime);
}

uint8_t RTC_delay_PTT(uint16_t deltime, uint8_t PTT_state_for_return) { // units of 1/32768 s
  static uint16_t timer;
  timer = RTC.CNT;
  while (RTC.CNT - timer < deltime) {
    if ((PTT_state_for_return && PTT_ACTIVE()) || (!PTT_state_for_return && !PTT_ACTIVE()))
      return 1;
  }
  return 0;
}

/* *** ISRs *** */

ISR(RTC_PIT_vect) { // 250 Hz
  RTC.PITINTFLAGS = 0x01; // clear flag

  // Fan timeout?
  if (FAN_timer < 15000) // 1 min
    FAN_timer++;
  else  // 1 min
    FAN_off();

  // Relay latch timeout?
  if (rellatch_timer < 25) // ~100 ms
    rellatch_timer++;
  else {
    RELLATCH_OFF();
  }

  ntc_timer++;
  if (25 == ntc_timer) {
    ntc_timer = 0;
    ntc_flag = 0x01;
  }
}

ISR(PORTA_PORT_vect) {
  PORTA.INTFLAGS = 0x20; // clear flag
  cyclecount++;
}

ISR(ADC0_WCOMP_vect) {
  ADC0.INTFLAGS = 0x02; // clear flag
  TCA0.SINGLE.CTRLA = 0b00000000; // DISABLE switching.
}

/* *** main function *** */

void main () {

#ifdef DEBUG
  char buffer[10];
#endif

  enum states state = alloff;
  uint8_t n_fail = 0;
  int cyclecounts[5];
  int cyclecount_mean;
  int loadpoint, load_l, load_h;
  
  PORTA.DIR = REL_L_RX | REL_L_TX | RELAY | BUCK_GATE; // outputs
  PORTA.PIN6CTRL = 0x04; // digital input disable for AIN6/PA6
  PORTA.PIN7CTRL = 0x04; // digital input disable for AINP0/AIN7/PA7
  PORTA.OUT = 0x00;
  PORTA.PIN4CTRL = 0x08; // enable pull-up for JP1
  PORTB.DIR = RX | FAN;
#ifdef DEBUG
  PORTB.DIR |=  TXD;
#endif
  PORTB.PIN2CTRL = 0x08; // enable pull-up for JP2
  PORTB.PIN4CTRL = 0x08; // enable pull-up for JP3
  PORTB.PIN0CTRL = 0x08; // enable pull-up for JP4
  PORTB.PIN3CTRL = 0x08; // enable pull-up for PTT
  PORTB.OUT = 0x00;
  PORTC.DIR = TX_INH | PA;
  PORTC.OUT = 0x00;
  PORTC.PIN2CTRL = 0x08; // enable pull-up for REL_FB_TX
  PORTC.PIN3CTRL = 0x08; // enable pull-up for REL_FB_RX

  PORTMUX.CTRLA = 0x01; // Enable EVOUT0

  _PROTECTED_WRITE(CLKCTRL.MCLKCTRLB,0x03); // Main clock divide by 4

  while(RTC.STATUS);
  RTC.CLKSEL = 0x00; // Use 32768 Hz from OSC32K
  while(RTC.STATUS);
  RTC.CTRLA = 0x01; // enable RTC counter

  RTC_delay(6554); // 200ms

  if (PORTA.IN & JP1)
    jp1_soldered = 0;
  else
    jp1_soldered = 1;

#ifndef DEBUG
  if (PORTB.IN & JP2)
    jp2_soldered = 0;
  else
    jp2_soldered = 1;
#else
  jp2_soldered = 0;
#endif
  
  if (PORTB.IN & JP3)
    jp3_soldered = 0;
  else
    jp3_soldered = 1;

  if (PORTB.IN & JP4)
    jp4_soldered = 0;
  else
    jp4_soldered = 1;

  TCA0.SINGLE.PER = 374; // 75us period -> 13.3 kHz
  TCA0.SINGLE.CTRLA = 0b00000001; // DIV1 -> 0.15us/tick, ENABLE
    
  TCB0.CTRLB = 0x16; // Single shot, output enable
  TCB0.EVCTRL = 0x01; // Enable input capture, start counter at pos edge
  TCB0.CCMP = 60; // 12us for 12V in and 4.4A current in 33uH 
  TCB0.CTRLA = 0x01; // ENABLE
  // P=u^2/(2*L)*T^2*f

  AC0.MUXCTRLA = 0b10000011; // Inverted output, DAC to neg in
  VREF.CTRLA = 0x33; // 4.3V ADC+DAC
  DAC0.DATA = 151; // 151/255*4.3*(100+10)/10 = 28V
  
  DAC0.CTRLA = 0x01; // ENABLE

  AC0.CTRLA = 0b00000001; // ENABLE
  
  EVSYS.SYNCCH0 = 0x02; // TCA0_OVF
  EVSYS.ASYNCCH0 = 0x01; // CCL_LUT0
  //EVSYS.ASYNCUSER8 = 0x01; // SYNCCH0 -> EVOUT0/PA2 to see max switch freq 
  EVSYS.ASYNCUSER2 = 0x01; // SYNCCH0 -> LUT0
  EVSYS.ASYNCUSER0 = 0x03; // ASYNCCH0 -> TCB0
  
  CCL.LUT0CTRLB = 0x63; // AC0 + EVENT0
  CCL.TRUTH0 = 0b10001000; // AND
  CCL.LUT0CTRLA = 0x01; // ENABLE
  CCL.CTRLA = 0x01; // ENABLE

  RTC_delay(3277); // wait 100ms after start before enabling overload detection

  TCA0.SINGLE.PER = 129; // 26us period -> 38.5 kHz
  
  ADC0.CTRLB = 0x00; // No accumulation
  ADC0.CTRLC = 0x03; // Int ref, div 16
  ADC0.CTRLD = 0x00;
  ADC0.CTRLE = 0x01; // window comp below
  ADC0.MUXPOS = 0x07; // AIN7
  ADC0.INTCTRL = 0x02; // int window comp enable
  ADC0.WINLT = 563; // 563 = 26V  628=29V
  ADC0.CTRLA = 0x03; // Enable, free-run, every 30us
  ADC0.COMMAND = 0x01; // Start conv
  
  loadpoint = eeprom_read_word(LOADPOINT_EEADDR);
  load_l = loadpoint - loadpoint/5;
  load_h = loadpoint + loadpoint/5;

  while(RTC.PITSTATUS);
  RTC.PITINTCTRL = 0x01; // enable interrupt
  while(RTC.PITSTATUS);
  RTC.PITCTRLA = 0b00110001; // Enable, interrupt after 128 cycles => 256 Hz

  RELLATCH_RX();
  RX_on();

  state = rx;

#ifdef DEBUG
  // UART for debug

  // Baud rate compensated with factory stored frequency error
  int8_t sigrow_val = SIGROW.OSC20ERR5V;
  int32_t baud_reg_val = (int32_t)2084;  // 9600 baud at 5MHz

  baud_reg_val *= (1024 + sigrow_val);
  baud_reg_val /= 1024;
  USART0.BAUD = (int16_t) baud_reg_val;

  //USART0.CTRLA = 0b10000000; // RXCIE
  USART0.CTRLB = 0b01000000; // TX enable
#endif
    
  sei();

#ifdef DEBUG
  USART_Transmit_String("Start\n");
#endif
  
  while(1) {

    switch (state) {
    case rx:
      if (PTT_ACTIVE()) { // then this is a transition	
	RX_off();
	REL_on();
	RELLATCH_TX();
	state = relay_activate;
      }
      break;
      
    case relay_activate:
      /*if (RTC_delay_PTT(328, 0)) { // 10ms // (RTC_delay_PTT(2785, 0)) { // 85ms
	REL_off();
	RELLATCH_RX();
	state = relay_deactivate;
	break;
	}*/
      cyclecount = 0;
      PORTA.PIN5CTRL = 0x02; // Interrupt at rising edge on gate drive pin
      if (RTC_delay_PTT(492, 0)) { // 15ms
	PORTA.PIN5CTRL = 0x00; // disable interrupt
	REL_off();
	RELLATCH_RX();
	state = relay_deactivate;
	break;
      }	
      PORTA.PIN5CTRL = 0x00; // disable interrupt
#ifdef DEBUG
      inttostr(buffer, cyclecount);
      USART_Transmit_String(buffer);
#endif
      if((PORTC.IN & REL_FB_TX) || !(PORTC.IN & REL_FB_RX)) {   // correct feedback is low-level
	state = relay_fail;
	break;
      }
      else if (jp3_soldered && !jp4_soldered) { // basic relay load monitoring
#ifdef DEBUG
	USART_Transmit_String("basic\n");
#endif
	if (cyclecount > 30 && cyclecount < 490) { // 5-85% load
	  PA_on();
	  state = pa;
	  n_fail = 0;
	  break;
	}
	else {
	  state = relay_fail;
	  break;
	}
      }
      else if (!jp3_soldered && jp4_soldered) { // precise relay load monitoring
#ifdef DEBUG
	USART_Transmit_String("precise\n");
#endif
	if (cyclecount > load_l && cyclecount < load_h) {
	  PA_on();
	  state = pa;
	  n_fail = 0;
	  break;
	}
	else {
	  cyclecounts[n_fail]=cyclecount;
	  n_fail++;
	  if (n_fail == 5) {
	    cyclecount_mean = (cyclecounts[0] +
			       cyclecounts[1] +
			       cyclecounts[2] +
			       cyclecounts[3] +
			       cyclecounts[4])/5;
	    for (uint8_t n=0; n<5; n++) {
	      if (cyclecounts[n] < 10 || cyclecounts[n] > 490 || cyclecounts[n] < (cyclecount_mean - 40) || cyclecounts[n] > (cyclecount_mean + 40))
		n_fail = 10; // flag
	    }
	    if (n_fail == 5) {  // all at max 85% load and close to eachother
	      loadpoint = cyclecount_mean;
	      load_l = loadpoint - loadpoint/5;
	      load_h = loadpoint + loadpoint/5;
	      eeprom_write_word(LOADPOINT_EEADDR, loadpoint);
#ifdef DEBUG
	      USART_Transmit_String("\nM: ");
	      inttostr(buffer, cyclecount_mean);
	      USART_Transmit_String(buffer);
	      USART_Transmit_String("\nL: ");
	      inttostr(buffer, load_l);
	      USART_Transmit_String(buffer);
	      USART_Transmit_String("\nH: ");
	      inttostr(buffer, load_h);
	      USART_Transmit_String(buffer);
#endif

	    }
	    n_fail = 0;
	  }
	  state = relay_fail;
	  break;
	}
      }
      else {
	PA_on();
	state = pa;
      }	  
      break;
      
    case relay_fail:
      if (!PTT_ACTIVE()) {
	REL_off();
	RELLATCH_RX();
	RTC_delay(328); // 10ms
	RX_on();
	state = rx;
      }
      break;
      
    case pa:
      if (RTC_delay_PTT(328, 0)) { // 10ms
	PA_off();
	if (RTC_delay_PTT(328, 1)) { // 10ms
	  PA_on();
	  state = pa;
	  break;
	}
	REL_off();
	RELLATCH_RX();
	state = relay_deactivate;
	break;
      }
      TX_on();
      state = tx;
      break;
      
    case tx:
      FAN_run();
      if (!PTT_ACTIVE()) {
	TX_off();
	if (RTC_delay_PTT(328, 1)) { // 10ms
	  TX_on();
	  state = tx;
	  break;
	}
	state = pa;
      }
      break;
      
    case relay_deactivate:
      if (RTC_delay_PTT(328, 1)) { // 10ms
	REL_on();
	RELLATCH_TX();
	state = relay_activate;
	break;
      }
      RX_on();
      state = rx;
      break;

    case stopped:
      TX_off();
      RX_off();
      RTC_delay(33); // 1ms
      PA_off();
      break; // Stay stopped until power cycle
    }
      
    if (ntc_flag) {  // up to 200us blind time every 100ms
      ADC0.INTCTRL = 0x00; // int window comp disable
      ADC0.CTRLA = 0x01; // Enable, free-run disable
      while(ADC0.COMMAND); // wait to complete
      ADC0.MUXPOS = 0x06; // AIN6
      RTC_delay(2); // 30-60us
      ADC0.COMMAND = 0x01; // Start conv
      while(ADC0.COMMAND); // wait to complete
      if (ADC0.RES < 319)  // < 1.34V
	state = stopped;
      ADC0.MUXPOS = 0x07; // AIN7
      RTC_delay(2); // 30-60us
      ADC0.INTCTRL = 0x02; // int window comp enable
      ADC0.CTRLA = 0x03; // Enable, free-run, every 30us
      ADC0.COMMAND = 0x01; // Start conv
      ntc_flag = 0x00;
    }
  }
}
