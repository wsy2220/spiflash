#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>

#define BAUD B115200
#define RW_TIMEOUT 5
#define CMD_RETRY 100
#define CE_TIMEOUT 10
#define BE_TIMEOUT 5
#define PP_TIMEOUT 1

#define ACK 0x06
#define NAK 0x15
#define SOH 0x01
#define STX 0x02
#define ETX 0x03

typedef struct {
	int Inum;
	int Onum;
	char *cmd;
} command;
	

/*append 24 bit address to data in big endian, 
 *start from the second byte */
void append_addr(char *data, int addr)
{
	data[1] = (addr >> 16) & 0xFF;
	data[2] = (addr >> 8 ) & 0xFF;
	data[3] = addr & 0xFF;
}

void print_array(FILE *stream, char *data, int n)
{
	int i;
	unsigned char c;
	for(i = 0; i < n; i++){
		c = (unsigned char) data[i];
		fprintf(stream, "%02X ", c);
	}
}

int serial_open(char *port)
{
	int fd;
	int flag = O_RDWR | O_NOCTTY | O_NDELAY;
	fd = open(port, flag);
	if(fd < 0) {
		fprintf(stderr, "Failed to open port, %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	return fd;
}

/* set port to 8N1 mode, given baud in B*  */
int serial_set(int fd, int baud)
{
	struct termios option;

	if(tcgetattr(fd, &option) < 0){
		fprintf(stderr, "Failed to get port attr, %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	option.c_iflag = 0;
	option.c_oflag = 0;
	option.c_cflag  = CS8 | CREAD | CLOCAL;
	option.c_lflag = 0;
	cfsetospeed(&option, baud);
	cfsetispeed(&option, baud);

	if(tcsetattr(fd, TCSANOW, &option) < 0){
		fprintf(stderr, "Failed to get port attr, %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	return 0;
}

/* unbuffered reliable write, 
 * return number of bytes actually written */
ssize_t serial_write(int fd, char *buf, size_t count)
{
	size_t left = count;
	ssize_t n;
	time_t t0, t1;
	t0 = t1 = time(NULL);
	while(left > 0 && (t1 - t0) < RW_TIMEOUT){
		n = write(fd, buf + count - left, left);
		if(n > 0)
			left -= n;
		t1 = time(NULL);
	}
	return count - left;
}

/* try to get enough data before timeout
 * return bytes actually read */
ssize_t serial_read(int fd, char *buf, size_t count)
{
	size_t left = count;
	ssize_t n;
	time_t t0, t1;
	t0 = t1 = time(NULL);
	while(left > 0 && (t1 - t0) < RW_TIMEOUT){
		n = read(fd, buf + count - left, left);
		if(n > 0)
			left -= n;
		t1 = time(NULL);
	}
	return count - left;
}

/* unbuffered, append STX and ETX
 * return 0 on sucess, -1 on short or error */
int send_data(int fd, char *data, size_t size)
{
	char stx = STX, etx = ETX;
	if(serial_write(fd, &stx, 1) < 1){
		return -1;
	}
	if(serial_write(fd, data, size) < size){
		return -1;
	}
	if(serial_write(fd, &etx, 1) < 1){
		return -1;
	}
	return 0;
}

/* send header of given Inum and Onum
 * return 0 on sucess, -1 or short or error */
int send_header(int fd, int Inum, int Onum)
{
	short num[2] = {(short)Inum, (short)Onum};
	char soh = SOH;
	if(serial_write(fd, &soh, 1) < 1)
		return -1;
	if(serial_write(fd, (char *)num, 4) < 4)
		return -1;
	return 0;
}

/* read data of given Onum, STX and ETX are checked.
 * return 0 on sucess, -1 or short or error */
int read_data(int fd, char *buf, int Onum)
{
	char c;
	int n;
	if((n = serial_read(fd, &c, 1) < 1) || c != STX){
		return -1;
	}
	if((n = serial_read(fd, buf, Onum)) < Onum){
		return -1;
	}
	if(serial_read(fd, &c, 1) < 1 || c != ETX){
		return -1;
	}
	return 0;
}

/* read one byte from serial, check if it is ACK.
 * return 1 on ACK, 0 on error */
int isACK(int fd)
{
	char c;
	int n;
	if((n = serial_read(fd, &c, 1)) < 1 || c != ACK){
		return 0;
	}
	return 1;
}

/* echo error message with given command */
void cmd_err(command *cmd, char *msg){
	fprintf(stderr, msg);
	print_array(stderr, cmd->cmd, cmd->Inum);
	fprintf(stderr,"\n");
}

/* do a complete communication by sending given command and read data back
 * return 0 on success, -1 on error. 
 * high level function, echo error info on failure */
int command_rw(int fd, command *cmd, char *Odata)
{
	if(send_header(fd, cmd->Inum, cmd->Onum) < 0){
		cmd_err(cmd, "send_header fail:");
		return -1;
	}

	if(!isACK(fd)){
		cmd_err(cmd, "no header ACK:");
		return -1;
	}
	if(send_data(fd, cmd->cmd, cmd->Inum) < 0){
		cmd_err(cmd, "send_data fail:");
		return -1;
	}
	if(!isACK(fd)){
		cmd_err(cmd, "no data ACK:");
		return -1;
	}
	if(read_data(fd, Odata, cmd->Onum) < 0){
		cmd_err(cmd, "read_data fail:");
		return -1;
	}
	return 0;
}

/* read device ID */
int RDID(int fd, char *buf){
	char rdid[1] = {0x9F};
	command cmd = {1, 3, rdid};
	int i, result = 1;
	for(i = 0; result && i < CMD_RETRY; i++)
		result = command_rw(fd, &cmd, buf); 
	return result;
}

/* read status register */
int RDSR(int fd, char *status)
{
	
	char rdsr[1] = {0x05};
	command cmd_rdsr = {1, 1, rdsr};
	int i, result = -1;
	for(i = 0; result && i < CMD_RETRY; i++)
		result = command_rw(fd, &cmd_rdsr, status);
	return result;
}

/* enable write, will check status register to make sure succeeded
 * 0 on success, -1 on failure */
int WREN(int fd)
{
	int i ;
	char wren[1] = {0x06};
	char status = 0;
	command cmd_wren = {1, 0, wren};
	for(i = 0; i < CMD_RETRY && !(status & 0x02); i++){
		command_rw(fd, &cmd_wren, NULL);
		RDSR(fd, &status);
	}
	if(i == CMD_RETRY)
		return -1;
	return 0;
}

/* disable write, will check status register to make sure succeeded
 * 0 on success, -1 on failure */
int WRDI(int fd)
{
	int i;
	char wrdi[1] = {0x04};
	char status = 0x02;
	command cmd_wrdi = {1, 0, wrdi};
	for(i = 0; i < CMD_RETRY && (status & 0x02); i++){
		command_rw(fd, &cmd_wrdi, NULL);
		RDSR(fd, &status);
	}
	if(i == CMD_RETRY)
		return -1;
	return 0;
}

/* read size bytes into buf, starting at addr
 * effective addr is 24 bits, higher bits will be discarded
 * return 0 on success, -1 on failure */
int RD(int fd, char *buf, int addr, int size)
{
	int i, result = -1;
	/* address in big endian */
	char rd[4];
	rd[0] = 0x03;
	append_addr(rd, addr);
	command cmd_rd = {4, size, rd};
	for(i = 0; result && i < CMD_RETRY; i++)
		result = command_rw(fd, &cmd_rd, buf);
	return result;
}

/* chip erase, will check status register to make sure completed.
 * assuming the command is accepted by the chip once sent.
 * since chip erase typically require several seconds, 
 * we can sleep some time to save cpu.
 * write-enable bit will be cleared after CE
 * return 0 on success, -1 on failure */
int CE(int fd)
{
	int i, result = -1;
	char status = 0x01;   /* assume write in progress */
	char ce[1] = {0x60};
	command cmd_ce = {1, 0, ce};
	for(i = 0; result && i < CMD_RETRY; i++)
		result = command_rw(fd, &cmd_ce, NULL);
	if(result)
		return result;
	for(i = 0; (status & 0x01) && i < CE_TIMEOUT; i++){
		sleep(1);
		RDSR(fd, &status);
	}
	if(status & 0x01)
		return -1;
	return 0;
}

/* program selected page at addr. data size <= 256
 * will check status to make sure completed
 * assuming the command is accepted by the chip once sent.
 * DO NOT LET size + (addr & 0xFF) > 0x100
 * return 0 on success, -1 on failure */
int PP(int fd, char *data, int addr, int size)
{
	int i, result = -1;
	char status = 0x01;
	char pp[4+256];     /* pre-allocate enough space */
	pp[0] = 0x02;
	append_addr(pp, addr);
	memcpy(pp+4, data, size);
	command cmd_rd = {4 + size, 0, pp};
	for(i = 0; result && i < CMD_RETRY; i++)
		result = command_rw(fd, &cmd_rd, NULL);
	if(result)
		return -1;
	time_t t0, t1;
	t0 = t1 = time(NULL);
	while((status & 0x01) && (t1 - t0) < PP_TIMEOUT)
		RDSR(fd, &status);
	if(status & 0x01)
		return -1;
	return 0;
}

int main(int argc, char **argv)
{
	if(argc != 2){
		printf("usage: %s port\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	int fd = serial_open(argv[1]);
	serial_set(fd, BAUD);
	char buf[0x10000];
	int i;
	for(i = 0; i < 1; i++){
		sleep(1);
		tcflush(fd, TCIOFLUSH);
		if(WREN(fd) < 0){
			printf("WREN fail\n");
			continue;
		}
		printf("WREN OK!\n");
		//if(CE(fd) < 0){
		//	printf("CE fail\n");
		//	continue;
		//}
		//printf("CE OK!\n");
		int j;
		for(j=0; j<16; j++)
			buf[j] = j;
		if(PP(fd, buf, 0x10, 16) < 0){
			printf("PP fail\n");
			continue;
		}
		printf("PP OK!\n");
		if(WRDI(fd) < 0) {
			printf("WRDI fail\n");
			continue;
		}
		printf("WRDI OK!\n");
		if(RD(fd, buf, 0x00, 32) < 0){
			printf("READ fail\n");
			continue;
		}
		print_array(stdout, buf, 32);
		printf("\n");
	}
	close(fd);
	return 0;
}

