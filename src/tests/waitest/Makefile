# Build src/tests/waitest.bin, the WAI interrupt wake regression ROM.
#
# Needs cc65 (ca65/ld65). The built ROM is committed, so running the test needs
# no toolchain - same arrangement as cputest816 and AllSuiteA. waitest.asm
# explains what the ROM checks and why.

ROM = ../waitest.bin

.PHONY: all clean

all: $(ROM)

waitest.o: waitest.asm
	ca65 --cpu 65816 $< -o $@

$(ROM): waitest.o waitest.cfg
	ld65 -C waitest.cfg -o $@ $<

clean:
	rm -f waitest.o
