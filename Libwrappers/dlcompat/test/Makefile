all:		dlone.so dltwo.so dllib.dylib dllib.a dltest dladdr dlsymtest

dlone.so:	dlmodule.c
		cc -bundle -o dlone.so dlmodule.c -DDL_MODULE_NUMBER=\"one\"

dltwo.so:	dlmodule.c
		cc -bundle -o dltwo.so dlmodule.c -DDL_MODULE_NUMBER=\"two\"

dlmodule.o:	dlmodule.c
		cc -c -o dlmodule.o dlmodule.c -fno-common -DPIC -DDL_MODULE_NUMBER=\"lib\"

dllib.dylib:	dlmodule.o
		cc -dynamiclib -o dllib.dylib dlmodule.o

dllib.a:	dlmodule.o
		ar cru dllib.a dlmodule.o
		ranlib dllib.a

dltest:		dltest.c
		cc -o dltest dltest.c -I.. -L.. -ldl
dladdr:		dladdr.c
		cc -o dladdr dladdr.c -I.. -L.. -ldl
		
dlsymtest: dlsymtest.c libfoo.dylib
		gcc -o dlsymtest  dlsymtest.c -I.. -L.. -L. -ldl -lfoo
        			
libfoo.o: libfoo.c
		gcc -c -o libfoo.o libfoo.c
  
libfoo.dylib: libfoo.o
		gcc -dynamiclib -o libfoo.dylib libfoo.o

clean:		
		rm -f *~ *.o *.so *.dylib *.a dltest dladdr dlsymtest
