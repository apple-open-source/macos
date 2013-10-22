package Common;
use Class::Std;
{
    sub AUTOMETHOD {
        return sub { return 'Common::foo()' };
        return;
    }
}

package Foo;
use Class::Std;
use base qw( Common );
{
    sub foo {
        return 'Foo::foo()';
    }
}


package Bar;
use Class::Std;
use base qw( Common );
{
    sub AUTOMETHOD {
        return sub { return 'Bar::foo()' }
            if m/\A foo \Z/xms;
        return;
    }
}

package Baz;
use base qw( Bar );

package Qux;


package main;
use Test::More 'no_plan';

my $meth_ref;

$meth_ref = Common->can('foo');
ok( $meth_ref                                => 'Common can foo()'        );
is( Common->foo(),       'Common::foo()'     => 'Direct common foo()'     );
is( Common->$meth_ref(), 'Common::foo()'     => 'Indirect common foo()'   );


$meth_ref = Foo->can('foo');
ok( $meth_ref                                => 'Foo can foo()'        );
is( Foo->foo(),       'Foo::foo()'           => 'Direct Foo foo()'     );
is( Foo->$meth_ref(), 'Foo::foo()'           => 'Indirect Foo foo()'   );


$meth_ref = Foo->can('bar');
ok( $meth_ref                                => 'Foo can bar()'        );
is( Foo->bar(),       'Common::foo()'        => 'Direct Foo bar()'     );
is( Foo->$meth_ref(), 'Common::foo()'        => 'Indirect Foo bar()'   );


$meth_ref = Bar->can('foo');
ok( $meth_ref                                => 'Bar can foo()'        );
is( Bar->foo(),       'Bar::foo()'           => 'Direct Bar foo()'     );
is( Bar->$meth_ref(), 'Bar::foo()'           => 'Indirect Bar foo()'   );


$meth_ref = Bar->can('bar');
ok( $meth_ref                                => 'Bar can bar()'        );
is( Bar->bar(),       'Common::foo()'        => 'Direct Bar bar()'     );
is( Bar->$meth_ref(), 'Common::foo()'        => 'Indirect Bar bar()'   );



$meth_ref = Baz->can('foo');
ok( $meth_ref                                => 'Baz can foo()'        );
is( Baz->foo(),       'Bar::foo()'           => 'Direct Baz foo()'     );
is( Baz->$meth_ref(), 'Bar::foo()'           => 'Indirect Baz foo()'   );


$meth_ref = Baz->can('bar');
ok( $meth_ref                                => 'Baz can bar()'        );
is( Baz->bar(),       'Common::foo()'        => 'Direct Baz bar()'     );
is( Baz->$meth_ref(), 'Common::foo()'        => 'Indirect Baz bar()'   );


$meth_ref = Qux->can('foo');
ok( !$meth_ref                               => 'Qux no can foo()'     );
eval { Qux->foo() };
ok( $@                                       => 'No Qux foo()'         );


