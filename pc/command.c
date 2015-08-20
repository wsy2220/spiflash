/* Basic command implementation. 24-bit address only */
#include "system.h"
#include "serial_pc.h"
#define CMD_RETRY 100
#define CE_TIMEOUT 10
#define BE_TIMEOUT 5
#define PP_TIMEOUT 2
#define SE_TIMEOUT 2

typedef struct {
	int Inum;
	int Onum;
	char *cmd;
} command;

void print_array(FILE *stream, char *data, int n)
{
	int i;
	unsigned char c;
	for(i = 0; i < n; i++){
		c = (unsigned char) data[i];
		fprintf(stream, "%02X ", c);
	}
}



/* echo error message with given command */
static void cmd_err(command *cmd, char *msg){
	fprintf(stderr, msg);
	print_array(stderr, cmd->cmd, cmd->Inum);
	fprintf(stderr,"\n");
}

/* do a complete communication by sending given command and read data back
 * return 0 on success, -1 on error. 
 * high level function, echo error info on failure */
static int command_rw(int fd, command *cmd, char *Odata)
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
int RDID(int fd, char *buf)
{
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


