ReadMe.txt


This directory contains results saved from running tests with Libm-101.

There are differences between these results and those from Libm-93x:

	printf's handling of long doubles has improved.

	The modff test cases were changed.

	LibmTestHarness on IA-32 reports new errors involving changes in
	exception flags raised and changes in some values returned, up to 33
	ulp (found a test case of the yn function).


Configurations used were:

	PowerPC, Tiger 8C32, gcc 4.0.0 20041026 build 4061.
	IA-32, Karma 8C32, gcc 4.0.0 build 5026.


CVS information:  $Revision: 1.2 $, $Date: 2005/06/29 21:57:29 $.


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

	VectorTestLDBL128.
		Executable name is VectorTest.
		Output file name is ../noship.subproj/VectorTestLDBL128.results.
		PowerPC:
			Runs to completion.

	VectorTest x86.
		Executable name is VectorTest (in build/Default).
		Output file name is ../noship.subproj/VectorTestx86.results.
		IA-32:
			Builds and gets "Bus error", after printing
			"../noship.subproj/vectors/TVB2D2B.TEXT".  Writes some results.

	VectorTestLP64.
		Executable name is VectorTestLP64.
		Output file name would be ../noship.subproj/VectorTestLP64.results?
		PowerPC:
			Runs to completion.

	LibmTestHarness.
		(With libm-test.c revision 1.3.)
		Executable name is LibmTestHarness.
		Writes results to standard output, redirected to LibmTestHarness.stdout.
		PowerPC:
			Reports 1139 test cases plus 953 for exception flags, 122 errors.
		IA-32:
			Reports 1139 test cases plus 953 for exception flags, 143 errors.

	VectorTestLDBL64++.
		Executable name is VectorTestLDBL64++.
		Output file name is ../noship.subproj/VectorTestLDBL64++.results.
		PowerPC:
			Runs to completion.

	VectorTestLDBL128++.
		Executable name is VectorTestLDBL128++.
		Output file name is ../noship.subproj/VectorTestLDBL128++.results.
		PowerPC:
			Runs to completion.
