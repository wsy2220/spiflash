CC = gcc
CFLAGS = -O2 -Wall
objects = spiflash.o serial_pc.o command.o
project = spiflash

all: $(objects)
	$(CC) $(CFLAGS) -o $(project) $(objects)

.PHONY: clean

clean:
	-rm $(project) $(objects)
