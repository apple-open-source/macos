use Sub::Install;
use Test::More 'no_plan';

use strict;
use warnings;

BEGIN { use_ok("Sub::Install"); }

my $code = sub { return 'FOO' };

bless $code, "Sub::Install::Bogus";

Sub::Install::install_sub({
  code => $code,
  as   => 'code',
});

is(code(), "FOO", "installed sub is OK");

