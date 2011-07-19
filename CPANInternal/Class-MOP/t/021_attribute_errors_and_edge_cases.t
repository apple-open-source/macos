use strict;
use warnings;

use Test::More;
use Test::Exception;

use Class::MOP;
use Class::MOP::Attribute;

# most values are static

{
    dies_ok {
        Class::MOP::Attribute->new('$test' => (
            default => qr/hello (.*)/
        ));
    } '... no refs for defaults';

    dies_ok {
        Class::MOP::Attribute->new('$test' => (
            default => []
        ));
    } '... no refs for defaults';

    dies_ok {
        Class::MOP::Attribute->new('$test' => (
            default => {}
        ));
    } '... no refs for defaults';


    dies_ok {
        Class::MOP::Attribute->new('$test' => (
            default => \(my $var)
        ));
    } '... no refs for defaults';

    dies_ok {
        Class::MOP::Attribute->new('$test' => (
            default => bless {} => 'Foo'
        ));
    } '... no refs for defaults';

}

{
    dies_ok {
        Class::MOP::Attribute->new('$test' => (
            builder => qr/hello (.*)/
        ));
    } '... no refs for builders';

    dies_ok {
        Class::MOP::Attribute->new('$test' => (
            builder => []
        ));
    } '... no refs for builders';

    dies_ok {
        Class::MOP::Attribute->new('$test' => (
            builder => {}
        ));
    } '... no refs for builders';


    dies_ok {
        Class::MOP::Attribute->new('$test' => (
            builder => \(my $var)
        ));
    } '... no refs for builders';

    dies_ok {
        Class::MOP::Attribute->new('$test' => (
            builder => bless {} => 'Foo'
        ));
    } '... no refs for builders';

    dies_ok {
        Class::MOP::Attribute->new('$test' => (
            builder => 'Foo', default => 'Foo'
        ));
    } '... no default AND builder';

}


{ # bad construtor args
    dies_ok {
        Class::MOP::Attribute->new();
    } '... no name argument';

    # These are no longer errors
    lives_ok {
        Class::MOP::Attribute->new('');
    } '... bad name argument';

    lives_ok {
        Class::MOP::Attribute->new(0);
    } '... bad name argument';
}

{
    my $attr = Class::MOP::Attribute->new('$test');
    dies_ok {
        $attr->attach_to_class();
    } '... attach_to_class died as expected';

    dies_ok {
        $attr->attach_to_class('Fail');
    } '... attach_to_class died as expected';

    dies_ok {
        $attr->attach_to_class(bless {} => 'Fail');
    } '... attach_to_class died as expected';
}

{
    my $attr = Class::MOP::Attribute->new('$test' => (
        reader => [ 'whoops, this wont work' ]
    ));

    $attr->attach_to_class(Class::MOP::Class->initialize('Foo'));

    dies_ok {
        $attr->install_accessors;
    } '... bad reader format';
}

{
    my $attr = Class::MOP::Attribute->new('$test');

    dies_ok {
        $attr->_process_accessors('fail', 'my_failing_sub');
    } '... cannot find "fail" type generator';
}


{
    {
        package My::Attribute;
        our @ISA = ('Class::MOP::Attribute');
        sub generate_reader_method { eval { die } }
    }

    my $attr = My::Attribute->new('$test' => (
        reader => 'test'
    ));

    dies_ok {
        $attr->install_accessors;
    } '... failed to generate accessors correctly';
}

{
    my $attr = Class::MOP::Attribute->new('$test' => (
        predicate => 'has_test'
    ));

    my $Bar = Class::MOP::Class->create('Bar');
    isa_ok($Bar, 'Class::MOP::Class');

    $Bar->add_attribute($attr);

    can_ok('Bar', 'has_test');

    is($attr, $Bar->remove_attribute('$test'), '... removed the $test attribute');

    ok(!Bar->can('has_test'), '... Bar no longer has the "has_test" method');
}


{
    # NOTE:
    # the next three tests once tested that
    # the code would fail, but we lifted the
    # restriction so you can have an accessor
    # along with a reader/writer pair (I mean
    # why not really). So now they test that
    # it works, which is kinda silly, but it
    # tests the API change, so I keep it.

    lives_ok {
        Class::MOP::Attribute->new('$foo', (
            accessor => 'foo',
            reader   => 'get_foo',
        ));
    } '... can create accessors with reader/writers';

    lives_ok {
        Class::MOP::Attribute->new('$foo', (
            accessor => 'foo',
            writer   => 'set_foo',
        ));
    } '... can create accessors with reader/writers';

    lives_ok {
        Class::MOP::Attribute->new('$foo', (
            accessor => 'foo',
            reader   => 'get_foo',
            writer   => 'set_foo',
        ));
    } '... can create accessors with reader/writers';
}

done_testing;
