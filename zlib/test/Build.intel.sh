#!/bin/ksh

export CFLAGS='-Os -pipe -fPIC -DUSE_MMAP -dead_strip -DVEC_OPTIMIZE'

gcc -arch i386 -arch x86_64 $CFLAGS -c -o adler32.o ../zlib/adler32.c
gcc -arch i386 -arch x86_64 $CFLAGS -c -o compress.o ../zlib/compress.c
gcc -arch i386 -arch x86_64 $CFLAGS -c -o crc32.o ../zlib/crc32.c
gcc -arch i386 -arch x86_64 $CFLAGS -c -o deflate.o ../zlib/deflate.c
gcc -arch i386 -arch x86_64 $CFLAGS -c -o gzclose.o ../zlib/gzclose.c
gcc -arch i386 -arch x86_64 $CFLAGS -c -o gzlib.o ../zlib/gzlib.c
gcc -arch i386 -arch x86_64 $CFLAGS -c -o gzread.o ../zlib/gzread.c
gcc -arch i386 -arch x86_64 $CFLAGS -c -o gzwrite.o ../zlib/gzwrite.c
gcc -arch i386 -arch x86_64 $CFLAGS -c -o infback.o ../zlib/infback.c
# gcc -arch i386 -arch x86_64 $CFLAGS -c -o inffast.o ../zlib/inffast.c
gcc -arch i386 -arch x86_64 $CFLAGS -c -o inflate.o ../zlib/inflate.c
gcc -arch i386 -arch x86_64 $CFLAGS -c -o inftrees.o ../zlib/inftrees.c
gcc -arch i386 -arch x86_64 $CFLAGS -c -o trees.o ../zlib/trees.c
gcc -arch i386 -arch x86_64 $CFLAGS -c -o uncompr.o ../zlib/uncompr.c
gcc -arch i386 -arch x86_64 $CFLAGS -c -o zutil.o ../zlib/zutil.c

gcc -arch i386 -arch x86_64 $CFLAGS -c -o adler32vec.o ../zlib/intel/adler32vec.s
gcc -arch i386 -arch x86_64 $CFLAGS -c -o inffast.o ../zlib/intel/inffast.s

gcc -Wmost -pedantic -MMD  -O3 -g -std=c99   -arch x86_64 -msse3  -c -o ClockServices.64.o ClockServices.c
gcc -Wmost -pedantic -MMD  -O3 -g -std=c99   -arch i386 -msse3  -c -o ClockServices.32.o ClockServices.c

lipo -create -o ClockServices.o ClockServices.64.o ClockServices.32.o 

rm ClockServices.64.o ClockServices.32.o ClockServices.64.d ClockServices.32.d

gcc -arch i386 -arch x86_64 $CFLAGS compress.c  *.o -o compress
gcc -arch i386 -arch x86_64 $CFLAGS decompress.c *.o -o decompress 

rm -f *.o




