ReadMe.txt


This directory contains libm, including implementations of math.h, fenv.h, and
complex.h and their associated routines, as specified by the C standard and by
a standard with many names:  IEEE 1003.1, POSIX, Single UNIX, and UNIX 03.


CVS information:  $Revision: 1.2 $, $Date: 2005/04/14 19:35:46 $.


Notes.

	The library cannot conform to UNIX 03 and C at the same time, as UNIX 03
	requires that symbols such as M_PI be defined by math.h and C requires
	that they not be defined by math.h.

	I have observed a couple of problems with building libm in Xcode.

		Building the libm target creates a directory /tmp/LibmV5.dst.  Removing
		the build subdirectory (in this directory) and attempting to build
		libm again results in an error complaining that the directory already
		exists.  This error is produced by jam, executing build instructions
		some file I was not able to track down.

		Xcode appears not to set the environment variable
		MACOSX_DEVELOPMENT_TARGET (to a Mac OS version number, like "10.4").
		This results in source references to routines like printf being
		compiled into references to _printf$LDBLStub, which is not defined.

	I presume libm was built previously with buildit, some other script, or
	perhaps a predecessor of Xcode that did define MACOSX_DEVELOPMENT_TARGET.


Tests.

	See information about test programs and saved results in
	edp/SavedTestResults.
