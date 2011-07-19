GCC=gcc
BFLAGS=-g -std=gnu99

TFR = ~public/bin/testfilerunner

all: momc tests

clean:
	rm -fr $(GCF:.m=.gcf32) $(GCF:.m=.gcf64) $(GCF:.m=.gcof32) $(GCF:.m=.gcof64) *.dSYM *-bin

dump: dump.m
	gcc -fobjc-gc -g -std=gnu99 dump.m -o dump -framework Foundation -L/GC/build -lauto

momc::
	/Developer/usr/bin/momc -XD_MOMC_TARGET_VERSION=10.6 MyDocument.xcdatamodel MyDocument.mom

tests:
	grep CONFIG *.m | $(TFR) --

fast:
	grep CONFIG *.m | $(TFR) -fast --

arr_test.o: arr_test.m
	gcc -g -c arr_test.m


arr_test: arr_test.o
	gcc -g arr_test.o -o arr_test -L/GC/build/Release -lauto -framework Foundation
