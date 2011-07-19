# $Id: 04tie.t,v 0.18 2006/10/08 03:37:29 ray Exp $
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..5\n"; }
END {print "not ok 1\n" unless $loaded;}
use Clone qw( clone );
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

my $test = 2;

require 't/dump.pl';
require 't/tied.pl';

my ($a, @a, %a);
tie $a, TIED_SCALAR;
tie %a, TIED_HASH;
tie @a, TIED_ARRAY;
$a{a} = 0;
$a{b} = 1;

my $b = [\%a, \@a, \$a]; 

my $c = clone($b);

my $d1 = &dump($b);
my $d2 = &dump($c);

print "not" unless $d1 eq $d2;
print "ok ", $test++, "\n";

my $t1 = tied(%{$b->[0]});
my $t2 = tied(%{$c->[0]});

$d1 = &dump($t1);
$d2 = &dump($t2);

print "not" unless $d1 eq $d2;
print "ok ", $test++, "\n";

$t1 = tied(@{$b->[1]});
$t2 = tied(@{$c->[1]});

$d1 = &dump($t1);
$d2 = &dump($t2);

print "not" unless $d1 eq $d2;
print "ok ", $test++, "\n";

$t1 = tied(${$b->[2]});
$t2 = tied(${$c->[2]});

$d1 = &dump($t1);
$d2 = &dump($t2);

print "not" unless $d1 eq $d2;
print "ok ", $test++, "\n";

