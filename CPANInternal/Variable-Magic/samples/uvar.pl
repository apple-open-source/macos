#!/usr/bin/env perl

use strict;
use warnings;

use lib qw{blib/arch blib/lib};
use Variable::Magic qw/wizard cast dispell/;

my $wiz = wizard
 fetch  => sub { print STDERR "$_[0] FETCH KEY $_[2]\n" },
 store  => sub { print STDERR "$_[0] STORE KEY $_[2]\n" },
 'exists' => sub { print STDERR "$_[0] EXISTS KEY $_[2]\n" },
 'delete' => sub { print STDERR "$_[0] DELETE KEY $_[2]\n" };

my %h = (foo => 1, bar => 2);
cast %h, $wiz;

print STDERR "foo was $h{foo}\n";
$h{foo} = 3;
print STDERR "now foo is $h{foo}\n";

print STDERR "foo exists!\n" if exists $h{foo};

my $d = delete $h{foo};
print STDERR "foo deleted, got $d\n";

dispell %h, $wiz;
