#!/usr/bin/env perl

use strict;
use warnings;

use lib qw{blib/arch blib/lib};
use Variable::Magic qw/wizard cast/;
use Tie::Hash;

my $wiz = wizard copy => sub { print STDERR "COPY $_[2] => $_[3]\n" },
                 free => sub { print STDERR "FREE\n" };
my %h;
tie %h, 'Tie::StdHash';
%h = (a => 1, b => 2);
cast %h, $wiz;
$h{b} = 3;
my $x = delete $h{b};
$x == 3 or die 'incorrect';
