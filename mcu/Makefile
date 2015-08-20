mcu = atmega128a
project = programmer
objects = programmer.o serial.o spi.o 
CC  = avr-gcc
CFLAGS = -mmcu=$(mcu) -Os -Wall

hex: elf
	avr-objcopy  -j .text -j.data -O ihex $(project).elf $(project).hex

elf: $(objects)
	$(CC) $(CFLAGS) -o $(project).elf $(objects)

.PHONY: clean

clean: 
	-rm $(project).elf $(project).hex $(objects)
