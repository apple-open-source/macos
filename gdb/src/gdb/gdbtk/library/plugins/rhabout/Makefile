TCL_CFLAGS = -I/home/fnasser/DEVO/insight-sourceware/src/tcl/generic
TCL =  -L/home/fnasser/BUILD/insight-sourceware/native/tcl/unix -ltcl8.0

PRE=
POS= ".so"

rhabout: rhabout.c
	gcc -fPIC $(TCL_CFLAGS) -I. -o rhabout.o -c rhabout.c
	gcc -shared -o rhabout$(POS) rhabout.o $(TCL)
