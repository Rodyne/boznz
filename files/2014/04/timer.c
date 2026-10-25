// 2014 Roving Dynamics Ltd. Code is free to use as required and for any purpose

#include <xc.h>

#pragma config WDT = OFF, OSC = IntRC_RB4EN, MCLRE = OFF, CP = OFF // Int 4Mhz Clock, disable MCLR, watchdog timer and Code protection.

#define SELENOID_ON RB5

const unsigned char Seg7b[] = {0x01,0x07,0x02,0x02,0x04,0x00,0x00,0x03,0x00,0x00}; // 7 segment lookup table for port b rb0=d, rb1=b, rb2=e
const unsigned char Seg7c[] = {0x00,0x06,0x01,0x04,0x06,0x24,0x20,0x06,0x00,0x04}; // 7 segment lookup table for port c

unsigned char ticks,count,seconds;

int main()
{
	// Init PIC16F505 (See datasheet)
  TRISB = 0x20;  // PortB: [All Output] except RB5 (Selenoid trigger input from rectifier)
	TRISC = 0x00;  // PortC: [All output]
	OPTION = 0xC3; // Prescaler to TMR0 (4mhz/4/16 = TMR0 count time). Modify TMR0 preset / C1 rollover values for strobe and second counter

	while(1) // DO Forever
	{
		count=0;
		ticks=0;
		seconds=0;

		while(count-seconds<20 && (count>1 || SELENOID_ON)) // while selenoid is on these are in sync, once selenoid off the counter keeps counting and we eventually timeout
		{  	
			// delay here for 5mS (tweek so about right to strobe LED digits without user noticing flicker)
			TMR0=1;
			while(TMR0);

			if(SELENOID_ON) seconds = count; // while the selonoid is active increment the counter, once selonoid innactive stop and hold seconds value

			if(count>0) // display
			{
				// display off (need this to prevent display ghosting as we multiplex) then alternately write each digit
				PORTB=0x07;
				PORTC=0x3f;
				if(seconds<35 || ticks>64) // logic here flashes digits if over 35 seconds
				{
					if(ticks%2) // display digit 1 (units) on even tick
					{
						PORTC = Seg7c[seconds%10] | 0x08; // or digit 1 anode
						PORTB = Seg7b[seconds%10];
					}
					else // display digit 2 (tens) on odd tick
					{
						PORTC = Seg7c[seconds/10] | 0x10; // or digit 2 anode
						PORTB = Seg7b[seconds/10];
					}
				}
			}

			if(ticks++ == 189) // tweek this to increment count once every second (obviously!)
			{
				ticks=0;
				count++;
			}
		}

		// display off then wait here until selonoid active so we can repeat all over again
		PORTB=0x07;
		PORTC=0x3f;
		while(!SELENOID_ON);
	}
}