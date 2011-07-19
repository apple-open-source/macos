use strict;
use warnings;

use Test::More;
use Test::Exception;

use Carp;

$SIG{__WARN__} = \&croak;

{
    package Foo;

    ::throws_ok{
        Class::MOP::in_global_destruction();
        } qr/\b deprecated \b/xmsi,
        'Class::MOP::in_global_destruction is deprecated';
}

{
    package Bar;

    use Class::MOP::Deprecated -compatible => 0.93;

    ::throws_ok{
        Class::MOP::in_global_destruction();
        } qr/\b deprecated \b/xmsi,
        'Class::MOP::in_global_destruction is deprecated with 0.93 compatibility';
}

{
    package Baz;

    use Class::MOP::Deprecated -compatible => 0.92;

    ::lives_ok{
        Class::MOP::in_global_destruction();
        }
        'Class::MOP::in_global_destruction is not deprecated with 0.92 compatibility';
}

{
    package Baz::Inner;

    ::lives_ok{
        Class::MOP::in_global_destruction();
        } 'safe in an inner class';
}

{
    package Foo2;

    use metaclass;

    ::throws_ok{ Foo2->meta->get_attribute_map }
        qr/\Qget_attribute_map method has been deprecated/,
        'get_attribute_map is deprecated';
}

{
    package Quux;

    use Class::MOP::Deprecated -compatible => 0.92;
    use Scalar::Util qw( blessed );

    use metaclass;

    sub foo {42}

    Quux->meta->add_method( bar => sub {84} );

    my $map = Quux->meta->get_method_map;
    my @method_objects = grep { blessed($_) } values %{$map};

    ::is(
        scalar @method_objects, 3,
        'get_method_map still returns all values as method object'
    );
    ::is_deeply(
        [ sort keys %{$map} ],
        [qw( bar foo meta )],
        'get_method_map returns expected methods'
    );
}

done_testing;
