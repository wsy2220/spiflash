#include <avr/io.h>
#include <avr/interrupt.h>

#define F_CPU 8000000UL
#define BAUD  19200
#include <util/setbaud.h>

/* setup a stack for buffer */
#define BUF_IN_SIZE 256
char buf_in[BUF_IN_SIZE];
int  buf_in_position;

#define BUF_OUT_SIZE 256
char buf_out[BUF_OUT_SIZE];
int  buf_out_position;
int  buf_out_start;

#define ENABLE_TX UCSR0B |= _BV(UDRIE0) 
#define DISABLE_TX UCSR0B &= ~_BV(UDRIE0)
#define ENABLE_RX UCSR0B |= _BV(RXCIE0)
#define DISABLE_RX UCSR0B &= ~_BV(RXCIE0)

char *memcpy(char *dest, const char *src, int n)
{
	int i;
	for(i=0; i<n; i++)
		dest[i] = src[i];
	return dest;
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
	if(error){
		return;
	}
	if(c != '\r' && buf_in_position < BUF_IN_SIZE)
		buf_in[buf_in_position++] = c;
	else{
		buf_in[buf_in_position++] = '\r';
		buf_in[buf_in_position++] = '\n';
		memcpy(buf_out, buf_in, buf_in_position);
		buf_out_position = buf_in_position;
		buf_out_start = 0;
		buf_in_position = 0;
		DISABLE_RX;
		ENABLE_TX;
	}
}

ISR(USART0_UDRE_vect)
{
	if(buf_out_start < buf_out_position)
		UDR0 = buf_out[buf_out_start++];
	else{
		buf_out_start = buf_out_position = 0;
		DISABLE_TX;
		ENABLE_RX;
	}
}


int main()
{
	cli();
	serial_init();
	sei();
	for(;;);
	return 0;
}
