#!perl -w
use strict;
use Test::More tests => 6;

package Foo;
use base 'Class::Accessor::Chained';
__PACKAGE__->mk_accessors(qw( foo bar baz ));
package main;

my $foo = Foo->new->foo(1)->baz(2)->bar(4);
isa_ok( $foo, 'Foo' );
is( $foo->bar, 4, "get gets the value" );
is( $foo->foo( 5 ), $foo, "set gets the object" );

# and again, but with Fast accessors
package Bar;
use base 'Class::Accessor::Chained::Fast';
__PACKAGE__->mk_accessors(qw( foo bar baz ));
package main;

my $bar = Bar->new->foo(1)->baz(2)->bar(4);
isa_ok( $bar, 'Bar' );
is( $bar->bar, 4, "get gets the value" );
is( $bar->foo( 5 ), $bar, "set gets the object" );
