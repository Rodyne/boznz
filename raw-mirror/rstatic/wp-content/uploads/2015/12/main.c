/*
2014 Roving Dynamics Ltd.

Title: RFID Combiner and Printer Control board

Rev: 1.0 (Dec 2015)

Target MPU =  PIC18F2xK22: ()

CLK =  11.0952Mhz (Use 4xPLL to internally click this up to 44Mhz for better timing)

 * Description
 * -----------
 * Our HMI PC is located 25M from 2 x RFID readers and a printer, RS232 can only do <10M reliably so we need 
 * to use RS-485. Because we need 4 serial ports and the PIC only has 2 we run 2 soft UART ports to 
 * read the RFID data via an interrupt. The RFID data is only a 10 char string but we also need to 
 * know which one was activated so before we send the string back to the PC we prefix it with an
 * A or a B (or whatever) data sent from the PC will go directly to the printer port as RS232 however
 * if we send a prefixed Command string we can optionally make the system switch on/off 4 relays, sound
 * a buzzer or resend the last RFID strings

 */

#include <xc.h>

#define uint8   unsigned char
#define uint16	unsigned short

#pragma config FOSC	  = HSHP		// Ext XTAL Running at 11.0592Mhz (reason: common xtal and gives exact 9600 baud rate)
#pragma config PLLCFG = ON      // PLL x 4 = 44.23Mhz (required for finer timing in interrupt)
#pragma config CCP3MX = PORTB5	// check if req
#pragma config PBADEN = OFF			// ADC OFF (Port B is digital)
#pragma config WDTPS  = 8192		// Watchdog timer reset timeout = 8 seconds
#pragma config WDTEN  = ON			// Watchdog timer on, will reset chip if I dont reset it within timeout
#pragma config STVREN = ON			// Stack overflow reset ON (Any stack probs then do a reset. Better safe than sorry!)
#pragma config BOREN  = ON			// Turn on Brown-out reset so the device resets on any power problem

// Pin I/O's (changes here will also require change to TRIS in program startup duhh!)

#define LED_PC(state)     LATCbits.LATC0 = !state // Output (LED Lights when low - hence inverted))
#define LED_PTR(state)    LATAbits.LATA4 = !state // Output (LED Lights when low - hence inverted)
#define LED_RFID1(state)  LATCbits.LATC2 = !state // Output (LED Lights when low - hence inverted)
#define LED_RFID2(state)  LATCbits.LATC1 = !state // Output (LED Lights when low - hence inverted)
#define OC1(state)        LATAbits.LATA0 = !state  // Output - OC1 relay1 gate
#define OC2(state)        LATAbits.LATA1 = !state  // Output - OC2 relay2 gate
#define OC3(state)        LATAbits.LATA2 = !state  // Output - OC3 relay3 gate
#define OC4(state)        LATAbits.LATA3 = !state  // Output - OC4 relay4 gate
#define TX_CTRL(state)    LATCbits.LATC4 = state  // Output - RD485 - 1 = transmit / 0=receive
#define BUZZER(state)     LATBbits.LATB4 = state  // Output - to buzzer
#define RFID1SER          !PORTBbits.RB1           // Input  - soft UART serial input from RFID 1
#define RFID2SER          !PORTBbits.RB2           // Input  - soft UART serial input from RFID 2

// use volatile prefix in interrupt variables and/or where you dont want compiler optimising anything.

// 9600 bit timings (in units of 7.787uS - value determined by cpu clock and timer 1/3 prescaller) 
// for soft uart, ie mid-point for sampling bit 1 comes 154us after start bit (this is approximately 20 x 7.787)
const uint8     Baud9600[] = {21,38,57,75,93,112,129,147,162};  // we dont time stop bit

volatile uint8  PCCommBuf[32];    // serial buffer from UART1
volatile uint8  PTRCommBuf[32];   // serial buffer from UART2
volatile uint8  RFID1CommBuf[16]; // serial buffer from soft UART connected to RFID reader1
volatile uint8  RFID2CommBuf[16]; // serial buffer from soft UART connected to RFID reader2
volatile uint8  PCBufPtr = 0;     // 
volatile uint8  PTRBufPtr = 0;    // 
volatile uint8  RFID1BufPtr = 0;  // 
volatile uint8  RFID2BufPtr = 0;  // 
volatile uint8  SampleBit1 = 0;
volatile uint8  SampleBit2 = 0;
volatile uint8  t40 = 0;
volatile uint16 beeper = 0;  // 
volatile uint16 tone;  // 

enum tHM { ReceiveDataFromPC, SendPrinterDataToPC, SendRFID1DataToPC, SendRFID2DataToPC } HostMode; // state of the main loop


// ALL interrupts come here! Keep interrupt code very tight n super fast as only 23uS to process!
// so NO loops and NO external proc/function calls!
void interrupt high_priority isr(void) 
{
  OC1(1);
  if (INTCON3bits.INT1IF && INTCON3bits.INT1E) // External Interrupt 1: comm detected RFID1. DONT acknowledge flag until stop bit received
  {
    LED_RFID1(1);
    INTCON3bits.INT1E = 0;  // disable external int1 until finished processing char and we are ready for next char from RFID1
    SampleBit1=0; // going to read in 10 bits, 1 start, 8 data and 1 stop each bit read will inc this byte
    TMR0L=32; // sync tmr0 interrupt so we dont get one immediately after
    INTCONbits.TMR0IF = 0; // stop it interrupting if already has
    TMR1H=0;
    TMR1L=0; // start timer3 to time the next char in 
  }
  if (INTCON3bits.INT2IF && INTCON3bits.INT2IE) // External Interrupt 2: comm detected RFID2. DONT acknowledge flag until stop bit received but disable interrupt so stop recurring
  {
    LED_RFID2(1);
    INTCON3bits.INT2IE = 0;  // disable external int2 until we have finished processing char and we are ready for next char from RFID2
    SampleBit2=0; // going to read in 10 bits, 1 start, 8 data and 1 stop each bit read will inc this byte
    TMR0L=32; // sync tmr0 interrupt 
    INTCONbits.TMR0IF = 0; // stop it interrupting if already has
    TMR3H=0;      
    TMR3L=0; // start timer1 to time the next char in
  }
  if (PIR1bits.RC1IF) // interrupt is caused by UART 1 RX pin (data received from PC)
  {
    LED_PC(1);
    PCCommBuf[PCBufPtr] = RCREG1; //read the byte from rx register into receive buffer
    if (PCBufPtr>31) PCBufPtr=0; // inc buffer: if we ever overrun then we wrap around (circular buffer)
    PIR1bits.RC1IF = 0; // acknowledge/clear rx int flag
    LED_PC(1);
  }
  if (PIR3bits.RC2IF) // interrupt is caused by UART 2 RX pin (data received from printer)
  {
    LED_PTR(1);
    PTRCommBuf[PTRBufPtr] = RCREG2; //read the byte from rx register into receive buffer
    if (PTRBufPtr>31) PTRBufPtr=0; // inc buffer: if we ever overrun then we wrap around (circular buffer)
    PIR3bits.RC2IF = 0; // acknowledge/clear rx int flag
    LED_PTR(1);
  }
  if (INTCONbits.TMR0IF) // timer 0 Overflow occurs all the time every 23uS (exact not too important so long as there are about 4 per bit at 9600 baud)
  {
    if(INTCON3bits.INT1IF) // RFID1 soft uart
    {
      OC2(1);
      if(TMR1H>Baud9600[SampleBit1]) //  wait for next sample window in middle of bit
      {  
        OC2(0);
        if(RFID1SER && SampleBit1<8) RFID1CommBuf[RFID1BufPtr] = RFID1CommBuf[RFID1BufPtr] | (1<<SampleBit1);  // build up 8 bit char
        if(SampleBit1++>=8) // received stop byte (8 bits) processed
        {
          if(RFID1BufPtr<13)
          {
            RFID1BufPtr++; // next char
            RFID1CommBuf[RFID1BufPtr]=0x0;      // clr it
            INTCON3bits.INT1IE=1;             // re-enable interrupt and..
            INTCON3bits.INT1IF=0;             // clr interrupt flag to wait for next interrupt
          }  
        }  
      }
    }  
    if(INTCON3bits.INT2IF) // RFID2
    {
      if(TMR3H>Baud9600[SampleBit2]) //  wait for next sample window
      { 
        if(RFID2SER && SampleBit2<8) RFID2CommBuf[RFID2BufPtr] = RFID2CommBuf[RFID2BufPtr] | (1<<SampleBit2);  // 
        if(SampleBit2++>=8) // received stop byte (8 bits) processed
        {
          if(RFID2BufPtr<13) 
          {  
            RFID2BufPtr++; // next char
            RFID2CommBuf[RFID2BufPtr]=0;      // clr it
            INTCON3bits.INT2IE=1;             // re-enable interrupt and..
            INTCON3bits.INT2IF=0;             // clr interrupt flag to wait for next interrupt
        }  
        }  
      }  
    }  
    if(beeper)
    {  
      if(--t40==0)
      {
        t40=40;
        tone=!tone; // interrupt calling @ 43Khz so too high to just modulate. need to divide to lower the freq of buzzer to around 1Khz
        beeper--;
      }  
      BUZZER(beeper && tone);
    }  
    INTCONbits.TMR0IF = 0; // acknowledge/clr timer0 interrupt
  }
  OC1(0);
}

void delay(uint16 ms) // blocking delay of approx 1000uS (1mS) doesnt need to be accurate, not used for timing (interrupt is used for that)
{
  static uint16 i;
  while (ms--)
  {
    i = 1600; // value tweeked for osc freq of 11.05mhz with 4xPLL = 44Mhz
    while (i--);
  }
}

void Send2PC(uint8 data) // Send byte to pc
{
  TX_CTRL(1);
  while (!PIR1bits.TX1IF); // wait for tx transmit reg to become free
  TXREG = data; // send char
  while (!TXSTA1bits.TRMT); // wait for character to be physically clocked out so we dont overrun transmit buffer    
}

void Send2Printer(uint8 data) // Send byte to printer
{
  while (!PIR3bits.TX2IF); // wait for tx transmit reg to become free
  TXREG = data; // send char
  while (!TXSTA2bits.TRMT); // wait for character to be physically clocked out so we dont overrun transmit buffer    
}

uint8 DecodeCMD() // Called from main on serial receive from PC. determine if command
{
  if (PCCommBuf[0] != 'X' ) return 0;     // CMD Wake up Sequence is "XW." followed by CMD_Byte
  if (PCCommBuf[1] != 'W' ) return 0; 
  if (PCCommBuf[2] != '.' ) return 0;     // eg "XW.A" in the data from the PC will turn on relay 1
  // process cmds
  if(PCCommBuf[3] == 'A' ) OC1(1);          // turn ON relay 1
  if(PCCommBuf[3] == 'B' ) OC1(0);          // turn OFF relay 1
  if(PCCommBuf[3] == 'C' ) OC2(1);          // turn ON relay 2
  if(PCCommBuf[3] == 'D' ) OC2(0);          // turn OFF relay 2
  if(PCCommBuf[3] == 'E' ) OC3(1);          // turn ON relay 3
  if(PCCommBuf[3] == 'F' ) OC3(0);          // turn OFF relay 3
  if(PCCommBuf[3] == 'G' ) OC4(1);          // turn ON relay 4
  if(PCCommBuf[3] == 'H' ) OC4(0);          // turn OFF relay 4
  if(PCCommBuf[3] == 'I' ) beeper=1000;     // sound alarm
  if(PCCommBuf[3] == '1' ) RFID1BufPtr=10;  // re-send RFID1 code (if one exists!)
  if(PCCommBuf[3] == '2' ) RFID2BufPtr=10;  // re-send RFID2 code (if one exists!)
  Send2PC('O');                             // Send OK string to PC/Host
  Send2PC('K');
  Send2PC('\r');
  Send2PC('\n');
  PCBufPtr=0;     // empty buffer as we dont want to pass this string through to printer
  return 1;
}

int main()
{
  // Init PIC18F2xK22 Registers, peripherals and I/O Ports (See Microchip datasheet for full descriptions)
  uint8 i;

  ADCON0 = 0;           // Turn off ADC
  ADCON1 = 0x0F;        // turn off ADC
  ANSELC = 0;           // Turn off ADC
  TRISA  = 0x00;        // PortA: (00000000) [All Output]
  TRISB  = 0x86;        // PortB: (10000110) RB1, RB2, RB7 Inputs, RB0, RB3, RB4, RB5, RB6 Outputs (RB7 is UART RX and must be input))
  TRISC  = 0x80;        // PortC: (10000000) RC7 input, RC0..RC6 output (RC7 is UART RX and must be input)

  // All outputs to default on POR
  
  OC1(0);               // relay 1 Off
  OC2(0);               // relay 2 Off
  OC3(0);               // relay 3 Off
  OC4(0);               // relay 4 Off
  BUZZER(0);            // Buzzer off
  TX_CTRL(0);           // RS485 tranceiver in receive mode
  LED_PC(1);            // LED ON
  LED_PTR(1);           // LED ON
  LED_RFID1(1);         // LED ON
  LED_RFID2(1);         // LED ON

  // now flash leds off in sequence to indicate power up (gives us indication if resetting for any other reasons too)
  delay(400);
  LED_PC(0);
  delay(200);
  LED_RFID2(0);
  delay(200);
  LED_RFID1(0);
  delay(200);
  LED_PTR(0);  

  // init state machine
  HostMode = ReceiveDataFromPC;
  RFID1BufPtr=0;
  RFID2BufPtr=0;
  PCBufPtr=0;
  PTRBufPtr=0;
  
  // interrupts
  RCONbits.IPEN = 0;    // DISABLE PRIORITY INTERRUPTS all use same single ISR (IPEN=0 is actually the default so this line has no effect and left in for comment)
  INTCON  = 0x60;       // (0110 0000) Turn on TMR0 interrupt TURN OFF GLOBAL INTERRUPTS until init finished
  INTCON2 = 0xF4;       // (1111 0100) PortB pullups disabled, INT1 + INT2 rising edge, TIMER0 High priority
  INTCON3 = 0xD8;       // (1101 1000) clr INT1 and INT2 flags dont enable interrupts until init finished tho

  // timers
  T0CON = 0xc8;         // timer0: (1100 1000) 8 bit overflow interrupt occurs every (fOsc/4/prescaler/256)  == 11059200 x 4 / 4 / 1 / 256 = 23uS  
  T1CON = 0x45;         // timer1: (0100 0101) 16 bit timer fOsc/Prescaler. high timer reg incs every 5.787uS which times our soft uart
  T3CON = 0x45;         // timer3: (0100 0101) same as tmr1
  
  // UART 1 (RS485/PC)
  SPBRGH1 = 0;          // BRG to 9600 baud
  SPBRG1 = 68;          // 68 = 17x4 as we are using 4xPLL on main XTAL
  TXSTA1bits.BRGH1 = 0; // BRGH=0
  TXSTA1bits.TXEN1 = 1; // TX ENABLED
  TXSTA1bits.SYNC1 = 0; // Async
  RCSTA1bits.SPEN1 = 1; // Serial port ENABLE (Interrupts enabled in ResetComms() )
  PIE1bits.RC1IE = 1;   // enable interrupt UART1

  // UART 2 (Printer)
  SPBRGH2 = 0;          // BRG to 9600 baud
  SPBRG2 = 68;          // 68 = 17x4 as we are using 4xPLL on main XTAL
  TXSTA2bits.BRGH2 = 0; // BRGH=0
  TXSTA2bits.TXEN2 = 1; // TX ENABLED
  TXSTA2bits.SYNC2 = 0; // Async
  RCSTA2bits.SPEN2 = 1; // Serial port ENABLE (Interrupts enabled in ResetComms() )
  PIE3bits.RC2IE = 1;   // enable interrupt UART2

  // turn on all interrupts
  INTCONbits.GIE = 1;     // enable global interrupts

  while (1) // main program loop
  {
    if (HostMode==ReceiveDataFromPC) // default is nothing being sent we are in receive mode waiting for input from PC Host
    {
      TX_CTRL(0); // ensure rs485 tranceiver is in receive mode
      delay(20);
      LED_RFID1(0);
      LED_RFID2(0);
      LED_PTR(0);
      LED_PC(0);
      if(PCBufPtr>3) // If chars in PC buffer (UART1), check for command or pass data through to Printer (UART2)
      {  
        PIE1bits.RCIE = 0; // turn OFF UART interrupts to prevent comm buffer changing while we are processing request
        if(!DecodeCMD()) Send2Printer(PCCommBuf[PCBufPtr--]); // pass through whats been received from PC to printer (UART1 rx to UART2 tx)
        PIE1bits.RCIE = 1; // turn ON UART interrupts          
      }
      else
      if (PTRBufPtr>4) HostMode=SendPrinterDataToPC; // data coming in from printer rs232 port, so send it
      else
      if (RFID1BufPtr > 10) // received a RFID tag so prefix it and send it
      {
        HostMode=SendRFID1DataToPC;
        RFID1CommBuf[0]='A';
        RFID1BufPtr--;
        RFID1CommBuf[RFID1BufPtr++]='\r';
        RFID1CommBuf[RFID1BufPtr]='\n';
      }
      else
      if (RFID2BufPtr > 10) // received a RFID2 tag so prefix it and send it
      {
        HostMode=SendRFID2DataToPC;
        RFID2CommBuf[0]='B';
        RFID2BufPtr--;
        RFID2CommBuf[RFID2BufPtr++]='\r';
        RFID2CommBuf[RFID2BufPtr++]='\n';
      }  
    }

    if ( HostMode==SendPrinterDataToPC ) // (shouldnt happen often, but) we are receiving data from Printer UART2 so send it to the PC on UART1
    {      
      Send2PC(PCCommBuf[PCBufPtr--]); // send char
      if(PCBufPtr==0) HostMode=ReceiveDataFromPC; // give up control once sent
    }

    if ( HostMode==SendRFID1DataToPC ) // we have received an RFID string so pass it to host PC
    {      
      for(i=0;i<=RFID1BufPtr; i++)
        Send2PC(RFID1CommBuf[i]);
      INTCON3bits.INT1IE=1;             // re-enable interrupt and..
      INTCON3bits.INT1IF=0;             // clr interrupt flag to wait for next interrupt
      RFID1BufPtr=0;
      HostMode=ReceiveDataFromPC; // give up control once RFID1 tag sent
    }

    if ( HostMode==SendRFID2DataToPC ) // we have received an RFID string so pass it to host PC
    {      
      for(i=0;i<=RFID2BufPtr; i++)
        Send2PC(RFID2CommBuf[i]);
      INTCON3bits.INT2IE=1;             // re-enable interrupt and..
      INTCON3bits.INT2IF=0;             // clr interrupt flag to wait for next interrupt
      RFID2BufPtr=0;
      HostMode=ReceiveDataFromPC; // give up control once RFID2 tag sent
    }
    ClrWdt(); // run this to stop MPU WDT resetting (Fail safe in case we lock up somewhere above due to crap code!)
  }
}