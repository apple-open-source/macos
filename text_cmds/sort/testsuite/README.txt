To run the tests:

1) Adjust the variable TESTED_SORT in the file run.sh - the value must point
to the binary that is to be tested.

2) Adjust the value ORIG_SORT in the file run.sh - the value must point to the binary that is assumed
to be working correctly. The tested sort binary will be checked against this program.

3) Run:

$ cd <...>/testsuite/
$ ./run.sh

4) Wait for many hours, it is running about 23 hours on my laptop.

5) Check the output and check the existence of the file errors.log in the current directory.
If the test run has been successful, then there must be no file errors.log.

 
