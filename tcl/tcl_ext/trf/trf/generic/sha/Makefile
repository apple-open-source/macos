# By default, the code is compiled for a "big endian" machine.
# To compile on a "little endian" machine set the LITTLE_ENDIAN flag.
# To make smaller object code, but run a little slower, don't use UNROLL_LOOPS.
# To use NIST's modified SHA of 7/11/94, define USE_MODIFIED_SHA

CC = gcc
CFLAGS = -O2 -ansi -Wall -pedantic -DUNROLL_LOOPS -DLITTLE_ENDIAN # -DUSE_MODIFIED_SHA

all:	sha

sha:	sha_driver.o sha.o
	$(CC) -o $@ sha_driver.o sha.o
	strip $@

test:	sha appgen
	appgen 1 | sha
	appgen 2 | sha
	appgen 3 | sha
	@echo "Values should be:"
	@cat sha.good.outputs
	@echo "except if you defined USE_MODIFIED_SHA!"
	@echo "I have no known-good comparisons for that case!"

clean:
	rm *.o sha appgen
