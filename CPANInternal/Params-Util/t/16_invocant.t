#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
	$ENV{PERL_PARAMS_UTIL_PP} ||= 1;
}

use Test::More tests => 11;
use File::Spec::Functions ':ALL';
BEGIN {
	use_ok('Params::Util', qw(_INVOCANT));
}

my $object = bless \do { my $i } => 'Params::Util::Test::Bogus::Whatever';
my $false_obj1 = bless \do { my $i } => 0;
my $false_obj2 = bless \do { my $i } => "\0";
my $tied   = tie my $x, 'Params::Util::Test::_INVOCANT::Tied';
my $unpkg  = 'Params::Util::Test::_INVOCANT::Fake';
my $pkg    = 'Params::Util::Test::_INVOCANT::Real'; eval "package $pkg;"; ## no critic

my @data = (# I
  [ undef        , 0, 'undef' ],
  [ 1000        => 0, '1000' ],
  [ $unpkg      => 1, qq("$unpkg") ],
  [ $pkg        => 1, qq("$pkg") ],
  [ []          => 0, '[]' ],
  [ {}          => 0, '{}' ],
  [ $object     => 1, 'blessed reference' ],
  [ $false_obj1 => 1, 'blessed reference' ],
  [ $tied       => 1, 'tied value' ],
);

for my $datum (@data) {
  is(
    _INVOCANT($datum->[0]) ? 1 : 0,
    $datum->[1],
    "$datum->[2] " . ($datum->[1] ? 'is' : "isn't") . " _IN"
  );
}

# Skip the most evil test except on automated testing, because it
# fails on at least one common production OS (RedHat Enterprise Linux 4)
# and the test case should be practically impossible to encounter
# in real life. The damage the bug could cause users in production is
# far lower than the damage caused by Params::Util failing to install.
SKIP: {
	unless ( $ENV{AUTOMATED_TESTING} ) {
		skip("Skipping nasty test unless AUTOMATED_TESTING", 1);
	}
	ok( !! _INVOCANT($false_obj2), 'Testing null class as an invocant' );
}

package Params::Util::Test::_INVOCANT::Tied;
sub TIESCALAR {
  my ($class, $value) = @_;
  return bless \$value => $class;
}
