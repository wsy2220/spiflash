#include <avr/io.h>
#include <ctype.h>

#define NULL 0

#define READ_TIMEOUT 0xFFFFFUL

#define DAT_SIZE 2048 /* max data length, buffer size is 2 more */
#define HDR_SIZE  5

#define F_CPU 16000000UL

#define CS_LOW PORTB &= ~_BV(PB0)
#define CS_HIGH PORTB |= _BV(PB0)
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
	//#include <util/setbaud.h>
	//UBRR0H = UBRRH_VALUE;
	//UBRR0L = UBRRL_VALUE;
	UBRR0H = 0;
	UBRR0L = 103;

	/* 8N1 frame */
	UCSR0C = _BV(UCSZ00) | _BV(UCSZ01);

	/* setup pull-up resistor on tx when tx disabled */
	DDRE &= ~_BV(PE1);
	PORTE |= _BV(PE1);

	/* enable tx */
	UCSR0B |= _BV(TXEN0) | _BV(RXEN0);

}

void serial_write(char *c, int n)
{
	int i;
	for(i = 0; i < n; i++) {
		while( !(UCSR0A & _BV(UDRE0)) )
			;
		UDR0 = *(c + i);
	}
}

/* read n bytes from serial, return bytes actually read */
int serial_read(char *c, int n, char no_timeout)
{
	int i ;
	unsigned long j;
	char error;
	for(i = 0; i < n; i++) {
		/* Wait for new data, if has received some, enable timeout */
		for(j = 0;
			!(UCSR0A & _BV(RXC0)) && (no_timeout || j < READ_TIMEOUT);
		    j++)
			;

		if(!no_timeout && j == READ_TIMEOUT){
			break;
		}
		error = UCSR0A & ( _BV(FE0) | _BV(DOR0) | _BV(UPE0) );
		if(error)
			break;
		*(c + i) = UDR0;
	}
	return i;
}

void rx_flush()
{
	UCSR0B &= ~_BV(RXEN0) ;
	UCSR0B |= _BV(RXEN0) ;
}


/* initialize SPI with CPOL=CPHA=0, MSB first, double speed */
void spi_init()
{
	/* set output, PB0 as CS# */
	DDRB |= _BV(PB2) | _BV(PB1) | _BV(PB0);
	SPSR = _BV(SPI2X);
	SPCR = _BV(MSTR) | _BV(SPE);
}

/* write wn bytes, then read rn bytes back, half-duplex 
 * chcs = 0: do not change cs status*/
void spi_rw(char *wbuf, int wn, char *rbuf, int rn, char chcs)
{
	if(chcs)
		CS_LOW;

	int i;
	for(i = 0; i < wn + rn; i++){
		if(i < wn)
			SPDR = *(wbuf + i);
		else
			SPDR = 0;
		while( !(SPSR & _BV(SPIF)) )
			;
		if(i >= wn)
			*(rbuf + i - wn) = SPDR;
	}
	if(chcs)
		CS_HIGH;
}

/* write wn bytes, read rn bytes and write to serial port */
void spi2serial(char *wbuf, int wn, int rn)
{
	CS_LOW;
	spi_rw(wbuf, wn, NULL, 0, 0);
	int i;
	char temp;
	for(i = 0; i < rn; i++){
		spi_rw(NULL, 0, &temp, 1, 0);
		serial_write(&temp, 1);
	}
	CS_HIGH;
}

int main()
{
	serial_init();
	spi_init();
	char header[HDR_SIZE];
	char buffer[DAT_SIZE + 2];
	int n, Inum, Onum;
	char ACK = 0x06, NAK = 0x15;
	char SOH = 0x01, STX = 0x02, ETX = 0x03;
	for(;;){
		rx_flush();
		/* read header, no timeout */
	n = serial_read(header, HDR_SIZE, 1);
	if(n < HDR_SIZE || header[0] != SOH){
		serial_write(&NAK, 1);
		continue;
	}
	Inum = header[1] + (header[2] << 8);
	Onum = header[3] + (header[4] << 8);
	if(Inum > DAT_SIZE){
		serial_write(&NAK, 1);
		continue;
	}
	serial_write(&ACK, 1);

	/* read data */
	n = serial_read(buffer, Inum + 2, 0);
	if(n < (Inum + 2) || buffer[0] != STX || buffer[Inum+1] != ETX){
		serial_write(&NAK, 1);
		continue;
	}
	serial_write(&ACK, 1);
	/* spi rw, directly send data to serial port, so size limit is 64k */
	serial_write(&STX, 1);
	spi2serial(buffer+1, Inum, Onum);
	serial_write(&ETX, 1);
	}
	return 0;
}
