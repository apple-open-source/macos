use strict;
use Test::More tests => 2;

package Foo;
use Class::Trigger;

sub foo {
    my $self = shift;
    $_ = "foo";
    $self->call_trigger('before_foo');
    ::is $_, "foo";
}

{
    my $foo = bless {}, "Foo";
    $foo->foo;
}

{
    Foo->add_trigger(before_foo => sub { });
    my $foo = bless {}, "Foo";
    $foo->foo;
}


