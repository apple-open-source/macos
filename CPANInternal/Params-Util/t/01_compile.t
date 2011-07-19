#!/usr/bin/perl

use 5.00503;
use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
	$ENV{PERL_PARAMS_UTIL_PP} ||= 0;
}

use Test::More tests => 4;
use File::Spec::Functions ':ALL';

# Does the module load
use_ok('Params::Util');

# Double check that Scalar::Util is valid
require_ok( 'Scalar::Util' );
ok( $Scalar::Util::VERSION >= 1.18, 'Scalar::Util version is at least 1.18' );
ok( defined &Scalar::Util::refaddr, 'Scalar::Util has a refaddr implementation' );
