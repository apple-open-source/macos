#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
	$ENV{PERL_PARAMS_UTIL_PP} ||= 1;
}

use Test::More tests => 86;
use File::Spec::Functions ':ALL';
BEGIN {
	ok( ! defined &_CLASSISA, '_CLASSISA does not exist' );
	ok( ! defined &_SUBCLASS, '_SUBCLASS does not exist' );
	ok( ! defined &_DRIVER,   '_DRIVER does not exist'   );
	use_ok('Params::Util', qw(_CLASSISA _SUBCLASS _DRIVER));
	ok(   defined &_CLASSISA, '_CLASSISA imported ok'    );
	ok(   defined &_SUBCLASS, '_SUBCLASS imported ok'    );
	ok(   defined &_DRIVER,   '_DRIVER imported ok'      );
}

# Import refaddr to make certain we have it
use Scalar::Util 'refaddr';





#####################################################################
# Preparing

my $A = catfile( 't', 'driver', 'A.pm' );
ok( -f $A, 'A exists' );
my $B = catfile( 't', 'driver', 'My_B.pm' );
ok( -f $B, 'My_B exists' );
my $C = catfile( 't', 'driver', 'C.pm' );
ok( ! -f $C, 'C does not exist' );
my $D = catfile( 't', 'driver', 'D.pm' );
ok( -f $D, 'D does not exist' );
my $E = catfile( 't', 'driver', 'E.pm' );
ok( -f $E, 'E does not exist' );
my $F = catfile( 't', 'driver', 'F.pm' );
ok( -f $F, 'F does not exist' );

unshift @INC, catdir( 't', 'driver' );

	



#####################################################################
# Things that are not file handles

foreach (
	undef, '', ' ', 'foo bar', 1, 0, -1, 1.23,
	[], {}, \'', bless( {}, "foo" )
) {
	is( _CLASSISA($_, 'A'), undef, 'Non-classisa returns undef' );
	is( _SUBCLASS($_, 'A'), undef, 'Non-subclass returns undef' );
	is( _DRIVER($_, 'A'),   undef, 'Non-driver returns undef'   );
}





#####################################################################
# Sample Classes

# classisa should not load classes
is( _CLASSISA('A', 'A'), 'A',   'A: Driver base class is undef' );
is( _CLASSISA('My_B', 'A'), undef, 'B: Good driver returns ok' );
is( _CLASSISA('My_B', 'H'), undef, 'B: Good driver return undef for incorrect base' );
is( _CLASSISA('C', 'A'), undef, 'C: Non-existant driver is undef' );
is( _CLASSISA('D', 'A'), undef, 'D: Broken driver is undef' );
is( _CLASSISA('E', 'A'), undef, 'E: Not a driver returns undef' );
is( _CLASSISA('F', 'A'), undef, 'F: Faked isa returns ok' );

# classisa should not load classes
is( _SUBCLASS('A', 'A'), undef, 'A: Driver base class is undef' );
is( _SUBCLASS('My_B', 'A'), undef, 'B: Good driver returns ok' );
is( _SUBCLASS('My_B', 'H'), undef, 'B: Good driver return undef for incorrect base' );
is( _SUBCLASS('C', 'A'), undef, 'C: Non-existant driver is undef' );
is( _SUBCLASS('D', 'A'), undef, 'D: Broken driver is undef' );
is( _SUBCLASS('E', 'A'), undef, 'E: Not a driver returns undef' );
is( _SUBCLASS('F', 'A'), undef, 'F: Faked isa returns ok' );

# The base class itself is not a driver
is( _DRIVER('A', 'A'), undef, 'A: Driver base class is undef' );
ok( $A::VERSION, 'A: Class is loaded ok' );
is( _DRIVER('My_B', 'A'), 'My_B',   'B: Good driver returns ok' );
is( _DRIVER('My_B', 'H'), undef, 'B: Good driver return undef for incorrect base' );
ok( $My_B::VERSION, 'B: Class is loaded ok' );
is( _DRIVER('C', 'A'), undef, 'C: Non-existant driver is undef' );
is( _DRIVER('D', 'A'), undef, 'D: Broken driver is undef' );
is( _DRIVER('E', 'A'), undef, 'E: Not a driver returns undef' );
is( _DRIVER('F', 'A'), 'F',   'F: Faked isa returns ok' );

# Repeat for classisa
is( _CLASSISA('A', 'A'), 'A',   'A: Driver base class is undef' );
is( _CLASSISA('My_B', 'A'), 'My_B',   'B: Good driver returns ok' );
is( _CLASSISA('My_B', 'H'), undef, 'B: Good driver return undef for incorrect base' );
is( _CLASSISA('C', 'A'), undef, 'C: Non-existant driver is undef' );
is( _CLASSISA('D', 'A'), 'D',   'D: Broken driver is undef' );
is( _CLASSISA('E', 'A'), undef, 'E: Not a driver returns undef' );
is( _CLASSISA('F', 'A'), 'F',   'F: Faked isa returns ok' );

# Repeat for subclasses
is( _SUBCLASS('A', 'A'), undef, 'A: Driver base class is undef' );
is( _SUBCLASS('My_B', 'A'), 'My_B',   'B: Good driver returns ok' );
is( _SUBCLASS('My_B', 'H'), undef, 'B: Good driver return undef for incorrect base' );
is( _SUBCLASS('C', 'A'), undef, 'C: Non-existant driver is undef' );
is( _SUBCLASS('D', 'A'), 'D',   'D: Broken driver is undef' );
is( _SUBCLASS('E', 'A'), undef, 'E: Not a driver returns undef' );
is( _SUBCLASS('F', 'A'), 'F',   'F: Faked isa returns ok' );
