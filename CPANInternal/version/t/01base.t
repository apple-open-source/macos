#! /usr/local/perl -w
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

#########################

use Test::More qw/no_plan/;
require "t/coretests.pm";

diag "Tests with base class" unless $ENV{PERL_CORE};

BEGIN {
    use_ok("version", 0.49); # If we made it this far, we are ok.
}

BaseTests("version");
