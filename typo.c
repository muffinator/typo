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

volatile char type = 1;
volatile unsigned char pinput, winput, slot, wave, pin, hit = HITSTOP;

void usartInit( unsigned int ubrr); 	//initialize USART

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
	PORTB = 0x00;
	pinput = PINB;
	winput = invwave[slot];
	DDRB = temp;
	PORTB = ptemp;
}

ISR(TIMER1_COMPA_vect)
{
	TCNT1 = 0;
	TCNT0 = 0;
	OCR1A = 820;
	char pinshift = (1<<pin);
	slot++;
	if((slot == wave)
	{
		if(hit < HITSTOP))
		{
			DDRB |= pinshift;
			PORTB &= ~pinshift;
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
			type = 1;
		}
	}
	else
	{
		DDRB &= ~pinshift;
		PORTB &= ~pinshift;
	}
}

int main(void)
{	
	EICRA = (1<<ISC00); //set interrupt for falling edge
	EIMSK = (1<<INT0);  //enable int0 interrupts
	TCCR1A = 0x00;  	//no pinchange
	TCCR1B = (1<<CS10) + (1<<WGM12); //1x prescaler + CTC output compare mode
	TIMSK1 = (1<<OCIE1A) + (1<<OCIE1B); //enable output compare interrupt A
	TIMSK0 = (1<<OCIE0A);
	TCCR0A = 0x00;
	TCCR0B = (1<<CS01);
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
	}
}


void usartInit(unsigned int ubrr)
{
	UBRR0H = (unsigned char)(ubrr>>8);	// Set baud rate
	UBRR0L = (unsigned char)ubrr;
	UCSR0B = (1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0);	//Enable receiver and transmitter  
	UCSR0C = (0<<USBS0)|(3<<UCSZ00);	// Set frame format: 8data, 1stop bit
}
