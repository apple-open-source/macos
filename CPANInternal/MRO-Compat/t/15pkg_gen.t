#!./perl

use strict;
use warnings;

use Test::More tests => 4;

BEGIN { use_ok('MRO::Compat') }

{
    package Foo;
    our @ISA = qw//;
}

my $f_gen = mro::get_pkg_gen('Foo');
ok($f_gen > 0, 'Foo pkg_gen > 0');

{
    no warnings 'once';
    *Foo::foo_func = sub { 123 };
}
my $new_f_gen = mro::get_pkg_gen('Foo');
ok($new_f_gen > $f_gen, 'Foo pkg_gen incs for methods');
$f_gen = $new_f_gen;

@Foo::ISA = qw/Bar/;
$new_f_gen = mro::get_pkg_gen('Foo');
ok($new_f_gen > $f_gen, 'Foo pkg_gen incs for @ISA');

