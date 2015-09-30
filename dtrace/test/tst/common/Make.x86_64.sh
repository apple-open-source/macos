xcrun clang -arch x86_64 ./io/tst.fds.c -o a.out; mv a.out ./io/tst.fds.exe
xcrun clang -arch x86_64 ./pid/tst.args1.c -o a.out; mv a.out ./pid/tst.args1.exe
xcrun clang -arch x86_64 ./pid/tst.float.c -o a.out; mv a.out ./pid/tst.float.exe
xcrun clang -arch x86_64 ./pid/tst.fork.c -o a.out; mv a.out ./pid/tst.fork.exe
xcrun clang -arch x86_64 ./pid/tst.gcc.c -o a.out; mv a.out ./pid/tst.gcc.exe
xcrun clang -arch x86_64 ./pid/tst.ret1.c -o a.out; mv a.out ./pid/tst.ret1.exe
xcrun clang -arch x86_64 ./pid/tst.ret2.c -o a.out; mv a.out ./pid/tst.ret2.exe
xcrun clang -arch x86_64 ./pid/tst.vfork.c -o a.out; mv a.out ./pid/tst.vfork.exe
# xcrun clang -arch x86_64 ./pid/tst.weak1.c -o a.out; mv a.out ./pid/tst.weak1.exe
# xcrun clang -arch x86_64 ./pid/tst.weak2.c -o a.out; mv a.out ./pid/tst.weak2.exe
# xcrun clang -arch x86_64 ./proc/tst.sigwait.c -o a.out; mv a.out ./proc/tst.sigwait.exe
xcrun clang -arch x86_64 ./raise/tst.raise1.c -o a.out; mv a.out ./raise/tst.raise1.exe
xcrun clang -arch x86_64 ./raise/tst.raise2.c -o a.out; mv a.out ./raise/tst.raise2.exe
xcrun clang -arch x86_64 ./raise/tst.raise3.c -o a.out; mv a.out ./raise/tst.raise3.exe
xcrun clang -arch x86_64 ./stop/tst.stop1.c -o a.out; mv a.out ./stop/tst.stop1.exe
xcrun clang -arch x86_64 ./stop/tst.stop2.c -o a.out; mv a.out ./stop/tst.stop2.exe
# xcrun clang -arch x86_64 ./usdt/tst.argmap.c -o a.out; mv a.out ./usdt/tst.argmap.exe
# xcrun clang -arch x86_64 ./usdt/tst.args.c -o a.out; mv a.out ./usdt/tst.args.exe
xcrun clang -arch x86_64 ./ustack/tst.spin.c -o a.out; mv a.out ./ustack/tst.spin.exe
xcrun clang -arch x86_64 ../i386/pid/tst.nop.c ../i386/pid/tst.nop.s -o a.out; mv a.out ../i386/pid/tst.nop.exe
xcrun clang -arch x86_64 ./pid/tst.dlopen.c -o ./pid/tst.dlopen.exe
xcrun clang -arch x86_64 ./types/tst.userlandkey.c -o ./types/tst.userlandkey.exe
