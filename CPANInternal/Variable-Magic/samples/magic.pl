#!/usr/bin/env perl

use strict;
use warnings;

use lib qw{blib/arch blib/lib};
use Variable::Magic qw/wizard cast dispell/;

sub foo { print STDERR "got ${$_[0]}!\n" }
my $bar = sub { ++${$_[0]}; print STDERR "now set to ${$_[0]}!\n"; };

my $a = 1;
{
 my $wiz = wizard get  => \&foo,
                  set  => $bar,
                  free => sub { print STDERR "deleted!\n"; };
 cast $a, $wiz, qw/a b c/;
 ++$a;              # "got 1!", "now set to 3!"
 dispell $a, $wiz;
 cast $a, $wiz;
 my $b = 123;
 cast $b, $wiz;
}                   # "deleted!"
my $b = $a;         # "got 3!"
$a = 3;             # "now set to 4!"
$b = 3;             # (nothing)
