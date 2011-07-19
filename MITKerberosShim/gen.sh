

perl gen.pl l > heim-loader.c
perl gen.pl h > heim-sym.h
perl gen.pl 6 > heim-sym.x86_64.s
perl gen.pl 3 > heim-sym.i386.s

perl gen.pl 6p > heim-proxy.x86_64.s
perl gen.pl 3p > heim-proxy.i386.s

: git commit -m 'regen' \
    heim-loader.c heim-sym.h \
    heim-sym.i386.s heim-sym.x86_64.s heim-sym.ppc.s \
    heim-proxy.x86_64.s heim-proxy.i386.s heim-proxy.ppc.s \
    gen.pl gen.sh
