#!/usr/bin/perl
use strict;
use warnings;

if ( not $ENV{TEST_AUTHOR} ) {
    my $msg = 'Author test.  Set $ENV{TEST_AUTHOR} to a true value to run.';
    print "1..0 
# $msg";
    exit 0;
}
require Test::More;
Test::More->import();

if ( not $ENV{TEST_AUTHOR} ) {
    my $msg = 'Author test.  Set $ENV{TEST_AUTHOR} to a true value to run.';
    plan( skip_all => $msg );
}

chdir '..' if -d ('../t');

eval 'use Test::Kwalitee';

if ( $@ ) {
    my $msg = 'Test::Kwalitee not installed; skipping';
    plan( skip_all => $msg );
}
