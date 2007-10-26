#! /usr/local/perl -w
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

#########################

use Test::More qw/no_plan/;
BEGIN {
    use_ok("version", 0.49); # If we made it this far, we are ok.
}
require "t/coretests.pm";

diag "Tests with empty derived class" unless $ENV{PERL_CORE};

package version::Empty;
use base version;
$VERSION = 0.01;
no warnings 'redefine';
*::qv = sub { return bless version::qv(shift), __PACKAGE__; };

package version::Bad;
use base version;
sub new { my($self,$n)=@_;  bless \$n, $self }

package main;
my $testobj = version::Empty->new(1.002_003);
isa_ok( $testobj, "version::Empty" );
ok( $testobj->numify == 1.002003, "Numified correctly" );
ok( $testobj->stringify eq "1.002003", "Stringified correctly" );
ok( $testobj->normal eq "v1.2.3", "Normalified correctly" );

my $verobj = version->new("1.2.4");
ok( $verobj > $testobj, "Comparison vs parent class" );
ok( $verobj gt $testobj, "Comparison vs parent class" );
BaseTests("version::Empty");

diag "tests with bad subclass" unless $ENV{PERL_CORE};
$testobj = version::Bad->new(1.002_003);
isa_ok( $testobj, "version::Bad" );
eval { my $string = $testobj->numify };
like($@, qr/Invalid version object/,
    "Bad subclass numify");
eval { my $string = $testobj->normal };
like($@, qr/Invalid version object/,
    "Bad subclass normal");
eval { my $string = $testobj->stringify };
like($@, qr/Invalid version object/,
    "Bad subclass stringify");
eval { my $test = $testobj > 1.0 };
like($@, qr/Invalid version object/,
    "Bad subclass vcmp");
