use strict;
use warnings;
use Test::More tests => 6;

BEGIN { use_ok('B::Hooks::EndOfScope') }

BEGIN {
    ok(exists &on_scope_end, 'on_scope_end imported');
    is(prototype('on_scope_end'), '&', '.. and has the right prototype');
}

our ($i, $called);

BEGIN { $i = 0 }

sub foo {
    BEGIN {
        on_scope_end { $called = 1; $i = 42 };
        on_scope_end { $i = 1 };
    };

    is($i, 1, 'value still set at runtime');
}

BEGIN {
    ok($called, 'first callback invoked');
    is($i, 1, '.. but the second is invoked later')
}

foo();
