#include <avr/io.h>
#include <ctype.h>

#define READ_TIMEOUT 0xFFFFFUL
#define CS_LOW PORTB &= ~_BV(PB0)
#define CS_HIGH PORTB |= _BV(PB0)

#define F_CPU 16000000UL
#define BAUD  9600UL
#include <util/setbaud.h>
#include <util/delay.h>
/* convert a single byte to 2 ascii codes */
void byte2hex(char c, char *hex)
{
	unsigned char h = c;
	h = h >> 4;
	if(h < 10)
		*hex = h + 0x30;
	else
		*hex = h + 0x41 - 10;
	h = c & 0x0F;
	hex++;
	if(h < 10)
		*hex = h + 0x30;
	else
		*hex = h + 0x41 - 10;
}
	

/* init UART0 for transfer by polling */
void serial_init()
{
	/* baud rate */
	UBRR0H = UBRRH_VALUE;
	UBRR0L = UBRRL_VALUE;

	/* 8N1 frame */
	UCSR0C = _BV(UCSZ00) | _BV(UCSZ01);

	/* setup pull-up resistor on tx when tx disabled */
	DDRE &= ~_BV(PE1);
	PORTE |= _BV(PE1);

	/* enable tx */

}

void serial_write(char *c, int n)
{
	UCSR0B |= _BV(TXEN0) ;
	int i;
	for(i = 0; i < n; i++) {
		while( !(UCSR0A & _BV(UDRE0)) )
			;
		UDR0 = *(c + i);
	}
	/* no need to wait */
	UCSR0B &= ~_BV(TXEN0) ;
}

/* read n bytes from serial, return bytes actually read */
int serial_read(char *c, int n)
{
	UCSR0B |= _BV(RXEN0) ;
	int i ;
	unsigned long j;
	char error;
	for(i = 0; i < n; i++) {
		/* Wait for new data */
		for(j = 0; !(UCSR0A & _BV(RXC0)) && j < READ_TIMEOUT ; j++)
			;

		if(j == READ_TIMEOUT){
			break;
		}
		error = UCSR0A & ( _BV(FE0) | _BV(DOR0) | _BV(UPE0) );
		if(error)
			break;
		*(c + i) = UDR0;
	}
	UCSR0B &= ~_BV(RXEN0) ;
	return i;
}

/* initialize SPI with CPOL=CPHA=0, MSB first, double speed */
void spi_init()
{
	/* set output, PB0 as CS# */
	DDRB |= _BV(PB2) | _BV(PB1) | _BV(PB0);
	//SPSR = _BV(SPI2X);
	SPCR = _BV(MSTR) | _BV(SPE) | _BV(SPR0) | _BV(SPR1) ;
}

/* write wn bytes, then read rn bytes back, half-duplex */
void spi_rw(char *wbuf, int wn, char *rbuf, int rn)
{
	CS_LOW;
	int i;
	volatile char temp;
	for(i = 0; i < wn + rn; i++){
		if(i < wn)
			SPDR = *(wbuf + i);
		else
			SPDR = 0;
		while( !(SPSR & _BV(SPIF)) )
			;
		if(i >= wn)
			*(rbuf + i - wn) = SPDR;
		else
			temp = SPDR;
	}
	CS_HIGH;
}


int main()
{
	serial_init();
	spi_init();
	char cmd = 0x9f;
	char buf[3];
	char hex[8];
	hex[6] = '\n';
	hex[7] = '\r';
	int i;
	for(;;){
		spi_rw(&cmd, 1, buf, 3);
		for(i=0; i<3; i++)
			byte2hex(buf[i], hex+2*i);
		serial_write(hex, 8);
	}
	return 0;
}
