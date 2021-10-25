# $FreeBSD$

PRINTF=/usr/bin/printf

REGRESSION_START($1)

echo '1..24'

REGRESSION_TEST(`b', `${PRINTF} "abc%b%b" "def\n" "\cghi"')
REGRESSION_TEST(`d', `${PRINTF} "%d,%5d,%.5d,%0*d,%.*d\n" 123 123 123 5 123 5 123')
REGRESSION_TEST(`f', `${PRINTF} "%f,%-8.3f,%f,%f\n" +42.25 -42.25 inf nan')
REGRESSION_TEST(`l1', `LC_ALL=en_US.ISO8859-1 ${PRINTF} "%d\n" $(${PRINTF} \"\\344)')
REGRESSION_TEST(`l2', `LC_ALL=en_US.UTF-8 ${PRINTF} "%d\n" $(${PRINTF} \"\\303\\244)')
REGRESSION_TEST(`m1', `${PRINTF} "%c%%%d\0\045\n" abc \"abc')
REGRESSION_TEST(`m2', `${PRINTF} "abc\n\cdef"')
REGRESSION_TEST(`m3', `${PRINTF} "%%%s\n" abc def ghi jkl')
REGRESSION_TEST(`m4', `${PRINTF} "%d,%f,%c,%s\n"')
REGRESSION_TEST(`m5', `${PRINTF} -- "-d\n"')
REGRESSION_TEST(`s', `${PRINTF} "%.3s,%-5s\n" abcd abc')
REGRESSION_TEST('zero', `${PRINTF} "%u%u\n" 15')
REGRESSION_TEST('zero', `${PRINTF} "%d%d\n" 15')
REGRESSION_TEST('zero', `${PRINTF} "%d%u\n" 15')
REGRESSION_TEST('zero', `${PRINTF} "%u%d\n" 15')
REGRESSION_TEST(`missingpos1', `${PRINTF} "%1\$*s" 1 1 2>&1')
REGRESSION_TEST(`missingpos1', `${PRINTF} "%*1\$s" 1 1 2>&1')
REGRESSION_TEST(`missingpos1', `${PRINTF} "%1\$*.*s" 1 1 1 2>&1')
REGRESSION_TEST(`missingpos1', `${PRINTF} "%*1\$.*s" 1 1 1 2>&1')
REGRESSION_TEST(`missingpos1', `${PRINTF} "%*.*1\$s" 1 1 1 2>&1')
REGRESSION_TEST(`missingpos1', `${PRINTF} "%1\$*2\$.*s" 1 1 1 2>&1')
REGRESSION_TEST(`missingpos1', `${PRINTF} "%*1\$.*2\$s" 1 1 1 2>&1')
REGRESSION_TEST(`missingpos1', `${PRINTF} "%1\$*.*2\$s" 1 1 1 2>&1')
REGRESSION_TEST(`bwidth', `${PRINTF} "%8.2b" "a\nb\n"')

REGRESSION_END()
