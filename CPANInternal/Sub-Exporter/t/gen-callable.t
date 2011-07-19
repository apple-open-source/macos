#!perl -T
use strict;
use warnings;

use Test::More tests => 8;

use lib 't/lib';

BEGIN {
  use_ok("Sub::Exporter");
  use_ok("Test::SubExporter::ObjGen", 'baz', '-meta', 'quux', '-ringo');
}

is(quux(), 'QUUX', 'blessed coderef generator');
is(baz(),  'BAZ',  'object with &{} as generator');

is(foo(),  'FOO',  'object with &{} as group generator (1/2)');
is(bar(),  'BAR',  'object with &{} as group generator (2/2)');

is(ringo(),   'starr',   'blessed coderef as group generator (1/2)');
is(richard(), 'starkey', 'blessed coderef as group generator (2/2)');
