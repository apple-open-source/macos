#!/usr/bin/perl -w

use strict;
use Test::More tests => 51;
use Test::Exception;

use Time::HiRes::Value;

use Carp ();

dies_ok( sub { Time::HiRes::Value->new( "Hello" ); }, 
         'Exception (not convertable)' );
dies_ok( sub { Time::HiRes::Value->new( "15.pineapple" ); },
         'Exception (not convertable, leading digits)' );
dies_ok( sub { Time::HiRes::Value->new( "hello", "world" ); },
         'Exception (not convertable pair)' );

my $t1 = Time::HiRes::Value->new( 1 );
ok( defined $t1, 'defined $t1' );
isa_ok( $t1, "Time::HiRes::Value", '$t1 isa Time::HiRes::Value' );

is( "$t1", "1.000000", 'Stringify' );
is( ref( $t1->NUMBER ), "", 'Numerify returns plain scalar' );

my $neg = Time::HiRes::Value->new( -4 );
is( "$neg", "-4.000000", 'Stringify negative' );

$neg = Time::HiRes::Value->new( -4.1 );
is( "$neg", "-4.100000", 'Stringify negative non-integer' );

$neg = Time::HiRes::Value->new( -0.5 );
is( "$neg", "-0.500000", 'Stringify negative non-integer > -1' );

my $t2 = Time::HiRes::Value->new( 1.5 );
is( "$t2", "1.500000", 'Non-integer constructor' );

my $t3 = Time::HiRes::Value->new( 2, 500 );
is( "$t3", "2.000500", 'Array' );

cmp_ok( $t1, '==', 1, 'Compare == scalar 1' );
cmp_ok( $t1, '<=', 2, 'Compare <= scalar 2' );
cmp_ok( $t2, '==', 1.5, 'Compare == scalar 1.5' );
cmp_ok( $t2, '!=', 1.6, 'Compare != scalar 1.6' );

cmp_ok( $t1, '!=', $t3, 'Compare != Value3' );
cmp_ok( $t3, '>', $t2, 'Compare > Value2' );

my $t4 = $t1 + 1;
cmp_ok( $t4, '==', 2, 'add scalar 1' );
isa_ok( $t4, "Time::HiRes::Value", 'add scalar 1 reftype' );
$t4 = $t2 + 2.3;
cmp_ok( $t4, '==', 3.8, 'add scalar 2.3' );
$t4 = 1 + $t1;
cmp_ok( $t4, '==', 2, 'add scalar 1 swapped' );
isa_ok( $t4, "Time::HiRes::Value", 'add scalar 1 swapped reftype' );

$t4 = $t1 + -1;
cmp_ok( $t4, '==', 0, 'inverse of addition' );

$t4 = $t1 + $t2;
cmp_ok( $t4, '==', 2.5, 'add Value2' );

cmp_ok( $t1 + 0, '==', $t1, 'identity of addition' );
cmp_ok( $t1 + 3, '==', 3 + $t1, 'commutativity of addition' );

$t4 = $t3 - 2;
cmp_ok( $t4, '==', 0.0005, 'subtract scalar 2' );
isa_ok( $t4, "Time::HiRes::Value", 'subtrace scalar 2 reftype' );
$t4 = 4 - $t2;
cmp_ok( $t4, '==', 2.5, 'subtract scalar 4 swapped' );
isa_ok( $t4, "Time::HiRes::Value", 'subtrace scalar 4 swapped reftype' );

$t4 = $t1 - 3.1;
cmp_ok( $t4, '==', -2.1, 'subtract scalar 3.1, negative result' );

cmp_ok( $t1 - 0, '==', $t1, 'identity of subtraction' );

cmp_ok( $t1 * 1, '==', "1.000000", 'multiply t1 * 1' );
cmp_ok( $t1 * 250, '==', "250.000000", 'multiply t1 * 250' );

cmp_ok( $t2 * 2, '==', "3.000000", 'multiply t2 * 2' );
cmp_ok( $t2 * 4.2, '==', "6.300000", 'multiply t2 * 4.2' );
cmp_ok( $t2 * -4.2, '==', "-6.300000", 'multiply t2 * -4.2' );

cmp_ok( $t1 * 1, '==', $t1, 'identity of multiplication' );
cmp_ok( $t1 * 3, '==', 3 * $t1, 'commutativity of multiplication' );

cmp_ok( $t1 * 0, '==', 0, 'nullability of multiplication' );

dies_ok( sub { $t1 * $t2 },
         'multiply t1 * t2 fails' );

cmp_ok( $t1 / 1, '==', 1, 'divide t1 / 1' );
cmp_ok( $t1 / 20, '==', 0.05, 'divide t1 / 20' );

cmp_ok( $t2 / 2, '==', 0.75, 'divide t2 / 2' );
cmp_ok( $t2 / 1.5, '==', 1, 'divide t2 / 1.5' );
cmp_ok( $t2 / -4, '==', -0.375, 'divide t2 / -4' );

cmp_ok( $t1 / 1, '==', $t1, 'identity of division' );

dies_ok( sub { 15 / $t1 },
         'divide 15 / t1 fails' );
dies_ok( sub { $t1 / $t2 },
         'divide t1 / t2 fails' );

# Ensure division by zero appears to come from the right place
# Test::Exception seems to mess this one up via Carp, so we'll do it the old-
# fashioned way
$_ = eval { $t1 / 0 };
like( $@, qr/^Illegal division by zero at $0 line/,
          'divide t1 / 0 fails' );
