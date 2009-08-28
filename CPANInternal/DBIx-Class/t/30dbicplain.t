#!/usr/bin/perl

use strict;
use warnings;
use Test::More;

use lib qw(t/lib);

plan tests => 3;

my @warnings;

{
  local $SIG{__WARN__} = sub { push(@warnings, $_[0]); };
  require DBICTest::Plain;
}

like($warnings[0], qr/compose_connection deprecated as of 0\.08000/,
      'deprecation warning emitted ok');
cmp_ok(@warnings, '==', 1, 'no unexpected warnings');
cmp_ok(DBICTest::Plain->resultset('Test')->count, '>', 0, 'count is valid');
