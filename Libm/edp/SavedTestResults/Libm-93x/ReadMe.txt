ReadMe.txt


This directory contains results saved from running tests with Libm-93 with
modifications for my current work.  While it would be preferable to have
results from a clean version of Libm, I have made a cursory review of the
differences between these results and those I did previously from a version
of Libm near Libm-90, and I believe these results can be used as a basis for
comparison with future results.  Any differences that turn up should be
investigated to understand why they occur.  (And, of course, the test programs
should be improved not to report false positives.  Or to run without failing
due to bus errors or segmentation faults.  Or to build without compiler
errors.)

The operating systems used were Tiger 8A410 and Karma 8V6.


CVS information:  $Revision: 1.2 $, $Date: 2005/04/15 18:05:00 $.


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
			Differences from Libm-90x:

				Four new errors in modff because it is run with test cases
				intended only for modf.  The test program appears to be
				ignoring the "d" mode that says the test case is for
				double-precision and not for single-precision.  Actually, the
				saved file reports only three errors in modff, but a later
				run shows four.  Four is expected, matching the four test
				cases at issue.  I am at a loss to explain the difference.
				A later run built from code only slightly changed showed the
				four expected differences.  The total test counts suggest one
				case was not executed -- was something changed in the test
				vector data file?

				New totals for test cases, as new cases were added.
		IA-32:
			Does not build because -mlong-double-64 is not supported.

	VectorTestLDBL128.
		Executable name is VectorTest.
		Output file name is ../noship.subproj/VectorTestLDBL128.results.
		PowerPC:
			Runs to completion.
			Differences from Libm-90x:

				New errors in modff and modfl and new totals as above.

				New differences in output that at first appearance are due to
				test program correctly printing very large finite numbers,
				where as the saved results show "inf".  Perhaps a bug in printf
				was fixed?

		IA-32:
			Does not build because -mlong-double-128 is not supported.

	VectorTest x86.
		Executable name is VectorTest.
		Output file name is ../noship.subproj/VectorTextx86.results.
		PowerPC:
			Fails to build:  "cc1: error: invalid option '96bit-long-double'".
			(Obviously, this does not build for a PowerPC, but it also does not
			build on a PowerPC.  The target appears not to contain explicit
			indication of its target that would build it for i386 on a
			PowerPC.)
		IA-32:
			Builds and gets "Bus error", after printing
			"../noship.subproj/vectors/TVB2D2B.TEXT".  Writes some results.

	VectorTestLP64.
		Executable name is VectorTestLP64.
		Output file name would be ../noship.subproj/VectorTextLP64.results?
		PowerPC:
			Gets Segmentation fault.
		IA-32:
			Builds a PowerPC executable (not executed).

	LibmTestHarness.
		(With libm-test.c revision 1.3.)
		Executable name is LibmTestHarness.
		Writes results to standard output, redirected to LibmTestHarness.stdout.
		PowerPC:
			Reports 1139 test cases plus 953 for exception flags, 122 errors.
		IA-32:
			Reports 1139 test cases plus 953 for exception flags, 62 errors.

	VectorTestLDBL64++.
		Executable name is VectorTestLDBL64++.
		Output file name is ../noship.subproj/VectorTestLDBL64++.results.
		PowerPC:
			Runs to completion.
			Differences from Libm-90x:
				New errors in modff and new totals as above.
		IA-32:
			Does not build because -mlong-double-64 is not supported.

	VectorTestLDBL128++.
		Executable name is VectorTestLDBL128++.
		Output file name is ../noship.subproj/VectorTestLDBL128++.results.
		PowerPC:
			Runs to completion.
			Differences from Libm-90x:
				New errors in modff and modfl and new totals as above.
		IA-32:
			Does not build because -mlong-double-128 is not supported.
