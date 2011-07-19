#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
	$ENV{PERL_PARAMS_UTIL_PP} ||= 0;
}

use Test::More tests => 612;
use File::Spec::Functions ':ALL';
use Scalar::Util 'refaddr';
use Params::Util ();

# Utility functions
sub true  { is( shift, 1,     shift || () ) }
sub false { is( shift, '',    shift || () ) }
sub null  { is( shift, undef, shift || () ) }
sub dies  {
	my ($code, $regexp, $message) = @_;
	eval "$code";
	ok( (defined($@) and length($@)), $message );
	if ( defined $regexp ) {
		like( $@, $regexp, '... with expected error message' );
	}
}





#####################################################################
# Tests for _STRING

# Test bad things against the actual function
dies( "Params::Util::_STRING()", qr/Not enough arguments/, '...::_STRING() dies' );
null( Params::Util::_STRING(undef),        '...::_STRING(undef) returns undef' );
null( Params::Util::_STRING(''),           '...::_STRING(nullstring) returns undef' );
null( Params::Util::_STRING({ foo => 1 }), '...::_STRING(HASH) returns undef' );
null( Params::Util::_STRING(sub () { 1 }), '...::_STRING(CODE) returns undef' );
null( Params::Util::_STRING([]),           '...::_STRING(ARRAY) returns undef' );
null( Params::Util::_STRING(\""),          '...::_STRING(null constant) returns undef' );
null( Params::Util::_STRING(\"foo"),       '...::_STRING(SCALAR) returns undef' );

# Test good things against the actual function (carefully)
foreach my $ident ( qw{0 1 foo _foo foo1 __foo_1 Foo::Bar}, ' ', ' foo' ) {
	is( Params::Util::_STRING($ident), $ident, "...::_STRING('$ident') returns ok" );
}

# Import the function
use_ok( 'Params::Util', '_STRING' );
ok( defined *_STRING{CODE}, '_STRING imported ok' );

# Test bad things against the actual function
dies( "_STRING()", qr/Not enough arguments/, '...::_STRING() dies' );
null( _STRING(undef),        '_STRING(undef) returns undef' );
null( _STRING(''),           '_STRING(nullstring) returns undef' );
null( _STRING({ foo => 1 }), '_STRING(HASH) returns undef' );
null( _STRING(sub () { 1 }), '_STRING(CODE) returns undef' );
null( _STRING([]),           '_STRING(ARRAY) returns undef' );
null( _STRING(\""),          '_STRING(null constant) returns undef' );
null( _STRING(\"foo"),       '_STRING(SCALAR) returns undef' );

# Test good things against the actual function (carefully)
foreach my $ident ( qw{0 1 foo _foo foo1 __foo_1 Foo::Bar}, ' ', ' foo' ) {
	is( _STRING($ident), $ident, "...::_STRING('$ident') returns ok" );
}





#####################################################################
# Tests for _IDENTIFIER

# Test bad things against the actual function
dies( "Params::Util::_IDENTIFIER()", qr/Not enough arguments/, '...::_IDENTIFIER() dies' );
null( Params::Util::_IDENTIFIER(undef),        '...::_IDENTIFIER(undef) returns undef' );
null( Params::Util::_IDENTIFIER(''),           '...::_IDENTIFIER(nullstring) returns undef' );
null( Params::Util::_IDENTIFIER(1),            '...::_IDENTIFIER(number) returns undef' );
null( Params::Util::_IDENTIFIER(' foo'),       '...::_IDENTIFIER(string) returns undef' );
null( Params::Util::_IDENTIFIER({ foo => 1 }), '...::_IDENTIFIER(HASH) returns undef' );
null( Params::Util::_IDENTIFIER(sub () { 1 }), '...::_IDENTIFIER(CODE) returns undef' );
null( Params::Util::_IDENTIFIER([]),           '...::_IDENTIFIER(ARRAY) returns undef' );
null( Params::Util::_IDENTIFIER(\""),          '...::_IDENTIFIER(null constant) returns undef' );
null( Params::Util::_IDENTIFIER(\"foo"),       '...::_IDENTIFIER(SCALAR) returns undef' );
null( Params::Util::_IDENTIFIER("Foo::Bar"),   '...::_IDENTIFIER(CLASS) returns undef' );
null( Params::Util::_IDENTIFIER("foo\n"),      '...::_IDENTIFIER(BAD) returns undef' );

# Test good things against the actual function (carefully)
foreach my $ident ( qw{foo _foo foo1 __foo_1} ) {
	is( Params::Util::_IDENTIFIER($ident), $ident, "...::_IDENTIFIER('$ident') returns ok" );
}

# Import the function
use_ok( 'Params::Util', '_IDENTIFIER' );
ok( defined *_IDENTIFIER{CODE}, '_IDENTIFIER imported ok' );

# Test bad things against the actual function
dies( "_IDENTIFIER()", qr/Not enough arguments/, '...::_IDENTIFIER() dies' );
null( _IDENTIFIER(undef),        '_IDENTIFIER(undef) returns undef' );
null( _IDENTIFIER(''),           '_IDENTIFIER(nullstring) returns undef' );
null( _IDENTIFIER(1),            '_IDENTIFIER(number) returns undef' );
null( _IDENTIFIER(' foo'),       '_IDENTIFIER(string) returns undef' );
null( _IDENTIFIER({ foo => 1 }), '_IDENTIFIER(HASH) returns undef' );
null( _IDENTIFIER(sub () { 1 }), '_IDENTIFIER(CODE) returns undef' );
null( _IDENTIFIER([]),           '_IDENTIFIER(ARRAY) returns undef' );
null( _IDENTIFIER(\""),          '_IDENTIFIER(null constant) returns undef' );
null( _IDENTIFIER(\"foo"),       '_IDENTIFIER(SCALAR) returns undef' );
null( _IDENTIFIER("Foo::Bar"),   '_IDENTIFIER(CLASS) returns undef' );
null( _IDENTIFIER("foo\n"),      '_IDENTIFIER(BAD) returns undef' );

# Test good things against the actual function (carefully)
foreach my $ident ( qw{foo _foo foo1 __foo_1} ) {
	is( _IDENTIFIER($ident), $ident, "...::_IDENTIFIER('$ident') returns ok" );
}





#####################################################################
# Tests for _CLASS

# Test bad things against the actual function
dies( "Params::Util::_CLASS()", qr/Not enough arguments/, '...::_CLASS() dies' );
null( Params::Util::_CLASS(undef),        '...::_CLASS(undef) returns undef' );
null( Params::Util::_CLASS(''),           '...::_CLASS(nullstring) returns undef' );
null( Params::Util::_CLASS(1),            '...::_CLASS(number) returns undef' );
null( Params::Util::_CLASS(' foo'),       '...::_CLASS(string) returns undef' );
null( Params::Util::_CLASS({ foo => 1 }), '...::_CLASS(HASH) returns undef' );
null( Params::Util::_CLASS(sub () { 1 }), '...::_CLASS(CODE) returns undef' );
null( Params::Util::_CLASS([]),           '...::_CLASS(ARRAY) returns undef' );
null( Params::Util::_CLASS(\""),          '...::_CLASS(null constant) returns undef' );
null( Params::Util::_CLASS(\"foo"),       '...::_CLASS(SCALAR) returns undef' );
null( Params::Util::_CLASS("D'oh"),       '...::_CLASS(bad class) returns undef' );
null( Params::Util::_CLASS("::Foo"),      '...::_CLASS(bad class) returns undef' );
null( Params::Util::_CLASS("1::X"),       '...::_CLASS(bad class) returns undef' );

# Test good things against the actual function (carefully)
foreach my $ident ( qw{foo _foo foo1 __foo_1 Foo::Bar _Foo::Baaar::Baz X::1} ) {
	is( Params::Util::_CLASS($ident), $ident, "...::_CLASS('$ident') returns ok" );
}

# Import the function
use_ok( 'Params::Util', '_CLASS' );
ok( defined *_CLASS{CODE}, '_CLASS imported ok' );

# Test bad things against the actual function
dies( "_CLASS()", qr/Not enough arguments/, '_CLASS() dies' );
null( _CLASS(undef),        '_CLASS(undef) returns undef' );
null( _CLASS(''),           '_CLASS(nullstring) returns undef' );
null( _CLASS(1),            '_CLASS(number) returns undef' );
null( _CLASS(' foo'),       '_CLASS(string) returns undef' );
null( _CLASS({ foo => 1 }), '_CLASS(HASH) returns undef' );
null( _CLASS(sub () { 1 }), '_CLASS(CODE) returns undef' );
null( _CLASS([]),           '_CLASS(ARRAY) returns undef' );
null( _CLASS(\""),          '_CLASS(null constant) returns undef' );
null( _CLASS(\"foo"),       '_CLASS(SCALAR) returns undef' );
null( _CLASS("D'oh"),       '_CLASS(bad class) returns undef' );
null( _CLASS("::Foo"),      '_CLASS(bad class) returns undef' );
null( _CLASS("1::X"),       '_CLASS(bad class) returns undef' );

# Test good things against the actual function (carefully)
foreach my $ident ( qw{foo _foo foo1 __foo_1 Foo::Bar _Foo::Baaar::Baz X::1} ) {
	is( _CLASS($ident), $ident, "_CLASS('$ident') returns ok" );
}





#####################################################################
# Tests for _NUMBER

# Test bad things against the actual function
dies( "Params::Util::_NUMBER()", qr/Not enough arguments/, '...::_NUMBER() dies' );
null( Params::Util::_NUMBER(undef),        '...::_NUMBER(undef) returns undef' );
null( Params::Util::_NUMBER(''),           '...::_NUMBER(nullstring) returns undef' );
null( Params::Util::_NUMBER(' foo'),       '...::_NUMBER(string) returns undef' );
null( Params::Util::_NUMBER({ foo => 1 }), '...::_NUMBER(HASH) returns undef' );
null( Params::Util::_NUMBER(sub () { 1 }), '...::_NUMBER(CODE) returns undef' );
null( Params::Util::_NUMBER([]),           '...::_NUMBER(ARRAY) returns undef' );
null( Params::Util::_NUMBER(\""),          '...::_NUMBER(null constant) returns undef' );
null( Params::Util::_NUMBER(\"foo"),       '...::_NUMBER(SCALAR) returns undef' );
null( Params::Util::_NUMBER("D'oh"),       '...::_NUMBER(bad class) returns undef' );

# Test good things against the actual function (carefully)
foreach my $id ( qw{1 2 10 123456789 -1 0 +1 02 .1 0.013e-3 1e1} ) {
	is( Params::Util::_NUMBER($id), $id, "...::_NUMBER('$id') returns ok" );
}

# Import the function
use_ok( 'Params::Util', '_NUMBER' );
ok( defined *_NUMBER{CODE}, '_NUMBER imported ok' );

# Test bad things against the actual function
dies( "_NUMBER()", qr/Not enough arguments/, '_NUMBER() dies' );
null( _NUMBER(undef),        '_NUMBER(undef) returns undef' );
null( _NUMBER(''),           '_NUMBER(nullstring) returns undef' );
null( _NUMBER(' foo'),       '_NUMBER(string) returns undef' );
null( _NUMBER({ foo => 1 }), '_NUMBER(HASH) returns undef' );
null( _NUMBER(sub () { 1 }), '_NUMBER(CODE) returns undef' );
null( _NUMBER([]),           '_NUMBER(ARRAY) returns undef' );
null( _NUMBER(\""),          '_NUMBER(null constant) returns undef' );
null( _NUMBER(\"foo"),       '_NUMBER(SCALAR) returns undef' );
null( _NUMBER("D'oh"),       '_NUMBER(bad class) returns undef' );

# Test good things against the actual function (carefully)
foreach my $id ( qw{1 2 10 123456789 -1 0 +1 02 .1 0.013e-3 1e1} ) {
	is( _NUMBER($id), $id, "_NUMBER('$id') returns ok" );
}





#####################################################################
# Tests for _POSINT

# Test bad things against the actual function
dies( "Params::Util::_POSINT()", qr/Not enough arguments/, '...::_POSINT() dies' );
null( Params::Util::_POSINT(undef),        '...::_POSINT(undef) returns undef' );
null( Params::Util::_POSINT(''),           '...::_POSINT(nullstring) returns undef' );
null( Params::Util::_POSINT(' foo'),       '...::_POSINT(string) returns undef' );
null( Params::Util::_POSINT({ foo => 1 }), '...::_POSINT(HASH) returns undef' );
null( Params::Util::_POSINT(sub () { 1 }), '...::_POSINT(CODE) returns undef' );
null( Params::Util::_POSINT([]),           '...::_POSINT(ARRAY) returns undef' );
null( Params::Util::_POSINT(\""),          '...::_POSINT(null constant) returns undef' );
null( Params::Util::_POSINT(\"foo"),       '...::_POSINT(SCALAR) returns undef' );
null( Params::Util::_POSINT("D'oh"),       '...::_POSINT(bad class) returns undef' );
null( Params::Util::_POSINT(-1),           '...::_POSINT(negative) returns undef' );
null( Params::Util::_POSINT(0),            '...::_POSINT(zero) returns undef' );
null( Params::Util::_POSINT("+1"),           '...::_POSINT(explicit positive) returns undef' );
null( Params::Util::_POSINT("02"),         '...::_POSINT(zero lead) returns undef' );

# Test good things against the actual function (carefully)
foreach my $id ( qw{1 2 10 123456789} ) {
	is( Params::Util::_POSINT($id), $id, "...::_POSINT('$id') returns ok" );
}

# Import the function
use_ok( 'Params::Util', '_POSINT' );
ok( defined *_POSINT{CODE}, '_POSINT imported ok' );

# Test bad things against the actual function
dies( "_POSINT()", qr/Not enough arguments/, '_POSINT() dies' );
null( _POSINT(undef),        '_POSINT(undef) returns undef' );
null( _POSINT(''),           '_POSINT(nullstring) returns undef' );
null( _POSINT(' foo'),       '_POSINT(string) returns undef' );
null( _POSINT({ foo => 1 }), '_POSINT(HASH) returns undef' );
null( _POSINT(sub () { 1 }), '_POSINT(CODE) returns undef' );
null( _POSINT([]),           '_POSINT(ARRAY) returns undef' );
null( _POSINT(\""),          '_POSINT(null constant) returns undef' );
null( _POSINT(\"foo"),       '_POSINT(SCALAR) returns undef' );
null( _POSINT("D'oh"),       '_POSINT(bad class) returns undef' );
null( _POSINT(-1),           '_POSINT(negative) returns undef' );
null( _POSINT(0),            '_POSINT(zero) returns undef' );
null( _POSINT("+1"),           '_POSINT(explicit positive) returns undef' );
null( _POSINT("02"),         '_POSINT(zero lead) returns undef' );

# Test good things against the actual function (carefully)
foreach my $id ( qw{1 2 10 123456789} ) {
	is( _POSINT($id), $id, "_POSINT('$id') returns ok" );
}





#####################################################################
# Tests for _NONNEGINT

# Test bad things against the actual function
dies( "Params::Util::_NONNEGINT()", qr/Not enough arguments/, '...::_NONNEGINT() dies' );
null( Params::Util::_NONNEGINT(undef),        '...::_NONNEGINT(undef) returns undef' );
null( Params::Util::_NONNEGINT(''),           '...::_NONNEGINT(nullstring) returns undef' );
null( Params::Util::_NONNEGINT(' foo'),       '...::_NONNEGINT(string) returns undef' );
null( Params::Util::_NONNEGINT({ foo => 1 }), '...::_NONNEGINT(HASH) returns undef' );
null( Params::Util::_NONNEGINT(sub () { 1 }), '...::_NONNEGINT(CODE) returns undef' );
null( Params::Util::_NONNEGINT([]),           '...::_NONNEGINT(ARRAY) returns undef' );
null( Params::Util::_NONNEGINT(\""),          '...::_NONNEGINT(null constant) returns undef' );
null( Params::Util::_NONNEGINT(\"foo"),       '...::_NONNEGINT(SCALAR) returns undef' );
null( Params::Util::_NONNEGINT("D'oh"),       '...::_NONNEGINT(bad class) returns undef' );
null( Params::Util::_NONNEGINT(-1),           '...::_NONNEGINT(negative) returns undef' );
null( Params::Util::_NONNEGINT("+1"),         '...::_NONNEGINT(explicit positive) returns undef' );
null( Params::Util::_NONNEGINT("02"),         '...::_NONNEGINT(zero lead) returns undef' );

# Test good things against the actual function (carefully)
foreach my $id ( qw{0 1 2 10 123456789} ) {
	is( Params::Util::_NONNEGINT($id), $id, "...::_NONNEGINT('$id') returns ok" );
}

# Import the function
use_ok( 'Params::Util', '_NONNEGINT' );
ok( defined *_NONNEGINT{CODE}, '_NONNEGINT imported ok' );

# Test bad things against the actual function
dies( "_NONNEGINT()", qr/Not enough arguments/, '_NONNEGINT() dies' );
null( _NONNEGINT(undef),        '_NONNEGINT(undef) returns undef' );
null( _NONNEGINT(''),           '_NONNEGINT(nullstring) returns undef' );
null( _NONNEGINT(' foo'),       '_NONNEGINT(string) returns undef' );
null( _NONNEGINT({ foo => 1 }), '_NONNEGINT(HASH) returns undef' );
null( _NONNEGINT(sub () { 1 }), '_NONNEGINT(CODE) returns undef' );
null( _NONNEGINT([]),           '_NONNEGINT(ARRAY) returns undef' );
null( _NONNEGINT(\""),          '_NONNEGINT(null constant) returns undef' );
null( _NONNEGINT(\"foo"),       '_NONNEGINT(SCALAR) returns undef' );
null( _NONNEGINT("D'oh"),       '_NONNEGINT(bad class) returns undef' );
null( _NONNEGINT(-1),           '_NONNEGINT(negative) returns undef' );
null( _NONNEGINT("+1"),           '_NONNEGINT(explicit positive) returns undef' );
null( _NONNEGINT("02"),         '_NONNEGINT(zero lead) returns undef' );

# Test good things against the actual function (carefully)
foreach my $id ( qw{0 1 2 10 123456789} ) {
	is( _NONNEGINT($id), $id, "_NONNEGINT('$id') returns ok" );
}





#####################################################################
# Tests for _SCALAR

my $foo    = "foo";
my $scalar = \$foo;

# Test bad things against the actual function
dies( "Params::Util::_SCALAR()", qr/Not enough arguments/, '...::_SCALAR() dies' );
null( Params::Util::_SCALAR(undef),        '...::_SCALAR(undef) returns undef' );
null( Params::Util::_SCALAR(\undef),       '...::_SCALAR(\undef) returns undef' );
null( Params::Util::_SCALAR(''),           '...::_SCALAR(nullstring) returns undef' );
null( Params::Util::_SCALAR(1),            '...::_SCALAR(number) returns undef' );
null( Params::Util::_SCALAR('foo'),        '...::_SCALAR(string) returns undef' );
null( Params::Util::_SCALAR({ foo => 1 }), '...::_SCALAR(HASH) returns undef' );
null( Params::Util::_SCALAR(sub () { 1 }), '...::_SCALAR(CODE) returns undef' );
null( Params::Util::_SCALAR([]),           '...::_SCALAR(ARRAY) returns undef' );
null( Params::Util::_SCALAR(\""),          '...::_SCALAR(null constant) returns undef' );

# Test good things against the actual function (carefully)
is( ref(Params::Util::_SCALAR(\"foo")),  'SCALAR', '...::_SCALAR(constant) returns true' );
is( ref(Params::Util::_SCALAR($scalar)), 'SCALAR', "...::_SCALAR(['foo']) returns true" );
is( refaddr(Params::Util::_SCALAR($scalar)), refaddr($scalar),
    '...::_SCALAR returns the same SCALAR reference');

# Import the function
use_ok( 'Params::Util', '_SCALAR' );
ok( defined *_SCALAR{CODE}, '_SCALAR imported ok' );

# Test bad things against the imported function
dies( "_SCALAR()", qr/Not enough arguments/, '...::_SCALAR() dies' );
null( _SCALAR(undef),        '...::_SCALAR(undef) returns undef' );
null( _SCALAR(\undef),       '...::_SCALAR(\undef) returns undef' );
null( _SCALAR(''),           '...::_SCALAR(nullstring) returns undef' );
null( _SCALAR(1),            '...::_SCALAR(number) returns undef' );
null( _SCALAR('foo'),        '...::_SCALAR(string) returns undef' );
null( _SCALAR({ foo => 1 }), '...::_SCALAR(HASH) returns undef' );
null( _SCALAR(sub () { 1 }), '...::_SCALAR(CODE) returns undef' );
null( _SCALAR([]),           '...::_SCALAR(ARRAY) returns undef' );
null( _SCALAR(\""),          '...::_SCALAR(null constant) returns undef' );

# Test good things against the actual function (carefully)
is( ref(_SCALAR(\"foo")),  'SCALAR', '...::_SCALAR(constant) returns true' );
is( ref(_SCALAR($scalar)), 'SCALAR', "...::_SCALAR(SCALAR) returns true" );
is( refaddr(_SCALAR($scalar)), refaddr($scalar),
    '...::_SCALAR returns the same SCALAR reference');




#####################################################################
# Tests for _SCALAR0

my $null = "";
my $scalar0 = \$null;

# Test bad things against the actual function
dies( "Params::Util::_SCALAR0()", qr/Not enough arguments/, '...::_SCALAR0() dies' );
null( Params::Util::_SCALAR0(undef),        '...::_SCALAR0(undef) returns undef' );
null( Params::Util::_SCALAR0(''),           '...::_SCALAR0(nullstring) returns undef' );
null( Params::Util::_SCALAR0(1),            '...::_SCALAR0(number) returns undef' );
null( Params::Util::_SCALAR0('foo'),        '...::_SCALAR0(string) returns undef' );
null( Params::Util::_SCALAR0({ foo => 1 }), '...::_SCALAR0(HASH) returns undef' );
null( Params::Util::_SCALAR0(sub () { 1 }), '...::_SCALAR0(CODE) returns undef' );
null( Params::Util::_SCALAR0([]),           '...::_SCALAR0(ARRAY) returns undef' );

# Test good things against the actual function (carefully)
is( ref(Params::Util::_SCALAR0(\"foo")),  'SCALAR', '...::_SCALAR0(constant) returns true' );
is( ref(Params::Util::_SCALAR0(\"")),  'SCALAR', '...::_SCALAR0(constant) returns true' );
is( ref(Params::Util::_SCALAR0(\undef)),  'SCALAR', '...::_SCALAR0(\undef) returns true' );
is( ref(Params::Util::_SCALAR0($scalar)), 'SCALAR', "...::_SCALAR0(constant) returns true" );
is( ref(Params::Util::_SCALAR0($scalar0)), 'SCALAR', "...::_SCALAR0(constant) returns true" );
is( refaddr(Params::Util::_SCALAR0($scalar)), refaddr($scalar),
    '...::_SCALAR returns the same SCALAR reference');
is( refaddr(Params::Util::_SCALAR0($scalar0)), refaddr($scalar0),
    '...::_SCALAR returns the same SCALAR reference');

# Import the function
use_ok( 'Params::Util', '_SCALAR0' );
ok( defined *_SCALAR0{CODE}, '_SCALAR0 imported ok' );

# Test bad things against the imported function
dies( "_SCALAR0()", qr/Not enough arguments/, '...::_SCALAR0() dies' );
null( _SCALAR0(undef),        '...::_SCALAR0(undef) returns undef' );
null( _SCALAR0(''),           '...::_SCALAR0(nullstring) returns undef' );
null( _SCALAR0(1),            '...::_SCALAR0(number) returns undef' );
null( _SCALAR0('foo'),        '...::_SCALAR0(string) returns undef' );
null( _SCALAR0({ foo => 1 }), '...::_SCALAR0(HASH) returns undef' );
null( _SCALAR0(sub () { 1 }), '...::_SCALAR0(CODE) returns undef' );
null( _SCALAR0([]),           '...::_SCALAR0(ARRAY) returns undef' );

# Test good things against the actual function (carefully)
is( ref(_SCALAR0(\"foo")),  'SCALAR', '...::_SCALAR0(constant) returns true' );
is( ref(_SCALAR0(\"")),  'SCALAR', '...::_SCALAR0(constant) returns true' );
is( ref(_SCALAR0(\undef)),  'SCALAR', '...::_SCALAR0(\undef) returns true' );
is( ref(_SCALAR0($scalar)), 'SCALAR', "...::_SCALAR0(constant) returns true" );
is( ref(_SCALAR0($scalar0)), 'SCALAR', "...::_SCALAR0(constant) returns true" );
is( refaddr(_SCALAR0($scalar)), refaddr($scalar),
    '...::_SCALAR returns the same SCALAR reference');
is( refaddr(_SCALAR0($scalar0)), refaddr($scalar0),
    '...::_SCALAR returns the same SCALAR reference');





#####################################################################
# Tests for _ARRAY

my $array = [ 'foo', 'bar' ];

# Test bad things against the actual function
dies( "Params::Util::_ARRAY()", qr/Not enough arguments/, '...::_ARRAY() dies' );
null( Params::Util::_ARRAY(undef),        '...::_ARRAY(undef) returns undef' );
null( Params::Util::_ARRAY(''),           '...::_ARRAY(nullstring) returns undef' );
null( Params::Util::_ARRAY(1),            '...::_ARRAY(number) returns undef' );
null( Params::Util::_ARRAY('foo'),        '...::_ARRAY(string) returns undef' );
null( Params::Util::_ARRAY(\'foo'),       '...::_ARRAY(SCALAR) returns undef' );
null( Params::Util::_ARRAY({ foo => 1 }), '...::_ARRAY(HASH) returns undef' );
null( Params::Util::_ARRAY(sub () { 1 }), '...::_ARRAY(CODE) returns undef' );
null( Params::Util::_ARRAY([]),           '...::_ARRAY(empty ARRAY) returns undef' );

# Test good things against the actual function (carefully)
is( ref(Params::Util::_ARRAY([ undef ])), 'ARRAY', '...::_ARRAY([undef]) returns true' );
is( ref(Params::Util::_ARRAY([ 'foo' ])), 'ARRAY', "...::_ARRAY(['foo']) returns true" );
is( ref(Params::Util::_ARRAY($array)), 'ARRAY', '...::_ARRAY returns an ARRAY ok' );
is( refaddr(Params::Util::_ARRAY($array)), refaddr($array),
    '...::_ARRAY($array) returns the same ARRAY reference');

# Import the function
use_ok( 'Params::Util', '_ARRAY' );
ok( defined *_ARRAY{CODE}, '_ARRAY imported ok' );

# Test bad things against the actual function
dies( "_ARRAY();", qr/Not enough arguments/, '_ARRAY() dies' );
null( _ARRAY(undef),        '_ARRAY(undef) returns undef' );
null( _ARRAY(''),           '_ARRAY(nullstring) returns undef' );
null( _ARRAY(1),            '_ARRAY(number) returns undef' );
null( _ARRAY('foo'),        '_ARRAY(string) returns undef' );
null( _ARRAY(\'foo'),       '_ARRAY(SCALAR) returns undef' );
null( _ARRAY({ foo => 1 }), '_ARRAY(HASH) returns undef' );
null( _ARRAY(sub () { 1 }), '_ARRAY(CODE) returns undef' );
null( _ARRAY([]),           '_ARRAY(empty ARRAY) returns undef' );

# Test good things against the actual function (carefully)
is( ref(_ARRAY([ undef ])), 'ARRAY', '_ARRAY([undef]) returns true' );
is( ref(_ARRAY([ 'foo' ])), 'ARRAY', "_ARRAY(['foo']) returns true" );
is( ref(_ARRAY($array)), 'ARRAY', '_ARRAY returns an ARRAY ok' );
is( refaddr(_ARRAY($array)), refaddr($array),
    '_ARRAY($array) returns the same ARRAY reference');





#####################################################################
# Tests for _ARRAY0

# Test bad things against the actual function
dies( "Params::Util::_ARRAY0();", qr/Not enough arguments/, '...::_ARRAY0() dies' );
null( Params::Util::_ARRAY0(undef),        '...::_ARRAY0(undef) returns undef' );
null( Params::Util::_ARRAY0(''),           '...::_ARRAY0(nullstring) returns undef' );
null( Params::Util::_ARRAY0(1),            '...::_ARRAY0(number) returns undef' );
null( Params::Util::_ARRAY0('foo'),        '...::_ARRAY0(string) returns undef' );
null( Params::Util::_ARRAY0(\'foo'),       '...::_ARRAY0(SCALAR) returns undef' );
null( Params::Util::_ARRAY0({ foo => 1 }), '...::_ARRAY0(HASH) returns undef' );
null( Params::Util::_ARRAY0(sub () { 1 }), '...::_ARRAY0(CODE) returns undef' );

# Test good things against the actual function (carefully)
is( ref(Params::Util::_ARRAY0([])),         'ARRAY', '...::_ARRAY0(empty ARRAY) returns undef' );
is( ref(Params::Util::_ARRAY0([ undef ])), 'ARRAY', '...::_ARRAY0([undef]) returns true' );
is( ref(Params::Util::_ARRAY0([ 'foo' ])), 'ARRAY', "...::_ARRAY0(['foo']) returns true" );
is( ref(Params::Util::_ARRAY0($array)), 'ARRAY', '...::_ARRAY0 returns an ARRAY ok' );
is( refaddr(Params::Util::_ARRAY0($array)), refaddr($array),
    '...::_ARRAY0($array) returns the same ARRAY reference');

# Import the function
use_ok( 'Params::Util', '_ARRAY0' );
ok( defined *_ARRAY0{CODE}, '_ARRAY0 imported ok' );

# Test bad things against the actual function
dies( "_ARRAY0();", qr/Not enough arguments/, '_ARRAY0() dies' );
null( _ARRAY0(undef),        '_ARRAY0(undef) returns undef' );
null( _ARRAY0(''),           '_ARRAY0(nullstring) returns undef' );
null( _ARRAY0(1),            '_ARRAY0(number) returns undef' );
null( _ARRAY0('foo'),        '_ARRAY0(string) returns undef' );
null( _ARRAY0(\'foo'),       '_ARRAY0(SCALAR) returns undef' );
null( _ARRAY0({ foo => 1 }), '_ARRAY0(HASH) returns undef' );
null( _ARRAY0(sub () { 1 }), '_ARRAY0(CODE) returns undef' );

# Test good things against the actual function (carefully)
is( ref(_ARRAY0([])),         'ARRAY', '_ARRAY0(empty ARRAY) returns undef' );
is( ref(_ARRAY0([ undef ])), 'ARRAY', '_ARRAY0([undef]) returns true' );
is( ref(_ARRAY0([ 'foo' ])), 'ARRAY', "_ARRAY0(['foo']) returns true" );
is( ref(_ARRAY0($array)), 'ARRAY', '_ARRAY0 returns an ARRAY ok' );
is( refaddr(_ARRAY0($array)), refaddr($array),
    '_ARRAY0($array) returns the same reference');





#####################################################################
# Tests for _HASH

my $hash = { 'foo' => 'bar' };

# Test bad things against the actual function
dies( "Params::Util::_HASH();", qr/Not enough arguments/, '...::_HASH() dies' );
null( Params::Util::_HASH(undef),        '...::_HASH(undef) returns undef' );
null( Params::Util::_HASH(''),           '...::_HASH(nullstring) returns undef' );
null( Params::Util::_HASH(1),            '...::_HASH(number) returns undef' );
null( Params::Util::_HASH('foo'),        '...::_HASH(string) returns undef' );
null( Params::Util::_HASH(\'foo'),       '...::_HASH(SCALAR) returns undef' );
null( Params::Util::_HASH([ 'foo' ]),    '...::_HASH(ARRAY) returns undef' );
null( Params::Util::_HASH(sub () { 1 }), '...::_HASH(CODE) returns undef' );
null( Params::Util::_HASH({}),           '...::_HASH(empty HASH) returns undef' );

# Test good things against the actual function (carefully)
is( ref(Params::Util::_HASH({ foo => 1 })), 'HASH', '...::_HASH([undef]) returns ok' );
is( ref(Params::Util::_HASH($hash)), 'HASH', '...::_HASH returns an HASH ok' );
is(
	refaddr(Params::Util::_HASH($hash)),
	refaddr($hash),
	'...::_HASH($hash) returns the same reference',
);

# Import the function
use_ok( 'Params::Util', '_HASH' );
ok( defined *_HASH{CODE}, '_HASH imported ok' );

# Test bad things against the actual function
dies( "_HASH();", qr/Not enough arguments/, '_HASH() dies' );
null( _HASH(undef),        '_HASH(undef) returns undef' );
null( _HASH(''),           '_HASH(nullstring) returns undef' );
null( _HASH(1),            '_HASH(number) returns undef' );
null( _HASH('foo'),        '_HASH(string) returns undef' );
null( _HASH(\'foo'),       '_HASH(SCALAR) returns undef' );
null( _HASH([]),           '_HASH(ARRAY) returns undef' );
null( _HASH(sub () { 1 }), '_HASH(CODE) returns undef' );
null( _HASH({}),           '...::_HASH(empty HASH) returns undef' );

# Test good things against the actual function (carefully)
is( ref(_HASH({ foo => 1 })), 'HASH', '_HASH([undef]) returns true' );
is( ref(_HASH($hash)), 'HASH', '_HASH returns an ARRAY ok' );
is(
	refaddr(_HASH($hash)),
	refaddr($hash),
	'_HASH($hash) returns the same reference',
);





#####################################################################
# Tests for _HASH0

# Test bad things against the actual function
dies( "Params::Util::_HASH0();", qr/Not enough arguments/, '...::_HASH0() dies' );
null( Params::Util::_HASH0(undef),        '...::_HASH0(undef) returns undef' );
null( Params::Util::_HASH0(''),           '...::_HASH0(nullstring) returns undef' );
null( Params::Util::_HASH0(1),            '...::_HASH0(number) returns undef' );
null( Params::Util::_HASH0('foo'),        '...::_HASH0(string) returns undef' );
null( Params::Util::_HASH0(\'foo'),       '...::_HASH0(SCALAR) returns undef' );
null( Params::Util::_HASH0([ 'foo' ]),    '...::_HASH0(ARRAY) returns undef' );
null( Params::Util::_HASH0(sub () { 1 }), '...::_HASH0(CODE) returns undef' );

# Test good things against the actual function (carefully)
is( ref(Params::Util::_HASH0({})),         'HASH', '...::_HASH0(empty ARRAY) returns undef' );
is( ref(Params::Util::_HASH0({ foo => 1 })), 'HASH', '...::_HASH0([undef]) returns true' );
is( ref(Params::Util::_HASH0($hash)), 'HASH', '...::_HASH0 returns an ARRAY ok' );
is(
	refaddr(Params::Util::_HASH0($hash)),
	refaddr($hash),
	'...::_HASH0($hash) returns the same reference',
);

# Import the function
use_ok( 'Params::Util', '_HASH0' );
ok( defined *_HASH0{CODE}, '_HASH0 imported ok' );

# Test bad things against the actual function
dies( "_HASH0();", qr/Not enough arguments/, '_HASH0() dies' );
null( _HASH0(undef),        '_HASH0(undef) returns undef' );
null( _HASH0(''),           '_HASH0(nullstring) returns undef' );
null( _HASH0(1),            '_HASH0(number) returns undef' );
null( _HASH0('foo'),        '_HASH0(string) returns undef' );
null( _HASH0(\'foo'),       '_HASH0(SCALAR) returns undef' );
null( _HASH0([]),           '_HASH0(ARRAY) returns undef' );
null( _HASH0(sub () { 1 }), '_HASH0(CODE) returns undef' );

# Test good things against the actual function (carefully)
is( ref(_HASH0({})),            'HASH', '_HASH0(empty ARRAY) returns undef' );
is( ref(_HASH0({ foo => 1 })), 'HASH', '_HASH0([undef]) returns true' );
is( ref(_HASH0($hash)), 'HASH', '_HASH0 returns an ARRAY ok' );
is(
	refaddr(_HASH0($hash)),
	refaddr($hash),
	'_HASH0($hash) returns the same reference',
);





#####################################################################
# Tests for _CODE

my $code = sub () { 1 };
sub testcode { 3 };

# Import the function
use_ok( 'Params::Util', '_CODE' );
ok( defined *_CODE{CODE}, '_CODE imported ok' );

# Test bad things against the actual function
dies( "Params::Util::_CODE();", qr/Not enough arguments/, '...::_CODE() dies' );
null( Params::Util::_CODE(undef),        '...::_CODE(undef) returns undef' );
null( Params::Util::_CODE(''),           '...::_CODE(nullstring) returns undef' );
null( Params::Util::_CODE(1),            '...::_CODE(number) returns undef' );
null( Params::Util::_CODE('foo'),        '...::_CODE(string) returns undef' );
null( Params::Util::_CODE(\'foo'),       '...::_CODE(SCALAR) returns undef' );
null( Params::Util::_CODE([ 'foo' ]),    '...::_CODE(ARRAY) returns undef' );
null( Params::Util::_CODE({}),           '...::_CODE(empty HASH) returns undef' );

# Test bad things against the actual function
dies( "_CODE();", qr/Not enough arguments/, '_CODE() dies' );
null( _CODE(undef),        '_CODE(undef) returns undef' );
null( _CODE(''),           '_CODE(nullstring) returns undef' );
null( _CODE(1),            '_CODE(number) returns undef' );
null( _CODE('foo'),        '_CODE(string) returns undef' );
null( _CODE(\'foo'),       '_CODE(SCALAR) returns undef' );
null( _CODE([]),           '_CODE(ARRAY) returns undef' );
null( _CODE({}),           '...::_CODE(empty HASH) returns undef' );

# Test good things against the actual function
is( ref(Params::Util::_CODE(sub { 2 })), 'CODE', '...::_CODE(anon) returns ok'   );
is( ref(Params::Util::_CODE($code)),     'CODE', '...::_CODE(ref) returns ok'    );
is( ref(Params::Util::_CODE(\&testsub)), 'CODE', '...::_CODE(\&func) returns ok' );
is( refaddr(Params::Util::_CODE($code)), refaddr($code),
    '...::_CODE(ref) returns the same reference');
is( refaddr(Params::Util::_CODE(\&testsub)), refaddr(\&testsub),
    '...::_CODE(\&func) returns the same reference');

# Test good things against the imported function
is( ref(_CODE(sub { 2 })), 'CODE', '_CODE(anon) returns ok'   );
is( ref(_CODE($code)),     'CODE', '_CODE(ref) returns ok'    );
is( ref(_CODE(\&testsub)), 'CODE', '_CODE(\&func) returns ok' );
is( refaddr(_CODE($code)), refaddr($code),
    '_CODE(ref) returns the same reference');
is( refaddr(_CODE(\&testsub)), refaddr(\&testsub),
    '_CODE(\&func) returns the same reference');





#####################################################################
# Tests for _INSTANCE

my $s1 = "foo";
my $s2 = "bar";
my $s3 = "baz";
my $scalar1 = \$s1;
my $scalar2 = \$s2;
my $scalar3 = \$s3;
my @objects = (
	bless( {}, 'Foo'),
	bless( [], 'Foo'),
	bless( $scalar1, 'Foo'),
	bless( {}, 'Bar'),
	bless( [], 'Bar'),
	bless( $scalar1, 'Bar'),
	bless( {}, 'Baz'),
	bless( [], 'Baz'),
	bless( $scalar3, 'Baz'),
	);

# Test bad things against the actual function
dies( "Params::Util::_INSTANCE()", qr/Not enough arguments/, '...::_INSTANCE() dies' );
dies( "Params::Util::_INSTANCE(bless {}, 'Foo')", qr/Not enough arguments/, '...::_INSTANCE(object) dies' );
null( Params::Util::_INSTANCE(undef, 'Foo'),        '...::_INSTANCE(undef) returns undef' );
null( Params::Util::_INSTANCE('', 'Foo'),           '...::_INSTANCE(nullstring) returns undef' );
null( Params::Util::_INSTANCE(1, 'Foo'),            '...::_INSTANCE(number) returns undef' );
null( Params::Util::_INSTANCE('foo', 'Foo'),        '...::_INSTANCE(string) returns undef' );
null( Params::Util::_INSTANCE({ foo => 1 }, 'Foo'), '...::_INSTANCE(HASH) returns undef' );
null( Params::Util::_INSTANCE(sub () { 1 }, 'Foo'), '...::_INSTANCE(CODE) returns undef' );
null( Params::Util::_INSTANCE([], 'Foo'),           '...::_INSTANCE(ARRAY) returns undef' );
null( Params::Util::_INSTANCE(\"", 'Foo'),          '...::_INSTANCE(null constant) returns undef' );
null( Params::Util::_INSTANCE(\"foo", 'Foo'),       '...::_INSTANCE(SCALAR) returns undef' );
null( Params::Util::_INSTANCE(bless({},'Bad'), 'Foo'), '...::_INSTANCE(bad object) returns undef' );

# Import the function
use_ok( 'Params::Util', '_INSTANCE' );
ok( defined *_INSTANCE{CODE}, '_INSTANCE imported ok' );

# Test bad things against the actual function
dies( "_INSTANCE()", qr/Not enough arguments/, '_INSTANCE() dies' );
dies( "_INSTANCE(bless {}, 'Foo')", qr/Not enough arguments/, '_INSTANCE(object) dies' );
null( _INSTANCE(undef, 'Foo'),        '_INSTANCE(undef) returns undef' );
null( _INSTANCE('', 'Foo'),           '_INSTANCE(nullstring) returns undef' );
null( _INSTANCE(1, 'Foo'),            '_INSTANCE(number) returns undef' );
null( _INSTANCE('foo', 'Foo'),       '_INSTANCE(string) returns undef' );
null( _INSTANCE({ foo => 1 }, 'Foo'), '_INSTANCE(HASH) returns undef' );
null( _INSTANCE(sub () { 1 }, 'Foo'), '_INSTANCE(CODE) returns undef' );
null( _INSTANCE([], 'Foo'),           '_INSTANCE(ARRAY) returns undef' );
null( _INSTANCE(\"", 'Foo'),          '_INSTANCE(null constant) returns undef' );
null( _INSTANCE(\"foo", 'Foo'),       '_INSTANCE(SCALAR) returns undef' );
null( _INSTANCE(bless({},'Bad'), 'Foo'), '_INSTANCE(bad object) returns undef' );

# Testing good things is a little more complicated in this case,
# so lets do the basic ones first.
foreach my $object ( @objects ) {
	ok( Params::Util::_INSTANCE($object, 'Foo'), '...::_INSTANCE(object, class) returns true when expected' );
	is( refaddr(Params::Util::_INSTANCE($object, 'Foo')), refaddr($object), '...::_INSTANCE(object, class) returns the same object' );
}

# Testing good things is a little more complicated in this case,
# so lets do the basic ones first.
foreach my $object ( @objects ) {
	ok( _INSTANCE($object, 'Foo'), '_INSTANCE(object, class) returns true when expected' );
	is( refaddr(_INSTANCE($object, 'Foo')), refaddr($object), '_INSTANCE(object, class) returns the same object' );
}





#####################################################################
# Tests for _REGEX

# Test bad things against the actual function
dies( "Params::Util::_REGEX();", qr/Not enough arguments/, '...::_REGEX() dies' );
null( Params::Util::_REGEX(undef),        '...::_REGEX(undef)   returns undef' );
null( Params::Util::_REGEX(''),           '...::_REGEX(STRING0) returns undef' );
null( Params::Util::_REGEX(1),            '...::_REGEX(number)  returns undef' );
null( Params::Util::_REGEX('foo'),        '...::_REGEX(string)  returns undef' );
null( Params::Util::_REGEX(\'foo'),       '...::_REGEX(SCALAR)  returns undef' );
null( Params::Util::_REGEX([ 'foo' ]),    '...::_REGEX(ARRAY)   returns undef' );
null( Params::Util::_REGEX(sub () { 1 }), '...::_REGEX(CODE)    returns undef' );
null( Params::Util::_REGEX({}),           '...::_REGEX(HASH0)   returns undef' );
null( Params::Util::_REGEX({ foo => 1 }), '...::_REGEX(HASH)    returns undef' );
ok(   Params::Util::_REGEX(qr//),         '...::_REGEX(qr//) ok' );
ok(   Params::Util::_REGEX(qr/foo/),      '...::_REGEX(qr//) ok' );

# Import the function
use_ok( 'Params::Util', '_REGEX' );
ok( defined *_REGEX{CODE}, '_REGEX imported ok' );

# Test bad things against the actual function
dies( "_REGEX();", qr/Not enough arguments/, '_REGEX() dies' );
null( _REGEX(undef),        '_REGEX(undef)   returns undef' );
null( _REGEX(''),           '_REGEX(STRING0) returns undef' );
null( _REGEX(1),            '_REGEX(number)  returns undef' );
null( _REGEX('foo'),        '_REGEX(string)  returns undef' );
null( _REGEX(\'foo'),       '_REGEX(SCALAR)  returns undef' );
null( _REGEX([]),           '_REGEX(ARRAY)   returns undef' );
null( _REGEX(sub () { 1 }), '_REGEX(CODE)    returns undef' );
null( _REGEX({}),           'REGEX(HASH0)    returns undef' );
null( _REGEX({ foo => 1 }), 'REGEX(HASH)     returns undef' );
ok(   _REGEX(qr//),         '_REGEX(qr//) ok' );
ok(   _REGEX(qr/foo/),      '_REGEX(qr//) ok' );





#####################################################################
# Tests for _SET

my %set = (
  good  => [ map { bless {} => 'Foo' } qw(1..3) ],
  mixed => [ map { bless {} => "Foo$_" } qw(1..3) ],
  unblessed => [ map { {} } qw(1..3) ],
);

# Test bad things against the actual function
dies( "Params::Util::_SET()",   qr/Not enough arguments/, '...::_SET() dies' );
dies( "Params::Util::_SET([])", qr/Not enough arguments/, '...::_SET(single) dies' );
null( Params::Util::_SET(undef, 'Foo'),        '...::_SET(undef) returns undef' );
null( Params::Util::_SET('', 'Foo'),           '...::_SET(nullstring) returns undef' );
null( Params::Util::_SET(1, 'Foo'),            '...::_SET(number) returns undef' );
null( Params::Util::_SET('foo', 'Foo'),        '...::_SET(string) returns undef' );
null( Params::Util::_SET(\'foo', 'Foo'),       '...::_SET(SCALAR) returns undef' );
null( Params::Util::_SET({ foo => 1 }, 'Foo'), '...::_SET(HASH) returns undef' );
null( Params::Util::_SET(sub () { 1 }, 'Foo'), '...::_SET(CODE) returns undef' );
null( Params::Util::_SET([], 'Foo'),           '...::_SET(empty ARRAY) returns undef' );
ok( Params::Util::_SET($set{good}, 'Foo'),     '...::_SET(homogenous ARRAY) returns true' );
null( Params::Util::_SET($set{mixed}, 'Foo'),  '...::_SET(mixed ARRAY) returns undef' );
null( Params::Util::_SET($set{unblessed}, 'Foo'), '...::_SET(unblessed ARRAY) returns undef' );

# Import the function
use_ok( 'Params::Util', '_SET' );
ok( defined *_SET{CODE}, '_SET imported ok' );

# Test bad things against the actual function
dies( "_SET()",   qr/Not enough arguments/, '_SET() dies' );
dies( "_SET([])", qr/Not enough arguments/, '_SET(single) dies' );
null( _SET(undef, 'Foo'),        '_SET(undef) returns undef' );
null( _SET('', 'Foo'),           '_SET(nullstring) returns undef' );
null( _SET(1, 'Foo'),            '_SET(number) returns undef' );
null( _SET('foo', 'Foo'),        '_SET(string) returns undef' );
null( _SET(\'foo', 'Foo'),       '_SET(SCALAR) returns undef' );
null( _SET({ foo => 1 }, 'Foo'), '_SET(HASH) returns undef' );
null( _SET(sub () { 1 }, 'Foo'), '_SET(CODE) returns undef' );
null( _SET([], 'Foo'),           '_SET(empty ARRAY) returns undef' );

ok( _SET($set{good}, 'Foo'),      '_SET(homogenous ARRAY) returns true');
null( _SET($set{mixed}, 'Foo'),     '_SET(mixed ARRAY) returns undef');
null( _SET($set{unblessed}, 'Foo'),     '_SET(unblessed ARRAY) returns undef');




#####################################################################
# Tests for _SET0

# Test bad things against the actual function
dies( "Params::Util::_SET0()",   qr/Not enough arguments/, '...::_SET0() dies' );
dies( "Params::Util::_SET0([])", qr/Not enough arguments/, '...::_SET0(single) dies' );
null( Params::Util::_SET0(undef, 'Foo'),        '...::_SET0(undef) returns undef' );
null( Params::Util::_SET0('', 'Foo'),           '...::_SET0(nullstring) returns undef' );
null( Params::Util::_SET0(1, 'Foo'),            '...::_SET0(number) returns undef' );
null( Params::Util::_SET0('foo', 'Foo'),        '...::_SET0(string) returns undef' );
null( Params::Util::_SET0(\'foo', 'Foo'),       '...::_SET0(SCALAR) returns undef' );
null( Params::Util::_SET0({ foo => 1 }, 'Foo'), '...::_SET0(HASH) returns undef' );
null( Params::Util::_SET0(sub () { 1 }, 'Foo'), '...::_SET0(CODE) returns undef' );
ok( Params::Util::_SET0([], 'Foo'),             '...::_SET0(empty ARRAY) returns true' );
ok( Params::Util::_SET0($set{good}, 'Foo'),      '...::_SET0(homogenous ARRAY) returns true' );
null( Params::Util::_SET0($set{mixed}, 'Foo'),     '...::_SET0(mixed ARRAY) returns undef' );
null( Params::Util::_SET0($set{unblessed}, 'Foo'),     '...::_SET0(unblessed ARRAY) returns undef' );

# Import the function
use_ok( 'Params::Util', '_SET0' );
ok( defined *_SET0{CODE}, '_SET0 imported ok' );

# Test bad things against the actual function
dies( "_SET0()",   qr/Not enough arguments/, '_SET0() dies' );
dies( "_SET0([])", qr/Not enough arguments/, '_SET0(single) dies' );
null( _SET0(undef, 'Foo'),        '_SET0(undef) returns undef' );
null( _SET0('', 'Foo'),           '_SET0(nullstring) returns undef' );
null( _SET0(1, 'Foo'),            '_SET0(number) returns undef' );
null( _SET0('foo', 'Foo'),        '_SET0(string) returns undef' );
null( _SET0(\'foo', 'Foo'),       '_SET0(SCALAR) returns undef' );
null( _SET0({ foo => 1 }, 'Foo'), '_SET0(HASH) returns undef' );
null( _SET0(sub () { 1 }, 'Foo'), '_SET0(CODE) returns undef' );
ok( _SET0([], 'Foo'),             '_SET0(empty ARRAY) returns true' );
ok( _SET0($set{good}, 'Foo'),     '_SET0(homogenous ARRAY) returns true' );
null( _SET0($set{mixed}, 'Foo'),  '_SET0(mixed ARRAY) returns undef' );
null( _SET0($set{unblessed}, 'Foo'),     '_SET0(unblessed ARRAY) returns undef' );





exit(0);

# Base class
package Foo;

sub foo { 1 }

# Normal inheritance
package Bar;

use vars qw{@ISA};
BEGIN {
	@ISA = 'Foo';
}

# Coded isa
package Baz;

sub isa {
	return 1 if $_[1] eq 'Foo';
	shift->SUPER::isa(@_);
}

# Not a subclass
package Bad;

sub bad { 1 }

1;
