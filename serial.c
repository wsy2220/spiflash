#include <avr/io.h>
#include <avr/interrupt.h>

#define F_CPU 8000000UL
#define BAUD  19200

#include <util/setbaud.h>

/* setup a stack for buffer */
#define BUFSIZE 80
char buf[BUFSIZE];
char *top = buf;

int push(char c)
{
	if((top - buf) < BUFSIZE){
		*top = c;
		top ++;
	}
	else
		return -1;
	return 0;
}

int pop(char *c)
{
	if((top - buf) > 0){
		top --;
		*c = *top;
	}
	else
		return -1;
	return 0;
}
	


void serial_init()
{
	/* baud rate */
	UBRR0H = UBRRH_VALUE;
	UBRR0L = UBRRL_VALUE;

	/* frame format 8N1 */
	UCSR0C = _BV(UCSZ00) | _BV(UCSZ01);

	/* enable  rx & interrupt */

	UCSR0B = _BV(TXEN0) | _BV(RXCIE0) | _BV(RXEN0);
}


ISR(USART0_RX_vect)
{
	char c;
	char error;
	error = UCSR0A & ( _BV(FE0) | _BV(DOR0) | _BV(UPE0) );
	c = UDR0;
	if(!error){
		if(!push(c))
			UCSR0B |= _BV(UDRIE0); /* Enable tx */
	}
}

ISR(USART0_UDRE_vect)
{
	char c;
	if(!pop(&c))
		UDR0 = c;

	/* disable data empty interrupt */
	UCSR0B &= ~_BV(UDRIE0);
}


int main()
{
	cli();
	serial_init();
	sei();
	for(;;);
	return 0;
}
