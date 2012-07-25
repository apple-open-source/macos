#!/bin/ksh

export CFLAGS='-std=gnu99 -Os -g -pipe -fPIC -DUSE_MMAP -DVEC_OPTIMIZE'
export CCC='xcrun -sdk iphoneos.internal -run gcc -isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS5.0.Internal.sdk'

xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -arch armv7 $CFLAGS  -o decompress decompress.c ../zlib/adler32.c ../zlib/crc32.c ../zlib/gzclose.c ../zlib/gzlib.c ../zlib/gzread.c ../zlib/gzwrite.c ../zlib/deflate.c ../zlib/compress.c ../zlib/infback.c ../zlib/inflate.c ../zlib/inftrees.c ../zlib/trees.c ../zlib/uncompr.c ../zlib/zutil.c ../zlib/arm/inffast.s ../zlib/arm/adler32vec.s ClockServices.c

xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -arch armv6 -arch armv7 $CFLAGS -c -o adler32.o ../zlib/adler32.c
xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -arch armv6 -arch armv7 $CFLAGS -c -o compress.o ../zlib/compress.c
xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -arch armv6 -arch armv7 $CFLAGS -c -o crc32.o ../zlib/crc32.c
xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -arch armv6 -arch armv7 $CFLAGS -c -o deflate.o ../zlib/deflate.c
xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -arch armv6 -arch armv7 $CFLAGS -c -o gzclose.o ../zlib/gzclose.c
xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -arch armv6 -arch armv7 $CFLAGS -c -o gzlib.o ../zlib/gzlib.c
xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -arch armv6 -arch armv7 $CFLAGS -c -o gzread.o ../zlib/gzread.c
xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -arch armv6 -arch armv7 $CFLAGS -c -o gzwrite.o ../zlib/gzwrite.c
xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -arch armv6 -arch armv7 $CFLAGS -c -o infback.o ../zlib/infback.c
xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -arch armv6 -arch armv7 $CFLAGS -c -o inflate.o ../zlib/inflate.c
xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -arch armv6 -arch armv7 $CFLAGS -c -o inftrees.o ../zlib/inftrees.c
xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -arch armv6 -arch armv7 $CFLAGS -c -o trees.o ../zlib/trees.c
xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -arch armv6 -arch armv7 $CFLAGS -c -o uncompr.o ../zlib/uncompr.c
xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -arch armv6 -arch armv7 $CFLAGS -c -o zutil.o ../zlib/zutil.c

# xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -arch armv6 -arch armv7 $CFLAGS -c -o inflateS.o ../zlib/arm/inflateS.s
xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -arch armv6 -arch armv7 $CFLAGS -c -o adler32vec.o ../zlib/arm/adler32vec.s
xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -arch armv6 -arch armv7 $CFLAGS -c -o inffast.o ../zlib/arm/inffast.s

xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -Wmost -pedantic -MMD  -O3 -g -std=c99 -mfpu=neon  -arch armv7 -c -o ClockServices.64.o ClockServices.c
xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -Wmost -pedantic -MMD  -O3 -g -std=c99   -arch armv6 -c -o ClockServices.32.o ClockServices.c

lipo -create -o ClockServices.o ClockServices.64.o ClockServices.32.o 

rm ClockServices.64.o ClockServices.32.o ClockServices.64.d ClockServices.32.d

xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -arch armv6 -arch armv7 $CFLAGS compress.c  *.o -o compress
# xcrun -sdk iphoneos.internal -run gcc -isysroot $(xcodebuild -version -sdk iphoneos.internal Path) -arch armv6 -arch armv7 $CFLAGS -g decompress.c *.o -o decompress 

rm -f *.o




