# $Id: 02hash.t,v 0.19 2006/10/08 03:37:29 ray Exp $
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..12\n"; }
END {print "not ok 1\n" unless $loaded;}
use Clone qw( clone );
use Data::Dumper;
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

# Insert your test code below (better if it prints "ok 13"
# (correspondingly "not ok 13") depending on the success of chunk 13
# of the test code):

package Test::Hash;

use vars @ISA;

@ISA = qw(Clone);

sub new
  {
    my $class = shift;
    my %self = @_;
    bless \%self, $class;
  }

sub DESTROY 
  {
    my $self = shift;
    # warn "DESTROYING $self";
  }

package main;
                                                
sub ok     { print "ok $test\n"; $test++ }
sub not_ok { print "not ok $test\n"; $test++ }

$^W = 0;
$test = 2;

my $a = Test::Hash->new(
    level => 1,
    href  => {
      level => 2,
      href  => {
        level => 3,
        href  => {
          level => 4,
        },
      },
    },
  );

$a->{a} = $a;

my $b = $a->clone(0);
my $c = $a->clone(3);

$a->{level} == $b->{level} ? ok : not_ok;

$b->{href} == $a->{href} ? ok : not_ok;
$c->{href} != $a->{href} ? ok : not_ok;

$b->{href}{href} == $a->{href}{href} ? ok : not_ok;
$c->{href}{href} != $a->{href}{href} ? ok : not_ok;

$c->{href}{href}{level} == 3 ? ok : not_ok;
$c->{href}{href}{href}{level} == 4 ? ok : not_ok;

$b->{href}{href}{href} == $a->{href}{href}{href} ? ok : not_ok;
$c->{href}{href}{href} == $a->{href}{href}{href} ? ok : not_ok;

my %circ = ();
$circ{c} = \%circ;
my $cref = clone(\%circ);
Dumper(\%circ) eq Dumper($cref) ? ok : not_ok;

# test for unicode support
{
  my $a = { chr(256) => 1 };
  my $b = clone( $a );
  ord( (keys(%$a))[0] ) == ord( (keys(%$b))[0] ) ? ok : not_ok;
}
