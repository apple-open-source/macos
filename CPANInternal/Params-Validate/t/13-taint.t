#!/usr/bin/perl -T

use strict;
use warnings;

use File::Spec;
use lib File::Spec->catdir( 't', 'lib' );

eval { "$0$^X" && kill 0; 1 };

use PVTests::Standard;
PVTests::Standard::run_tests();
