use strict;
use warnings;
use Test::More tests => 18;
use DBIx::Class::Schema::Loader::Utils 'split_name';

is_deeply [split_name('foo_bar_baz')], [qw/foo bar baz/],
    'by underscore';

is_deeply [split_name('foo__bar__baz')], [qw/foo bar baz/],
    'by double underscore';

is_deeply [split_name('Foo_Bar_Baz')], [qw/Foo Bar Baz/],
    'by underscore with full capitalization';

is_deeply [split_name('foo_Bar_Baz')], [qw/foo Bar Baz/],
    'by underscore with lcfirst capitalization';

is_deeply [split_name('fooBarBaz')], [qw/foo Bar Baz/],
    'lcfirst camelCase identifier';

is_deeply [split_name('FooBarBaz')], [qw/Foo Bar Baz/],
    'ucfirst camelCase identifier';

is_deeply [split_name('VLANValidID')], [qw/VLAN Valid ID/],
    'CAMELCase identifier (word with all caps)';

is_deeply [split_name('VlanVALIDId')], [qw/Vlan VALID Id/],
    'CamelCASE identifier (second word with all caps)';

is_deeply [split_name('foo..bar/baz')], [qw/foo bar baz/],
    'by non-alphanum chars';

# naming=v7

is_deeply [split_name('foo_bar_baz', 7)], [qw/foo bar baz/],
    'by underscore for v=7';

is_deeply [split_name('foo__bar__baz', 7)], [qw/foo bar baz/],
    'by double underscore for v=7';

is_deeply [split_name('Foo_Bar_Baz', 7)], [qw/Foo Bar Baz/],
    'by underscore with full capitalization for v=7';

is_deeply [split_name('foo_Bar_Baz', 7)], [qw/foo Bar Baz/],
    'by underscore with lcfirst capitalization for v=7';

is_deeply [split_name('fooBarBaz', 7)], [qw/foo Bar Baz/],
    'lcfirst camelCase identifier for v=7';

is_deeply [split_name('FooBarBaz', 7)], [qw/Foo Bar Baz/],
    'ucfirst camelCase identifier for v=7';

is_deeply [split_name('VLANValidID', 7)], [qw/VLANValid ID/],
    'CAMELCase identifier (word with all caps) for v=7';

is_deeply [split_name('VlanVALIDId', 7)], [qw/Vlan VALIDId/],
    'CamelCASE identifier (second word with all caps) for v=7';

is_deeply [split_name('foo..bar/baz', 7)], [qw/foo bar baz/],
    'by non-alphanum chars for v=7';
