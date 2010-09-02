#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>
#include "map.c"

#define F_CPU 1000000UL
#define FOSC 1000000
#define baud 4800
#define myubrr FOSC/16/baud-1
#define HITSTOP 10
#define UART_BUFFER_LEN 240
#define OUTPUT_BUFFER_LEN 10

void usartInit( unsigned int ubrr); 	//initialize USART
void typeChar(const char ch);

volatile unsigned char nexthit = 1;
volatile unsigned char pinput = 0;
volatile unsigned int winput = 0;
volatile unsigned char slot=0;  		//current timeslot (out of 14)
volatile unsigned char hit = HITSTOP; 		//hit (type) counter
volatile unsigned char wave = 1;  		//waveform number
volatile unsigned char pin = 0;  		//output pin index

//----------------------buffer stuff -------------------------//
volatile char uart_buffer[UART_BUFFER_LEN];
volatile char output_buffer[OUTPUT_BUFFER_LEN];
volatile unsigned int uart_buffer_towrite = 0;
volatile unsigned int uart_buffer_index = 0;
volatile unsigned char output_buffer_towrite = 0;
volatile unsigned char output_buffer_index = 0;

//----------------------wavetable stuff-------------------------//
unsigned char wavetable[15]= 			//assigns the output pin to the 
{0, 0, 11, 8, 7, 6, 5, 4, 3, 2, 1, 12, 10, 9, 13}; //correct timeslot
unsigned char invwave[15]=
{0, 10, 9, 8, 7, 6, 5, 4, 3, 13, 12, 2, 11, 14};



ISR(INT0_vect)
{
	TCNT1 = 0;
	TCNT0 = 0;
	slot = 0;
	OCR1A = 700;
}

ISR(TIMER0_COMPA_vect)
{
	unsigned char temp = DDRB;
	unsigned char ptemp = PORTB;
	DDRB = 0x00;
	PORTB = 0xff;
	pinput = PINB;
	winput = invwave[slot];
	DDRB = temp;
	PORTB = ptemp;
}

ISR(TIMER1_COMPB_vect)
{
	OCR1A = 820;
	if(slot == 0)
	{
		OCR1A = 720;	
	}
	unsigned char pinshift = (1<<(pin-1));
	if(slot == wave)
	{
		if(hit < HITSTOP)
		{
			DDRB |= pinshift;
			if(hit < (HITSTOP - 4))
			{
				PORTB &= ~pinshift;		//pull pin[pin] low
			}
			else
			{
				PORTB |= pinshift;
			}
			if(hit == 1)
			{
				OCR1A = 3270;
			}
			hit++;
		}
		else
		{
			DDRB &= ~pinshift;
			PORTB &= ~pinshift;
			nexthit = 1;
		}
	}
	else
	{
		DDRB &= ~pinshift;
		PORTB &= ~pinshift;
	}
	if((nexthit == 1) && (uart_buffer_towrite > 0))
	{
		typeChar(uart_buffer[(uart_buffer_index - uart_buffer_towrite + UART_BUFFER_LEN) % UART_BUFFER_LEN]);
		uart_buffer_towrite--;
	}
}

ISR(TIMER1_COMPA_vect)
{
	TCNT1 = 0;
	TCNT0 = 0;
	slot++;
	if (slot > 14)		//if we've been counting too fast
	{
		slot = 0;		//just go back to timeslot 0
	}
}

ISR(USART_RX_vect)
{
	unsigned char data = UDR0;
	if(uart_buffer_towrite < UART_BUFFER_LEN)
	{
		uart_buffer[uart_buffer_index] = data;
		uart_buffer_towrite++;
		uart_buffer_index++;
		if(uart_buffer_index >= UART_BUFFER_LEN)
		{
			uart_buffer_index = 0;
		}	
	}
}

int main(void)
{	
	EICRA = (1<<ISC01); //set interrupt for falling edge
	EIMSK = (1<<INT0);  //enable int0 interrupts
	TCCR1A = 0x00;  	//no pinchange
	TCCR1B = (1<<CS10) + (1<<WGM12); //1x prescaler + CTC output compare mode
	TIMSK1 = (1<<OCIE1A) + (1<<OCIE1B); //enable output compare interrupt A
	TCCR0A = 0x00;
	TCCR0B = (1<<CS01);
	TIMSK0 = (1<<OCIE0A);
	OCR0A = 50;
	OCR1A = 0xFFFE; 	
	OCR1B = 0x0008;		
	DDRB = 0x00;		
	PORTB = 0x00;		//high-z inputs
	DDRD = 0x00;		//port d is for int0 (input)
	PORTD = 0x00;		//high-z inputs
	usartInit(myubrr);	//initialize USART w/ baud register value ubrr
	TCNT1 = 0; 			//init clock @ 0
	sei();				//enable interrupts
	while(1)
	{
		//typeChar('h');
	}
}


void usartInit(unsigned int ubrr)
{
	UBRR0H = (unsigned char)(ubrr>>8);	// Set baud rate
	UBRR0L = (unsigned char)ubrr;
	UCSR0B = (1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0);	//Enable receiver and transmitter  
	UCSR0C = (0<<USBS0)|(3<<UCSZ00);	// Set frame format: 8data, 1stop bit
}

void typeChar(const char ch)
{
	unsigned char letter = lookup[(int)ch];
	unsigned char sig = ((letter&0xf0)>>4);
	wave = wavetable[sig];
	pin = (letter&0x07)+1;
	hit = 1;
	nexthit=0;
}
