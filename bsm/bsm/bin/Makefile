## Probably only works with GNU make

ifndef CFLAGS
CFLAGS = -g -Wall -pipe -fno-common -I/usr/include $(CCFLAGS) 
endif
BINCFLAGS = $(CFLAGS) -I../lib
BSMLIBPATH = -L../lib -L/usr/lib
GCC=gcc 

OUTFILES = praudit auditreduce

all: ${OUTFILES}

install: ${OUTFILES}
	for bin in ${OUTFILES} ; do \
		install $$bin /usr/sbin ; \
		/usr/bin/strip -S /usr/sbin/$$bin ; \
	done

praudit: praudit.c
	$(GCC) $(BINCFLAGS) $(BSMLIBPATH) -lbsm praudit.c -o praudit 

auditreduce: auditreduce.c
	$(GCC) $(BINCFLAGS) $(BSMLIBPATH) -lbsm auditreduce.c -o auditreduce 

clean: 
	rm -f *.o ${OUTFILES}

