#!/bin/sh

#
# Coverage information needs to be compiled into the binary for data to be
# generated, this is achieved with these two flags:
# 	-fprofile-instr-generate -fcoverage-mapping
#
# Some helpful commands to use with the output of this script
#
# Show the per function coverage for dns.c 
# 	xcrun llvm-cov report build/Release/libresolv.9.dylib -arch arm64e -instr-profile=testcoverage.profdata -show-functions dns.c
# coverage
#	xcodebuild -project libresolv.xcodeproj -target tests build "OTHER_CFLAGS=-fprofile-instr-generate -fcoverage-mapping" "OTHER_LDFLAGS=-fprofile-instr-generate -fcoverage-mapping" "OTHER_TAPI_FLAGS=-fprofile-instr-generate"

testprog=./build/Release/tests
dyld_path=$(pwd)/build/Release 

export DYLD_LIBRARY_PATH=$dyld_path

tests=$($testprog -l | grep "ident:" | awk '{print $2}')

#echo "WARNING: overriding tests for development"
#tests="test_ns_get16 test_ns_get32 test_ns_put16 test_ns_put32"

echo "found the following tests:\n$tests"

coveragefiles=""
for test in $tests
do
	echo "running test $test"
	LLVM_PROFILE_FILE="${test}.profraw" $testprog $test
	coveragefiles="${test}.profraw ${coveragefiles}"
done

xcrun llvm-profdata merge -sparse $coveragefiles -o testcoverage.profdata

#xcrun llvm-cov show $testprog -arch arm64e -instr-profile=testcoverage.profdata
xcrun llvm-cov report build/Release/libresolv.9.dylib -arch arm64e -instr-profile=testcoverage.profdata 
progress=$(xcrun llvm-cov report build/Release/libresolv.9.dylib -arch arm64e -instr-profile=testcoverage.profdata  | grep ^TOTAL)
echo $(date +%Y%M%d%H%M) $progress >> progress.out
echo "Date               Regions    cov   Funcs   cov     Execd lines cov   branches   cov"
cat progress.out | tail -n 5

