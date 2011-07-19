# $Id: 01array.t,v 0.19 2006/10/08 03:37:29 ray Exp $
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..7\n"; }
END {print "not ok 1\n" unless $loaded;}
use Clone qw( clone );
use Data::Dumper;
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

# Insert your test code below (better if it prints "ok 13"
# (correspondingly "not ok 13") depending on the success of chunk 13
# of the test code):

package Test::Array;

use vars @ISA;

@ISA = qw(Clone);

sub new
  {
    my $class = shift;
    my @self = @_;
    bless \@self, $class;
  }

package main;
                                                
sub ok     { print "ok $test\n"; $test++ }
sub not_ok { print "not ok $test\n"; $test++ }

$^W = 0;
$test = 2;
my $a = Test::Array->new(
    1, 
    [ 'two', 
      [ 3,
        ['four']
      ],
    ],
  );
my $b = $a->clone(0);
my $c = $a->clone(2);

# TEST 2
$b->[1][0] eq 'two' ? ok : not_ok;

# TEST 3
$b->[1] == $a->[1] ? ok : not_ok;

# TEST 4
$c->[1] != $a->[1] ? ok : not_ok;

# TEST 5
$c->[1][1][1] == $a->[1][1][1] ? ok : not_ok;

my @circ = ();
$circ[0] = \@circ;
$aref = clone(\@circ);
Dumper(\@circ) eq Dumper($aref) ? ok : not_ok;

# test for unicode support
{
  my $a = [ chr(256) => 1 ];
  my $b = clone( $a );
  ord( $a->[0] ) == ord( $b->[0] ) ? ok : not_ok;
}
