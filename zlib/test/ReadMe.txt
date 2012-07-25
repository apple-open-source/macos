This directory is added to provide a test suite for verifying the function and profiling the performance of the
built zlib dylib.

To build i386/x86_64 zlib compress/decompress command line tools, run Build.intel.sh

$ Build.intel.sh

this will generate zlib compress/decompress tools that use the same source code used to build zlib.dylib.

To run the tool

$ ./compress input compressed_file

$ ./decompress compressed_file decompressed_file

The decompressed_file should be identical to the input uncompressed file.

$ cmp input decompressed_file

Similar executables for iOS (armv6/armv7) can be built, copied to iOS devices, and executed on the iOS devices.

Note: 	
	it takes a while to do the compression. So, it is better to do the compression on a intel host, 
	and then copied the compressed bit streams to the iOS devices, and verify the decompress tool there.

The current build system does not allow put binary files in the project source. So a binary test suite

test_suite.tar.bz2  

is attached to the origin radar

<rdar://problem/7284394> zlib -- set up test and performance measuring harness for code development.

Simply copy the decompress tool to the directory and run run ./verify.sh and
it will provide the deflate performance, and/or indicate errors in the implementation, 
should there by any implementation bugs.

cclee	3-21-11 
