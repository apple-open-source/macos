ROOT=/Volumes/untitled/Users/kdienes/build/gdb/gdb.roots
SRCROOT=/Volumes/untitled/Users/kdienes/source/gdb/macsbug

OBJROOT=${ROOT}/gdb.obj/powerpc-apple-macos10--powerpc-apple-macos10/macsbug

SYMROOT=${ROOT}/gdb.sym

DSTROOT=${ROOT}/gdb.dst

make OBJROOT=$OBJROOT SYMROOT=$SYMROOT DSTROOT=$DSTROOT SRCROOT=${SRCROOT} install
