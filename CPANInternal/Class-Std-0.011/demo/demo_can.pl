package Foo;
use Class::Std;

sub foo { print "$_[0]\->Foo::foo()\n" }


package Bar;
use Class::Std;

sub AUTOMETHOD {
    return sub { print "$_[0]\->Bar::foo()\n" } if m/\A foo \Z/xms;
    return;
}


package Baz;
use base qw( Bar );

package Qux;


package main;

if ($meth_ref = Foo->can('foo')) {
    Foo->$meth_ref();
}

if ($meth_ref = Bar->can('foo')) {
    Bar->$meth_ref();
}

if ($meth_ref = Qux->can('foo')) {
    Qux->$meth_ref();
}

