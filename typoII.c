#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>
#include "map.c"

#define F_CPU 1000000UL
#define FOSC 1000000
#define baud 4800
#define myubrr FOSC/16/baud-1
#define HITSTOP 20
#define UART_BUFFER_LEN 240
#define OUTPUT_BUFFER_LEN 10


/*
So pd = 11.5 ms -> 87Hz
time low = .820 ms
time high = 10.68 ms

delay time (referenced to 1 = 0)
* 1 = 0.00 ms		slot 0
* 2 = 9.010 ms		slot 11
* 3 = 6.55 ms		slot 8
* 4 = 5.73 ms		slot 7
* 5 = 4.910 ms		slot 6
* 6 = 4.099 ms		slot 5
* 7 = 3.28 ms 		slot 4
* 8 = 2.46 ms		slot 3
* 9 = 1.64 ms		slot 2
* 10 = 0.820 ms		slot 1
* 11 = 9.84 ms		slot 12
* 12 = 8.19 ms		slot 10
* 13 = 7.37 ms		slot 9
* 14 = 10.675 ms	slot 13
* 

 */

void usartInit( unsigned int ubrr); 	//initialize USART
void typeChar(const char ch); 			//type a charcter ch
void type(const char* buffer); 			//type a whole buffer!
void text(char in);						//"hello"
void talkback(void);

volatile unsigned char pinput = 0;
volatile unsigned int winput = 0;
volatile unsigned char slot=0;  		//current timeslot (out of 14)
volatile unsigned char hit = HITSTOP; 		//hit (type) counter
volatile unsigned char wave = 1;  		//waveform number
volatile unsigned char pin = 0;  		//output pin index
//volatile unsigned char ind=0; 			//buffer index
volatile char line[] = "hello, my name is typo";  //input line
volatile char uart_buffer[UART_BUFFER_LEN];
volatile char output_buffer[OUTPUT_BUFFER_LEN];
volatile unsigned int uart_buffer_towrite = 0;
volatile unsigned int uart_buffer_index = 0;
volatile unsigned char output_buffer_towrite = 0;
volatile unsigned char output_buffer_index = 0;
unsigned char lastChar = 0x00;
volatile unsigned char letter = 0;		//
unsigned char wavetable[15]= 			//assigns the output pin to the 
{0, 0, 11, 8, 7, 6, 5, 4, 3, 2, 1, 12, 10, 9, 13}; //correct timeslot
unsigned char invwave[15]=
{0, 10, 9, 8, 7, 6, 5, 4, 3, 13, 12, 2, 11, 14};


ISR(INT0_vect)			//external interrupt 0 (falling edge)
{
		TCNT1 = 0;		//reset timer
		TCNT0 = 0;
		slot = 0;		//back in timeslot 0
		OCR1A = 700;	//set compare register for 700 to compensate
}						//for interrupt time.

ISR(TIMER0_COMPA_vect)
{
	unsigned char temp = DDRB;
	unsigned char ptemp = PORTB;
	DDRB = 0x00;
	PORTB = 0x00;
	pinput = PINB;
	winput = invwave[slot];
	DDRB = temp;
	PORTB = ptemp;
}

ISR(TIMER1_COMPA_vect)	//timer1 compare interrupt
{						//end of each time slot
	TCNT1 = 0;			//reset timer
	TCNT0 = 0;
	slot++;				//advance which timeslot we're in
	if (slot > 14)		//if we've been counting too fast
	{
		slot = 15;		//just go back to timeslot 0
	}
	
}

ISR(TIMER1_COMPB_vect)	//timer2 compare interrupt
{						//the beginning of each time slot
	OCR1A = 820;		//set output compare for .8ms
	if(slot == 0)
	{
		OCR1A = 720;	
	}
	if ((hit < HITSTOP) && (slot == wave))	//strike a key in timeslot wave
	{
		DDRB |= (1<<(pin-1));		//enable pin[pin] as output
		if(hit < (HITSTOP - 4))
		{
			PORTB &= ~(1<<(pin-1));		//pull pin[pin] low
		}
		else
		{
			PORTB |= (1<<(pin-1));
		}
		if (hit == 1)				//reset OCR1A to compensate for 
		{							//"initial hit" (filters out a key
			OCR1A = 3270;			//press from a key hold)
		}
		hit++;						//increment hit so we dont' do this
	}								//all day
	else
	{
		PORTB |= (1<<(pin-1));		//pull pin high
		DDRB &= ~(1<<(pin-1));		//enable as input
		PORTB &= ~(1<<(pin-1));		//high z.
	}
	if(hit >= HITSTOP && uart_buffer_towrite > 0){
		typeChar(uart_buffer[(uart_buffer_index - uart_buffer_towrite + UART_BUFFER_LEN) % UART_BUFFER_LEN]);
		uart_buffer_towrite--;
	}
}


ISR(USART_RX_vect)
{
	unsigned char data = UDR0;
	/*if(data == 0)
	{
		letter = 0;
	}
	else
	{
		line[ind]=data;
		ind++;
	}*/
	//while (!( UCSR0A & (1<<UDRE0)));
	/* Put data into buffer, sends the data */
	//UDR0 = data;
	//typeChar(data);
	if(uart_buffer_towrite < UART_BUFFER_LEN){
		uart_buffer[uart_buffer_index] = data;
		uart_buffer_towrite++;
		uart_buffer_index++;
		/*
		if(uart_buffer_towrite == 1 && hit >= HITSTOP){
			typeChar(data);
			uart_buffer_towrite--;
		}*/
		if(uart_buffer_index >= UART_BUFFER_LEN){
			uart_buffer_index = 0;
		}
	}
}

int main(void)
{	
	EICRA = (1<<ISC01); //set interrupt for falling edge
	EIMSK = (1<<INT0);  //enable int0 interrupts
	TCCR1A = 0x00;  //no pinchange
	TCCR1B = (1<<CS10) + (1<<WGM12); //1x prescaler + CTC output compare mode
	TIMSK1 = (1<<OCIE1A) + (1<<OCIE1B); //enable output compare interrupt A
	TCCR0A = 0x00;
	TCCR0B = (1<<CS01);
	TIMSK0 = (1<<OCIE0A);
	OCR0A = 50;
	OCR1A = 0xFFFE; 	//set output compare value for a billion
	OCR1B = 0x0008;		//set outcompB value to eight
	DDRB = 0x00;		//disable all outputs
	PORTB = 0x00;		//but pull all of 'em high anyway!
	DDRC = 0x00;		//port c is for inputs
	PORTC = 0x00;		//also pulled high internally
	DDRD = 0x00;		//port d is for int0 (input)
	PORTD = 0x00;
	usartInit(myubrr);	//initialize USART w/ baud register value ubrr
	TCNT1 = 0; 			//init clock @ 0
	sei();				//enable interrupts
	
	//memset(uart_buffer, 'q', UART_BUFFER_LEN);
	//uart_buffer[UART_BUFFER_LEN-1] = 0x00;
	
	while(1)
	{
		//type(line);
		//typeBuffer(uart_buffer, uart_buffer_towrite);

		talkback();
		if(output_buffer_towrite){
			while(!( UCSR0A & (1<<UDRE0)));
			UDR0 = output_buffer[(output_buffer_index - output_buffer_towrite + OUTPUT_BUFFER_LEN) % OUTPUT_BUFFER_LEN];
			output_buffer_towrite--;
		}
	}
}

void talkback(void)
{
	//cli();
	
	static unsigned int pressCount = 0;
	unsigned char in = 0;
	unsigned char done = 0;
	if(pinput == 0xff)
	{
		return;
	}
	while(pinput & 0x01){
		pinput>>=1;
		in++;
	}
	in |= (winput<<4);
	for (done=127;done > 7; done--)
	{
		if (lookup[done] == in) break;
	}
	if((lastChar == done))
	{
		pressCount++;
		if(pressCount == 1)
		{
			output_buffer[output_buffer_index] = lastChar;
			output_buffer_index++;
			if(output_buffer_index >= OUTPUT_BUFFER_LEN)
				output_buffer_index = 0;
			//output_buffer_index = output_buffer_index == OUTPUT_BUFFER_LEN ? 0 : output_buffer_index;
			output_buffer_towrite++;
		}
	}
	else
	{
		pressCount = 0;
	}
	lastChar = done;
	pinput = 0xFF;
	//sei();
}

void type(const char* buffer)
{
	unsigned char x = 0;
	char ch = buffer[x];
	while(ch != 0)
	{
		typeChar(ch);
		_delay_ms(100);
		x++;
		ch = buffer[x];
	}
	typeChar('\r');
	_delay_ms(500);
}

void typeChar(const char ch)
{
	unsigned char letter = lookup[(int)ch];
	unsigned char sig = ((letter&0xf0)>>4);
	//if((letter&0x08)>>3) //shifted
	//{
	//	
	//}
	//while (!( UCSR0A & (1<<UDRE0)));
	//UDR0 = ch;
	wave = wavetable[sig];
	pin = (letter&0x07)+1;
	hit = 1;
}

void usartInit(unsigned int ubrr)
{
	UBRR0H = (unsigned char)(ubrr>>8);	// Set baud rate
	UBRR0L = (unsigned char)ubrr;
	UCSR0B = (1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0);	//Enable receiver and transmitter  
	UCSR0C = (0<<USBS0)|(3<<UCSZ00);	// Set frame format: 8data, 1stop bit
}
