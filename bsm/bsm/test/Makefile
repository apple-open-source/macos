## Probably only works with GNU make

ifndef CFLAGS
CFLAGS = -g -Wall -pipe -fno-common -I/usr/include $(CCFLAGS) 
endif
TESTCFLAGS = $(CFLAGS) -I../lib
BSMLIBPATH = -L../lib -L/usr/lib
GCC=gcc 
OUTFILES = tok_test lib_test

all: $(OUTFILES)

install: $(OUTFILES)

tok_test:
	$(GCC) $(TESTCFLAGS) $(BSMLIBPATH) -lbsm bsm_tok_test.c -o tok_test

lib_test:
	$(GCC) $(TESTCFLAGS) $(BSMLIBPATH) -lbsm bsm_lib_test.c -o lib_test  

clean: 
	rm -f $(OUTFILES)

