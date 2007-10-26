#!/usr/bin/perl -Tw

use strict;
use Test::More tests => 2;

use UNIVERSAL::require;

my $tainted = $0."bogus";
ok !eval { $tainted->require or die $@};
like $@, '/^Insecure dependency in require /';
