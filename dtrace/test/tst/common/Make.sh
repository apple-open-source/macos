gcc -arch i386 ./io/tst.fds.c -o a.out; mv a.out ./io/tst.fds.exe
gcc -arch i386 ./pid/tst.args1.c -o a.out; mv a.out ./pid/tst.args1.exe
gcc -arch i386 ./pid/tst.float.c -o a.out; mv a.out ./pid/tst.float.exe
gcc -arch i386 ./pid/tst.fork.c -o a.out; mv a.out ./pid/tst.fork.exe
gcc -arch i386 ./pid/tst.gcc.c -o a.out; mv a.out ./pid/tst.gcc.exe
gcc -arch i386 ./pid/tst.ret1.c -o a.out; mv a.out ./pid/tst.ret1.exe
gcc -arch i386 ./pid/tst.ret2.c -o a.out; mv a.out ./pid/tst.ret2.exe
gcc -arch i386 ./pid/tst.vfork.c -o a.out; mv a.out ./pid/tst.vfork.exe
# gcc -arch i386 ./pid/tst.weak1.c -o a.out; mv a.out ./pid/tst.weak1.exe
# gcc -arch i386 ./pid/tst.weak2.c -o a.out; mv a.out ./pid/tst.weak2.exe
# gcc -arch i386 ./proc/tst.sigwait.c -o a.out; mv a.out ./proc/tst.sigwait.exe
gcc -arch i386 ./raise/tst.raise1.c -o a.out; mv a.out ./raise/tst.raise1.exe
gcc -arch i386 ./raise/tst.raise2.c -o a.out; mv a.out ./raise/tst.raise2.exe
gcc -arch i386 ./raise/tst.raise3.c -o a.out; mv a.out ./raise/tst.raise3.exe
gcc -arch i386 ./stop/tst.stop1.c -o a.out; mv a.out ./stop/tst.stop1.exe
gcc -arch i386 ./stop/tst.stop2.c -o a.out; mv a.out ./stop/tst.stop2.exe
# gcc -arch i386 ./usdt/tst.argmap.c -o a.out; mv a.out ./usdt/tst.argmap.exe
# gcc -arch i386 ./usdt/tst.args.c -o a.out; mv a.out ./usdt/tst.args.exe
gcc -arch i386 ./ustack/tst.spin.c -o a.out; mv a.out ./ustack/tst.spin.exe
gcc -arch i386 ../i386/pid/tst.nop.c ../i386/pid/tst.nop.s -o a.out; mv a.out ../i386/pid/tst.nop.exe
gcc -arch i386 ./pid/tst.dlopen.c -o ./pid/tst.dlopen.exe

