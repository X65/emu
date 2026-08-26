# Build src/tests/cputest816.bin, the 65816 instruction suite ROM.
#
# The test bodies come from gilyon's snes-tests release; only the harness in
# this directory is ours. The generated .inc files are ~1 MB of machine-written
# assembly and are not committed - this fetches them. The built ROM *is*
# committed, so CI needs neither cc65 nor a network. See README.md.
#
#   make          fetch, assemble, link, and install over the committed ROM
#   make clean    drop everything this generated, keeping the download

UPSTREAM_VERSION = v1.4
UPSTREAM_URL = https://github.com/gilyon/snes-tests/releases/download/$(UPSTREAM_VERSION)/snes-tests.zip
UPSTREAM_SHA256 = c3a39e15a9f3789fe143e54dfa035c988204576eafacf97df8ff3546a917c5e9

ROM = ../cputest816.bin
SOURCES = main.asm io.asm
GENERATED = tests-full.inc tests_table.inc tests-full.txt

.PHONY: all clean verify-sha

all: $(ROM)

snes-tests.zip:
	curl -fsSL -o $@ $(UPSTREAM_URL)

make_cpu_tests.py: snes-tests.zip
	unzip -oj $< 'cputest/make_cpu_tests.py'
	touch $@

# Generate rather than unpack: the release ships tests-full.inc and
# tests-basic.inc but only one tests_table.inc, left over from whichever
# variant was generated last. Pairing the full tests with the basic table
# makes the test chain run off the end of the table into address $0000.
# tests-full.txt describes every test, and is what to read when one fails.
$(GENERATED): make_cpu_tests.py
	python3 make_cpu_tests.py
	touch $(GENERATED)

cputest816.o: $(SOURCES) $(GENERATED)
	ca65 --cpu 65816 main.asm -o $@

$(ROM): cputest816.o flat.cfg
	ld65 -C flat.cfg -o $@ $< -m cputest816.map

# Print the hash of what was actually downloaded, for updating UPSTREAM_SHA256
# when bumping UPSTREAM_VERSION.
verify-sha: snes-tests.zip
	sha256sum $<
	@echo "expected: $(UPSTREAM_SHA256)"

clean:
	rm -f cputest816.o cputest816.map make_cpu_tests.py $(GENERATED)
