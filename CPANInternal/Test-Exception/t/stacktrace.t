#! /usr/bin/perl -Tw

use strict;
use warnings;
use Sub::Uplevel;
use Carp;
use Test::Builder::Tester tests => 2;
use Test::More;

BEGIN { use_ok( 'Test::Exception' ) };

test_out('not ok 1 - threw /fribble/');
test_fail(+1);
throws_ok { confess('died') } '/fribble/';
my $exception = $@;
test_diag('expecting: /fribble/');
test_diag(split /\n/, "found: $exception");
test_test('regex in stacktrace ignored');
