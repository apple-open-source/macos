#!/usr/bin/perl

# Compile testing

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use Test::More tests => 2;

# Load-test Task::Weaken (what the hell)
use_ok( 'Task::Weaken' );

# Load Scalar::Util
use_ok( 'Scalar::Util' );
