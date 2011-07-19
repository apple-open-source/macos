# Testing magical scalars (using tied scalar)
# Note that XSUBs do not handle magical scalars automatically.

use strict;
use warnings;

use Test::More;
use Test::Exception;

use Class::MOP;

use Tie::Scalar;

{
    package Foo;
    use metaclass;

    Foo->meta->add_attribute('bar' =>
        reader => 'get_bar',
        writer => 'set_bar',
    );

    Foo->meta->add_attribute('baz' =>
        accessor => 'baz',
    );

    Foo->meta->make_immutable();
}

{
    tie my $foo, 'Tie::StdScalar', Foo->new(bar => 100, baz => 200);

    is $foo->get_bar, 100, 'reader with tied self';
    is $foo->baz,     200, 'accessor/r with tied self';

    $foo->set_bar(300);
    $foo->baz(400);

    is $foo->get_bar, 300, 'writer with tied self';
    is $foo->baz,     400, 'accessor/w with tied self';
}

{
    my $foo = Foo->new();

    tie my $value, 'Tie::StdScalar', 42;

    $foo->set_bar($value);
    $foo->baz($value);

    is $foo->get_bar, 42, 'reader/writer with tied value';
    is $foo->baz,     42, 'accessor with tied value';
}

{
    my $x = tie my $value, 'Tie::StdScalar', 'Class::MOP';

    lives_ok{ Class::MOP::load_class($value) } 'load_class(tied scalar)';

    $value = undef;
    $x->STORE('Class::MOP'); # reset

    lives_and{
        ok Class::MOP::is_class_loaded($value);
    } 'is_class_loaded(tied scalar)';

    $value = undef;
    $x->STORE(\&Class::MOP::get_code_info); # reset

    lives_and{
        is_deeply [Class::MOP::get_code_info($value)], [qw(Class::MOP get_code_info)], 'get_code_info(tied scalar)';
    }
}

done_testing;
