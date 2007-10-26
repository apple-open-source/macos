#! /usr/bin/perl -Tw

use strict;
use warnings;
use Test::More tests => 2;
BEGIN { use_ok( 'Test::Exception' ) };

eval { throws_ok {} undef };
like( $@, '/^throws_ok/', 'cannot pass undef to throws_ok' );