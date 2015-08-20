#include "avr.h"
#define READ_TIMEOUT 0xFFFFFUL

/* init UART0 for transfer by polling */
void serial_init()
{
	/* baud rate */
	//#include <util/setbaud.h>
	//UBRR0H = UBRRH_VALUE;
	//UBRR0L = UBRRL_VALUE;
	/* 115200 */
	UBRR0H = 0;
	UBRR0L = 8;

	/* 8N1 frame */
	UCSR0C = _BV(UCSZ00) | _BV(UCSZ01);

	/* setup pull-up resistor on tx when tx disabled */
	DDRE &= ~_BV(PE1);
	PORTE |= _BV(PE1);

	/* enable tx */
	UCSR0B |= _BV(TXEN0) | _BV(RXEN0);

}

void serial_write(uint8_t *c, uint16_t n)
{
	uint16_t i;
	for(i = 0; i < n; i++) {
		while( !(UCSR0A & _BV(UDRE0)) )
			;
		UDR0 = *(c + i);
	}
}

/* read n bytes from serial, return bytes actually read */
uint16_t serial_read(uint8_t *c, uint16_t n, uint8_t no_timeout)
{
	uint16_t i;
	uint32_t j;
	uint8_t error;
	for(i = 0; i < n; i++) {
		/* Wait for new data, if has received some, enable timeout */
		for(j = 0;
			!(UCSR0A & _BV(RXC0)) && ((no_timeout && !i) || j < READ_TIMEOUT) ;
		    j++)
			;

		if((!no_timeout || i) && j == READ_TIMEOUT ){
			break;
		}
		error = UCSR0A & ( _BV(FE0) | _BV(DOR0) | _BV(UPE0) );
		if(error)
			break;
		*(c + i) = UDR0;
	}
	return i;
}

/* flush UART0 rx buffer */
void rx_flush()
{
	UCSR0B &= ~_BV(RXEN0) ;
	UCSR0B |= _BV(RXEN0) ;
}


