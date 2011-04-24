
MCU=atmega328p
CC=avr-gcc
OBJCOPY=avr-objcopy
# optimize for size:
CFLAGS=-g -mmcu=$(MCU) -Wall -Wstrict-prototypes -Os -mcall-prologues



.PHONY: all fuse
all: main.hex


# Compile
main.hex : main.out 
	$(OBJCOPY) -R .eeprom -O ihex main.out main.hex 

main.o : main.c 
	$(CC) $(CFLAGS) -Os -c main.c

main.out : main.o
	$(CC) $(CFLAGS) -o main.out -Wl,-Map,main.map main.o

# Load firmware into microcontroller
load: main.hex
	avrdude -p m328p -c avrusb500 -e -U flash:w:"main.hex"

# Read fuses
rdfuses:
	avrdude -p m328p -c avrusb500 -v -q


# Fuses (external crystal 16MHz; clock output enabled)
fuse:
	avrdude -p m328p -c avrusb500 -U lfuse:w:0x96:m -U hfuse:w:0xd9:m #-U efuse:w:0xff:m


# Clean temp files
clean:
	rm -f *.o *.map *.out main.hex

