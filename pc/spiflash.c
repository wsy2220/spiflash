#include "system.h"
#include "serial_pc.h"
#include "command.h"

#define RD_BLOCK 0xffff
#define PP_BLOCK 0x100





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
	
	char id[3];
	if(RDID(fd, id) < 0)
		fprintf(stderr, "Cannot get chip ID, trying to continue.\n");
	else{
		printf("Chip ID: ");
		print_array(stdout, id, 3);
		printf("\n");
	}

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
			goto Fail;
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
		printf("Erasing block...\n");
		for(i = 0; i < block; i++){
			if(WREN(fd) < 0){
				fprintf(stderr,"Cannot enable write.\n");
				goto Fail;
			}
			if(SE(fd, offset_new + i*0x1000)){
				fprintf(stderr,"Erase failed.\n");
				goto Fail;
			}
		}
		printf("Writing page...\n");
		for(i = 0; i < block_pp; i++){
			if(WREN(fd) < 0){
				fprintf(stderr,"Cannot enable write.\n");
				goto Fail;
			}
			if(PP(fd, buf + i * 0x100, offset_new + i*0x100, 0x100) < 0){
				fprintf(stderr,"Page write fail at %X\n", offset_new + i*0x100);
				goto Fail;
			}
		}
		printf("Operation complete.\n");
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

