# $Id: 06refcnt.t,v 0.22 2007/07/25 03:41:06 ray Exp $
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..20\n"; }
END {print "not ok 1\n" unless $loaded;}
use Clone qw( clone );
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

# Insert your test code below (better if it prints "ok 13"
# (correspondingly "not ok 13") depending on the success of chunk 13
# of the test code):

# code to test for memory leaks

## use Benchmark;
use Data::Dumper;
# use Storable qw( dclone );

$^W = 1;
$test = 2;

sub ok     { printf("ok %d\n", $test++); }
sub not_ok { printf("not ok %d\n", $test++); }

use strict;

package Test::Hash;

@Test::Hash::ISA = qw( Clone );

sub new()
{
  my ($class) = @_;
  my $self = {};
  bless $self, $class;
}

my $ok = 0;
END { $ok = 1; };
sub DESTROY
{
  my $self = shift;
  printf("not ") if $ok;
  printf("ok %d\n", $::test++);
}

package main;

{
  my $a = Test::Hash->new();
  my $b = $a->clone;
  # my $c = dclone($a);
}

# benchmarking bug
{
  my $a = Test::Hash->new();
  my $sref = sub { my $b = clone($a) };
  $sref->();
}

# test for cloning unblessed ref
{
  my $a = {};
  my $b = clone($a);
  bless $a, 'Test::Hash';
  bless $b, 'Test::Hash';
}

# test for cloning unblessed ref
{
  my $a = [];
  my $b = clone($a);
  bless $a, 'Test::Hash';
  bless $b, 'Test::Hash';
}

# test for cloning ref that was an int(IV)
{
  my $a = 1;
  $a = [];
  my $b = clone($a);
  bless $a, 'Test::Hash';
  bless $b, 'Test::Hash';
}

# test for cloning ref that was a string(PV)
{
  my $a = '';
  $a = [];
  my $b = clone($a);
  bless $a, 'Test::Hash';
  bless $b, 'Test::Hash';
}

# test for cloning ref that was a magic(PVMG)
{
  my $a = *STDOUT;
  $a = [];
  my $b = clone($a);
  bless $a, 'Test::Hash';
  bless $b, 'Test::Hash';
}

# test for cloning weak reference
{
  use Scalar::Util qw(weaken isweak);
  my $a = new Test::Hash();
  my $b = { r => $a };
  $a->{r} = $b;
  weaken($b->{'r'});
  my $c = clone($a);
}

# another weak reference problem, this one causes a segfault in 0.24
{
  use Scalar::Util qw(weaken isweak);
  my $a = new Test::Hash();
  {
    my $b = [ $a, $a ];
    $a->{r} = $b;
    weaken($b->[0]);
    weaken($b->[1]);
  }
  my $c = clone($a);
  # check that references point to the same thing
  print  "not " unless $c->{'r'}[0] == $c->{'r'}[1];
  printf "ok %d\n", $::test++;
}
