CC = gcc
CFLAGS = -Wall
LDFLAGS = -lprussdrv

PRU_CC = pasm

all: pru1_cic loading

clean:
	-@rm gen/*

# Assemble pru files and move them to the gen/ directory
pru1_cic: cic_pru1.asm prudefs.hasm
	@tput bold
	@echo "\n----- Building PRU1 (CIC) Firmware -----"
	@tput sgr0
	$(PRU_CC) -b cic_pru1.asm
	mv *.bin gen/

# Build the loader program
loading: loader.c
	@tput bold
	@echo "\n----- Building C Host Loader -----"
	@tput sgr0
	$(CC) $(CFLAGS) -o loader loader.c $(LDFLAGS)
	mv loader gen/
