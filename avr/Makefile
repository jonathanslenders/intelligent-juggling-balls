# makefile, written by guido socher
#MCU=atmega88
MCU=atmega168
CC=avr-gcc
OBJCOPY=avr-objcopy
# optimize for size:
CFLAGS=-g -mmcu=$(MCU) -Wall -Wstrict-prototypes -Os -mcall-prologues
#-------------------
.PHONY: all fuse
all: main.hex
#-------------------
help: 
	@echo "Help?"
#-------------------
main.hex : main.out 
	$(OBJCOPY) -R .eeprom -O ihex main.out main.hex 

main.o : main.c 
	$(CC) $(CFLAGS) -Os -c main.c

main.out : main.o
	$(CC) $(CFLAGS) -o main.out -Wl,-Map,main.map main.o
#------------------
load: main.hex
	./prg_load_uc main.hex
#
loaduisp: main.hex
	./prg_load_uc -u main_pre.hex
#-------------------
# fuse byte settings:
#  Atmel AVR ATmega8 
#  Fuse Low Byte      = 0xe1 (1MHz internal), 0xe3 (4MHz internal), 0xe4 (8MHz internal)
#  Fuse High Byte     = 0xd9 
#  Factory default is 0xe1 for low byte and 0xd9 for high byte
# Check this with make rdfuses
rdfuses:
	./prg_fusebit_uc -r
#
fuse: wrfuse1mhz
#
# use internal RC oscillator 1 Mhz
wrfuse1mhz:
	./prg_fusebit_uc -w 1
# use internal RC oscillator 8 Mhz
wrfuse8mhz:
	./prg_fusebit_uc -w 8
# use external 3-8 Mhz crystal
# Warning: you can not reset this to intenal unless you connect a crystal!!
#-------------------
clean:
	rm -f *.o *.map *.out main.hex
#-------------------
