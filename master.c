#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BAUD B115200
#define RW_TIMEOUT 20
#define ACK_TIMEOUT 10
#define CMD_RETRY 100
#define CE_TIMEOUT 10
#define BE_TIMEOUT 5
#define PP_TIMEOUT 2
#define SE_TIMEOUT 2

#define RD_BLOCK 0xffff
#define PP_BLOCK 0x100

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
	char c = 0;
	time_t t0, t1;
	t0 = t1 = time(NULL);
	while(serial_read(fd, &c, 1) < 1 && (t1-t0) < ACK_TIMEOUT){
		t1 = time(NULL);
	}
	if(c == ACK)
		return 1;
	else{
		if(c == NAK)
			printf("NAK!!\n");
		return 0;
	}
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

/* block erase. assume block size 64k. block_addr = addr & 0xFF0000
 * check status register to make sure completed
 * write enable bit is cleared after BE
 * return 0 on sucess, -1 on failure */
int BE(int fd, int addr)
{
	int i, result = -1;
	char status = 0x01;   /* assume write in progress */
	char be[4] ;
	be[0] = 0x52;
	append_addr(be, addr);
	command cmd_be = {4, 0, be};
	for(i = 0; result && i < CMD_RETRY; i++)
		result = command_rw(fd, &cmd_be, NULL);
	if(result)
		return result;
	time_t t0, t1;
	t0 = t1 = time(NULL);
	while((status & 0x01) && (t1 - t0) < BE_TIMEOUT)
		RDSR(fd, &status);
	if(status & 0x01)
		return -1;
	return 0;
}

/* sector erase. assume sector size 4k, sector_addr=addr & 0xFFF
 * check status register to make sure completed
 * write enable bit is cleared after SE
 * return 0 on sucess, -1 on failure */
int SE(int fd, int addr)
{
	int i, result = -1;
	char status = 0x01;   /* assume write in progress */
	char se[4] ;
	se[0] = 0x20;
	append_addr(se, addr);
	command cmd_se = {4, 0, se};
	for(i = 0; result && i < CMD_RETRY; i++)
		result = command_rw(fd, &cmd_se, NULL);
	if(result)
		return result;
	time_t t0, t1;
	t0 = t1 = time(NULL);
	while((status & 0x01) && (t1 - t0) < SE_TIMEOUT)
		RDSR(fd, &status);
	if(status & 0x01)
		return -1;
	return 0;
}

void printhelp(char *argv0)
{
	printf("Usage: %s [options]\n",argv0);
	printf("Opions:\n");
	printf("  -p <port>         Required. Specify serial port device.\n");
	printf("  -f <filename>     Specify a file to read or written.\n");
	printf("  -r                Dump rom content into file.\n");
	printf("  -w                Program rom content from file.\n");
	printf("  -b <file_offset>  Read file start from offset value.\n");
	printf("  -B <rom_offset>   Read file start from offset value.\n");
	printf("  -s <size>         Read or write a given size.\n");
	printf("                    Prefix 0x for hex value, 00 for octal value\n");
	printf("  -e                Perform a chip erase, other options are ignored\n");
	printf("  -h                Print this message\n");
}

int main(int argc, char **argv)
{
	char *port = NULL, *path = NULL;
	int isread=0, iswrite=0, isce = 0, offset_rom=0,size=0,opt;
	long offset_file = 0;
	if(argc == 1){
		printhelp(argv[0]);
		exit(1);
	}
	while((opt = getopt(argc, argv, "p:f:b:B:s:rweh")) != -1){
		switch(opt){
			case 'p':
				port = optarg;
				break;
			case 'f':
				path = optarg;
				break;
			case 'b':
				offset_file = strtol(optarg, NULL, 0);
				if(offset_file < 0){
					fprintf(stderr,"Wrong file offset\n");
					exit(1);
				}
				break;
			case 'B':
				offset_rom = (int)strtol(optarg, NULL, 0);
				if(offset_rom < 0 || offset_rom > 0xFFFFFF){
					fprintf(stderr,"Wrong rom offset\n");
					exit(1);
				}
				break;
			case 's':
				size = (int)strtol(optarg, NULL, 0);
				if(size < 0 || size > 0xFFFFFF || size + offset_rom > 0xFFFFFF){
					fprintf(stderr,"Wrong size\n");
					exit(1);
				}
				break;
			case 'r':
				if(iswrite || isce){
					fprintf(stderr,"Only one action can be specified\n");
					exit(1);
				}
				isread = 1;
				break;
			case 'w':
				if(isread || isce){
					fprintf(stderr,"Only one action can be specified\n");
					exit(1);
				}
				iswrite = 1;
				break;
			case 'e':
				if(isread || iswrite || offset_file || offset_rom || size){
					fprintf(stderr,"Only one action can be specified\n");
					exit(1);
				}
				isce = 1;
				break;
			case 'h':
			default:
				printhelp(argv[0]);
				exit(1);
		}
	}

	if(port == NULL){
		fprintf(stderr, "No port specified\n");
		exit(1);
	}
	if(path == NULL && !isce){
		fprintf(stderr, "No file specified\n");
		exit(1);
	}
	
	/* initialize serial port */
	int fd = serial_open(port);
	serial_set(fd, BAUD);
	char *buf = NULL;

	if(isce){
		printf("Performing chip erase...\n");
		if(WREN(fd) < 0){
			fprintf(stderr, "Cannot enable write.\n");
			exit(1);
		}
		if(CE(fd) < 0){
			fprintf(stderr, "Erase failed, please try again.\n");
			exit(1);
		}
		printf("Chip erased!\n");
	}

	FILE *file = NULL;
	tcflush(fd, TCIOFLUSH);
	int i, block;
	if(isread){
		buf = malloc(size);
		if(buf == NULL){
			fprintf(stderr, "Memory allocation failed.\n");
			exit(1);
		}
		if(!size){
			fprintf(stderr,"Please specify size for reading.\n");
			goto Fail;
		}
		if(offset_file){
			fprintf(stderr,"File offset is not allowed for option -r.\n");
			goto Fail;
		}
		file = fopen(path, "w");
		if(file == NULL){
			fprintf(stderr,"Failed to create file, %s\n", strerror(errno));
			goto Fail;
		}
		printf("Reading rom content\n");
		/* get number of  blocks */
		block = size / RD_BLOCK;
		for(i = 0; i < block; i++){
			if(RD(fd, buf + i * RD_BLOCK, offset_rom + i * RD_BLOCK, RD_BLOCK) < 0){
				fprintf(stderr,"RD instruction failed.\n");
				goto Fail;
			}
		}
		if((size % RD_BLOCK) &&
			RD(fd, buf + block * RD_BLOCK, 
			   offset_rom + block * RD_BLOCK, size % RD_BLOCK) < 0)
		{
			fprintf(stderr,"RD instruction failed.\n");
			goto Fail;
		}
		if(fwrite(buf, size, 1, file) < 1){
			fprintf(stderr,"File write failed\n");
			goto Fail;
		}
		printf("Operation complete.\n");
	}

	if(iswrite){
		file = fopen(path,"r");
		if(file == NULL){
			fprintf(stderr,"Failed to open file, %s\n", strerror(errno));
			goto Fail;
		}
		if(!size){
			struct stat st;
			if(stat(path, &st) < 0 || st.st_size > 0x1000000){
				fprintf(stderr,"Invalid file.\n");
				goto Fail;
			}
			size = (int)st.st_size;
		}
		printf("Perfroming programming...\n");
		/* check offset boundary */
		char buf_h[0x1000], buf_p[0x1000];
		int has_h = 0, has_p = 0, size_new, offset_new, block_pp;
		if(offset_rom & 0xFFF){
			has_h = 1;
			if(RD(fd, buf_h, offset_rom & ~0xFFF, 0x1000) < 0){
				fprintf(stderr,"RD instruction failed.\n");
				goto Fail;
			}
		}
		if((offset_rom + size) & 0xFFF){
			has_p = 1;
			if(RD(fd, buf_p, (offset_rom + size) & ~0xFFF, 0x1000) < 0){
				fprintf(stderr,"RD instruction failed.\n");
				goto Fail;
			}
		}
		offset_new = offset_rom & ~0xFFF;
		size_new = ((size + (offset_rom & 0xFFF)) & ~0xFFF) + (has_p * 0x1000);
		buf = malloc(size_new);
		if(buf == NULL){
			fprintf(stderr, "Memory allocation failed.\n");
			exit(1);
		}
		block = (size_new & ~0xFFF) >> 12;
		block_pp = (size_new & ~0xFF) >> 8;
		printf("Old size: %X\nNew size: %X\nOld offset: %X\nNew offset: %X\n",size,size_new,offset_rom, offset_new);
		memcpy(buf, buf_h, has_h * 0x1000);
		memcpy(buf + size_new - 0x1000, buf_p, has_p * 0x1000);
		/* over write part of first and last block */
		if(fread(buf + (offset_rom & 0xFFF), size, 1, file) < 1){
			fprintf(stderr,"failed to read file.\n");
			goto Fail;
		}
		for(i = 0; i < block; i++){
			printf("Erasing block at %X\n", offset_new + i*0x1000);
			if(WREN(fd) < 0){
				fprintf(stderr,"Cannot enable write.\n");
				goto Fail;
			}
			if(SE(fd, offset_new + i*0x1000)){
				fprintf(stderr,"Erase failed.\n");
				goto Fail;
			}
		}
		for(i = 0; i < block_pp; i++){
			if(WREN(fd) < 0){
				fprintf(stderr,"Cannot enable write.\n");
				goto Fail;
			}
			printf("Writing page at %X\n", offset_new + i*0x100);
			if(PP(fd, buf + i * 0x100, offset_new + i*0x100, 0x100) < 0){
				fprintf(stderr,"Page write fail at %X\n", offset_new + i*0x100);
				goto Fail;
			}
		}
	}
	
	if(file)
		fclose(file);
	if(buf)
		free(buf);
	close(fd);
	exit(0);

Fail:
	if(!buf)
		free(buf);
	close(fd);
	exit(1);
}

