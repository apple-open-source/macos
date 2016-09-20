# print usage and exit
usage() { echo "Usage: $0 [-a <arch>[=x86_64]] [-s <sdk name>[=macosx]]" 1>&2; exit 1; }

# default configuration
arch="x86_64"
sdk="macosx"

# parse arguments
while getopts ":a:s:" o; do
	case "${o}" in
		a)
			arch=${OPTARG}
			;;
		s)
			sdk=${OPTARG}
			;;
		*)
			usage
			;;
	esac
done

xcrun -sdk $sdk clang -arch $arch ./io/tst.fds.c -o a.out; mv a.out ./io/tst.fds.exe
xcrun -sdk $sdk clang -arch $arch ./pid/tst.args1.c -o a.out; mv a.out ./pid/tst.args1.exe
xcrun -sdk $sdk clang -arch $arch ./pid/tst.float.c -o a.out; mv a.out ./pid/tst.float.exe
xcrun -sdk $sdk clang -arch $arch ./pid/tst.fork.c -o a.out; mv a.out ./pid/tst.fork.exe
xcrun -sdk $sdk clang -arch $arch ./pid/tst.gcc.c -o a.out; mv a.out ./pid/tst.gcc.exe
xcrun -sdk $sdk clang -arch $arch ./pid/tst.ret1.c -o a.out; mv a.out ./pid/tst.ret1.exe
xcrun -sdk $sdk clang -arch $arch ./pid/tst.ret2.c -o a.out; mv a.out ./pid/tst.ret2.exe
xcrun -sdk $sdk clang -arch $arch ./pid/tst.vfork.c -o a.out; mv a.out ./pid/tst.vfork.exe
#xcrun -sdk $sdk clang -arch $arch ./pid/tst.weak1.c -o a.out; mv a.out ./pid/tst.weak1.exe
#xcrun -sdk $sdk clang -arch $arch ./pid/tst.weak2.c -o a.out; mv a.out ./pid/tst.weak2.exe
#xcrun -sdk $sdk clang -arch $arch ./proc/tst.sigwait.c -o a.out; mv a.out ./proc/tst.sigwait.exe
xcrun -sdk $sdk clang -arch $arch ./raise/tst.raise1.c -o a.out; mv a.out ./raise/tst.raise1.exe
xcrun -sdk $sdk clang -arch $arch ./raise/tst.raise2.c -o a.out; mv a.out ./raise/tst.raise2.exe
xcrun -sdk $sdk clang -arch $arch ./raise/tst.raise3.c -o a.out; mv a.out ./raise/tst.raise3.exe
xcrun -sdk $sdk clang -arch $arch ./stop/tst.stop1.c -o a.out; mv a.out ./stop/tst.stop1.exe
xcrun -sdk $sdk clang -arch $arch ./stop/tst.stop2.c -o a.out; mv a.out ./stop/tst.stop2.exe
#xcrun -sdk $sdk clang -arch $arch ./usdt/tst.argmap.c -o a.out; mv a.out ./usdt/tst.argmap.exe
#xcrun -sdk $sdk clang -arch $arch ./usdt/tst.args.c -o a.out; mv a.out ./usdt/tst.args.exe
xcrun -sdk $sdk clang -arch $arch ./ustack/tst.spin.c -o a.out; mv a.out ./ustack/tst.spin.exe
xcrun -sdk $sdk clang -arch $arch ../i386/pid/tst.nop.c ../i386/pid/tst.nop.s -o a.out; mv a.out ../i386/pid/tst.nop.exe
xcrun -sdk $sdk clang -arch $arch ./pid/tst.dlopen.c -o ./pid/tst.dlopen.exe
