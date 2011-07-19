#!/usr/bin/perl

use strict;
use warnings;
use Test::More;
use SQL::Abstract;

plan tests => 13;

my $obj = bless {}, "Foo::Bar";

is(SQL::Abstract->_refkind(undef), 'UNDEF', 'UNDEF');

is(SQL::Abstract->_refkind({}), 'HASHREF', 'HASHREF');
is(SQL::Abstract->_refkind([]), 'ARRAYREF', 'ARRAYREF');

is(SQL::Abstract->_refkind(\{}), 'HASHREFREF', 'HASHREFREF');
is(SQL::Abstract->_refkind(\[]), 'ARRAYREFREF', 'ARRAYREFREF');

is(SQL::Abstract->_refkind(\\{}), 'HASHREFREFREF', 'HASHREFREFREF');
is(SQL::Abstract->_refkind(\\[]), 'ARRAYREFREFREF', 'ARRAYREFREFREF');

is(SQL::Abstract->_refkind("foo"), 'SCALAR', 'SCALAR');
is(SQL::Abstract->_refkind(\"foo"), 'SCALARREF', 'SCALARREF');
is(SQL::Abstract->_refkind(\\"foo"), 'SCALARREFREF', 'SCALARREFREF');

# objects are treated like scalars
is(SQL::Abstract->_refkind($obj), 'SCALAR', 'SCALAR');
is(SQL::Abstract->_refkind(\$obj), 'SCALARREF', 'SCALARREF');
is(SQL::Abstract->_refkind(\\$obj), 'SCALARREFREF', 'SCALARREFREF');

