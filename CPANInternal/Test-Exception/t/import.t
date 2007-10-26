#! /usr/bin/perl -Tw

use strict;
use warnings;
use Test::More;

BEGIN { use_ok( 'Test::Exception', tests => 2 ) };

is( Test::Builder->new->expected_tests, 2, 'Test::Exception set plan' );