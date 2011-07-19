use strict;
use warnings;
use Test::More tests => 2;

use B::Hooks::EndOfScope;

eval q[
    sub foo {
        BEGIN {
            on_scope_end { die 'bar' };
        }
    }
];

like($@, qr/^bar/);
pass('no segfault');
