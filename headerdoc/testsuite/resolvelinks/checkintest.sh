#!/bin/sh

TEST=$1

cvs add tests/$TEST.rltest \
	tests/$TEST.expected \
	    tests/$TEST.expected/bogus_requests \
	    tests/$TEST.expected/seed_tests \
	        tests/$TEST.expected/seed_tests/maintest \
	        tests/$TEST.expected/seed_tests/seed1 \
	        tests/$TEST.expected/seed_tests/seed2 \
	            tests/$TEST.expected/seed_tests/seed2/nested

cvs add \
	    tests/$TEST.expected/testout \
	    tests/$TEST.expected/xref_out \
	        tests/$TEST.expected/bogus_requests/*.html \
	            tests/$TEST.expected/seed_tests/maintest/*.html \
	            tests/$TEST.expected/seed_tests/seed1/*.html \
	            tests/$TEST.expected/seed_tests/seed2/nested/*.html

if [ -f tests/$TEST.expected/seed1.xref ] ; then
	cvs add tests/$TEST.expected/seed1.xref
fi
if [ -f tests/$TEST.expected/seed2.xref ] ; then
	cvs add tests/$TEST.expected/seed2.xref
fi

