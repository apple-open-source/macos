#!/usr/bin/perl -Tw

# Make sure caller() is undisturbed.

use strict;
use warnings;

use Test::Exception;
use Test::More tests => 2;

eval { die caller() . "\n" };
is( $@, "main\n" );

throws_ok { die caller() . "\n" }  qr/^main$/;
