#!/usr/bin/env perl

use strict;
use warnings;

use Tie::Hash;

use lib qw{blib/arch blib/lib};
use Variable::Magic qw/wizard cast VMG_UVAR/;

use Benchmark qw/cmpthese/;

die 'Your perl does not support the nice uvar magic of 5.10.*' unless VMG_UVAR;

tie my %t, 'Tie::StdHash';
$t{a} = 1;

my $wiz = wizard fetch  => sub { 0 },
                 store  => sub { 0 },
                 exists => sub { 0 },
                 delete => sub { 0 };
my %v;
cast %v, $wiz;
$v{a} = 2;

print "Using Variable::Magic ", $Variable::Magic::VERSION, "\n";

print "Fetch:\n";
cmpthese -1, {
 'tie'  => sub { $t{a} },
 'v::m' => sub { $v{a} }
};

print "Store:\n";
cmpthese -1, {
 'tie'  => sub { $t{a} = 2 },
 'v::m' => sub { $v{a} = 2 }
};

print "Exists:\n";
cmpthese -1, {
 'tie'  => sub { exists $t{a} },
 'v::m' => sub { exists $v{a} }
};

print "Delete/store:\n";
cmpthese -1, {
 'tie'  => sub { delete $t{a}; $t{a} = 3 },
 'v::m' => sub { delete $v{a}; $v{a} = 3 }
};
