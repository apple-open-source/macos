use Sub::Install;
use Test::More tests => 4;

use strict;
use warnings;

BEGIN { use_ok('Sub::Install'); }

package Bar;
{ no warnings 'once';
  *import = Sub::Install::exporter { exports => [ qw(foo) ] };
}
sub foo { return 10; }

package main;

eval { Bar->import('bar'); };
like($@, qr/'bar' is not exported/, "exception on bad import");

eval { foo(); };
like($@, qr/Undefined subroutine/, "foo isn't imported yet");

Bar->import(qw(foo));
is(foo(), 10, "foo imported from Bar OK");
