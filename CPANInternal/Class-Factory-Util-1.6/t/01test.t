use strict;

use Test;

plan tests => 5;

require Class::Factory::Util;

ok(1);

use lib 'lib', 't/lib', 't/lib2';

use Factory;

my @s = sort Factory->subclasses;

ok( scalar @s, 3 );

ok( $s[0], 'Bar' );
ok( $s[1], 'Baz' );
ok( $s[2], 'Foo' );
