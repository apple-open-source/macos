ReadMe.txt


This directory contains libm, including implementations of math.h, fenv.h, and
complex.h and their associated routines, as specified by the C standard and by
a standard with many names:  IEEE 1003.1, POSIX, Single UNIX, and UNIX 03.


CVS information:  $Revision: 1.1 $, $Date: 2005/03/09 00:51:38 $.


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


Attempting to build and run the test targets, after building the targets libm
and libmx with environment variable MACOSX_DEPLOYMENT_TARGET set to 10.4,
produces:

	VectorTestLDBL64.
		Runs and writes results to noship.subproj/VectorTestLDBL64.results.

	VectorTestLDBL128.
		Both of the following behaviors have been observed.  I have not
		diagnosed why:
			Reports SIGFPE taken due to invalid operand and then terminates
			with signal 10 (SIGBUS).
			Does not build:  Undefined symbols __xlq{add,div,mul,sub}.

	VectorTest x86.
		Reports SIGFPE taken due to invalid operand and then terminates with
		signal 10 (SIGBUS).

	VectorTestLP64.
		Reports SIGFPE taken due to invalid operand and then terminates with
		signal 10 (SIGBUS).

	LibmTestHarness.
		Does not build:  "FEDIVBYZERO" and "FE_INVALID" undeclared.  Steve
		Peters says:

			Used to be that FE_DIVBYZERO was an *enum* in fenv.h, and
			mistakenly so. But I didn't realize the mistake at the time. I put
			in the nonsense at line 130 and ff. in libm-test.c to work around
			the mistake. Yank it. I did, compiles fine. Runs OK. This is a
			secondary test tool, useful for some coverage of the fenv.h APIs
			and complex.h APIs.

	VectorTestLDBL64++.
		Runs and writes results to noship.subproj/VectorTestLDBL64++.results.

	VectorTestLDBL128++.
		Runs and writes results to noship.subproj/VectorTestLDBL128++.results.


				-- edp (Eric Postpischil), March 8, 2005.
