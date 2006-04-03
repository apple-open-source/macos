ReadMe.txt


This directory contains results saved from running tests with Libm-90 under
Tiger 8A410 on a G5:

	I set the environment variable MACOSX_DEPLOYMENT_TARGET to 10.4.

	I used xcodebuild to build targets libm and libmx.
	(libmx is probably not needed and not tested by these programs.)

	I attempted to build each of the test targets below and run their
	executables, and I saved the results here.


CVS information:  $Revision: 1.1 $, $Date: 2005/04/14 19:35:19 $.


Test Programs.

	The libm target was built, and then an attempt was made to build and run
	each of the targets below.  The builds occurred in the main Libm directory,
	but the test programs appear to expect to be run from the build
	subdirectory.

	VectorTestLDBL64.

		Executable name is VectorTestLDBL64.
		Output file name is ../noship.subproj/VectorTestLDBL64.results.
		PowerPC:
			Runs to completion.
		IA-32:
			Not tried.

	VectorTestLDBL128.
		Executable name is VectorTest.
		Output file name is ../noship.subproj/VectorTestLDBL128.results.
		PowerPC:
			Reports SIGFPE taken due to invalid operand and then terminates
			with signal 10 (SIGBUS).
			Does not build:  Undefined symbols __xlq{add,div,mul,sub}.
		IA-32:
			Not tried.

	VectorTest x86.
		Executable name is VectorTest.
		Output file name is ../noship.subproj/VectorTextx86.results.
		PowerPC:
			Fails to build.  (Obviously, this does not build for a PowerPC, but
			it also does not build on a PowerPC.  The target appears not to
			contain explicit indication of its target that would build it for
			i386 on a PowerPC.)
		IA-32:
			Not tried.

	VectorTestLP64.
		Executable name is VectorTestLP64.
		Output file name would be ../noship.subproj/VectorTextLP64.results?
		PowerPC:
			Reports SIGFPE taken due to invalid operand and then terminates
			with signal 10 (SIGBUS).
		IA-32:
			Not tried.

	LibmTestHarness.
		Executable name is LibmTestHarness.
		Writes results to standard output, redirected to LibmTestHarness.stdout.
		PowerPC:
			Does not build:  "FE_DIVBYZERO" and "FE_INVALID" undeclared.  Steve
			Peters says:

				Used to be that FE_DIVBYZERO was an *enum* in fenv.h, and
				mistakenly so. But I didn't realize the mistake at the time. I
				put in the nonsense at line 130 and ff. in libm-test.c to work
				around the mistake. Yank it. I did, compiles fine. Runs OK.
				This is a secondary test tool, useful for some coverage of the
				fenv.h APIs and complex.h APIs.
		IA-32:
			Not tried.

	VectorTestLDBL64++.
		Executable name is VectorTestLDBL64++.
		Output file name is ../noship.subproj/VectorTestLDBL64++.results.
		PowerPC:
			Runs to completion.
		IA-32:
			Not tried.

	VectorTestLDBL128++.
		Executable name is VectorTestLDBL128++.
		Output file name is ../noship.subproj/VectorTestLDBL128++.results.
		PowerPC:
			Runs to completion.
		IA-32:
			Not tried.
