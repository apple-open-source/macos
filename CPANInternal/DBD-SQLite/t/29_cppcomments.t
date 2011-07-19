#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use Test::More;
use t::lib::Test;
use Fatal qw(open);

my @c_files = (<*.c>, <*.h>, <*.xs>);
plan tests => scalar(@c_files);

FILE:
foreach my $file (@c_files) {
    if ($file =~ /ppport.h/) {
        pass("$file is not ours to be tested");
        next;
    }

    open my $fh, '<', $file;
    my $line = 0;
    while (<$fh>) {
        $line++;
        if (/^(.*)\/\//) {
            my $m = $1;
            if ($m !~ /\*/ && $m !~ /http:$/) { # skip the // in c++ comment in parse.c
                fail("C++ comment in $file line $line");
                next FILE;
            }
        }

        if (/#define\s+DBD_SQLITE_CROAK_DEBUG/) {
            fail("debug macro is enabled in $file line $line");
            next FILE;
        }
    }
    pass("$file has no C++ comments");
    close $fh;
}
