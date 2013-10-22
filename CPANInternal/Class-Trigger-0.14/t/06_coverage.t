use strict;
use Test::More tests => 1;

use lib 't/lib';
use Foo;

my $foo = Foo->new();
is($foo->call_trigger(), 0, 'no triggers, no action');
