# $FreeBSD$

REGRESSION_START($1)

echo '1..24'

REGRESSION_TEST(`b', `env printf "abc%b%b" "def\n" "\cghi"')
REGRESSION_TEST(`d', `env printf "%d,%5d,%.5d,%0*d,%.*d\n" 123 123 123 5 123 5 123')
REGRESSION_TEST(`f', `env printf "%f,%-8.3f,%f,%f\n" +42.25 -42.25 inf nan')
REGRESSION_TEST(`l1', `LC_ALL=en_US.ISO8859-1 env printf "%d\n" $(env printf \"\\344)')
REGRESSION_TEST(`l2', `LC_ALL=en_US.UTF-8 env printf "%d\n" $(env printf \"\\303\\244)')
REGRESSION_TEST(`m1', `env printf "%c%%%d\0\045\n" abc \"abc')
REGRESSION_TEST(`m2', `env printf "abc\n\cdef"')
REGRESSION_TEST(`m3', `env printf "%%%s\n" abc def ghi jkl')
REGRESSION_TEST(`m4', `env printf "%d,%f,%c,%s\n"')
REGRESSION_TEST(`m5', `env printf -- "-d\n"')
REGRESSION_TEST(`s', `env printf "%.3s,%-5s\n" abcd abc')
REGRESSION_TEST('zero', `env printf "%u%u\n" 15')
REGRESSION_TEST('zero', `env printf "%d%d\n" 15')
REGRESSION_TEST('zero', `env printf "%d%u\n" 15')
REGRESSION_TEST('zero', `env printf "%u%d\n" 15')
REGRESSION_TEST(`missingpos1', `env printf "%1\$*s" 1 1 2>&1')
REGRESSION_TEST(`missingpos1', `env printf "%*1\$s" 1 1 2>&1')
REGRESSION_TEST(`missingpos1', `env printf "%1\$*.*s" 1 1 1 2>&1')
REGRESSION_TEST(`missingpos1', `env printf "%*1\$.*s" 1 1 1 2>&1')
REGRESSION_TEST(`missingpos1', `env printf "%*.*1\$s" 1 1 1 2>&1')
REGRESSION_TEST(`missingpos1', `env printf "%1\$*2\$.*s" 1 1 1 2>&1')
REGRESSION_TEST(`missingpos1', `env printf "%*1\$.*2\$s" 1 1 1 2>&1')
REGRESSION_TEST(`missingpos1', `env printf "%1\$*.*2\$s" 1 1 1 2>&1')
REGRESSION_TEST(`bwidth', `env printf "%8.2b" "a\nb\n"')

REGRESSION_END()
