use strict;
use warnings;

use Test::More;
use Test::Exception;

use Class::MOP;

{
    my $attr = Class::MOP::Attribute->new('$test');
    is( $attr->meta, Class::MOP::Attribute->meta,
        '... instance and class both lead to the same meta' );
}

{
    my $meta = Class::MOP::Attribute->meta();
    isa_ok( $meta, 'Class::MOP::Class' );

    my @methods = qw(
        new
        clone

        initialize_instance_slot
        _set_initial_slot_value

        name
        has_accessor      accessor
        has_writer        writer
        has_write_method  get_write_method  get_write_method_ref
        has_reader        reader
        has_read_method   get_read_method   get_read_method_ref
        has_predicate     predicate
        has_clearer       clearer
        has_builder       builder
        has_init_arg      init_arg
        has_default       default           is_default_a_coderef
        has_initializer   initializer
        has_insertion_order insertion_order _set_insertion_order

        definition_context

        slots
        get_value
        set_value
        get_raw_value
        set_raw_value
        set_initial_value
        has_value
        clear_value

        associated_class
        attach_to_class
        detach_from_class

        accessor_metaclass

        associated_methods
        associate_method

        process_accessors
        _process_accessors
        install_accessors
        remove_accessors

        _new
    );

    is_deeply(
        [
            sort Class::MOP::Mixin::AttributeCore->meta->get_method_list,
            $meta->get_method_list
        ],
        [ sort @methods ],
        '... our method list matches'
    );

    foreach my $method_name (@methods) {
        ok( $meta->find_method_by_name($method_name),
            '... Class::MOP::Attribute->find_method_by_name(' . $method_name . ')' );
    }

    my @attributes = (
        'name',
        'accessor',
        'reader',
        'writer',
        'predicate',
        'clearer',
        'builder',
        'init_arg',
        'initializer',
        'definition_context',
        'default',
        'associated_class',
        'associated_methods',
        'insertion_order',
    );

    is_deeply(
        [
            sort Class::MOP::Mixin::AttributeCore->meta->get_attribute_list,
            $meta->get_attribute_list
        ],
        [ sort @attributes ],
        '... our attribute list matches'
    );

    foreach my $attribute_name (@attributes) {
        ok( $meta->find_attribute_by_name($attribute_name),
                  '... Class::MOP::Attribute->find_attribute_by_name('
                . $attribute_name
                . ')' );
    }

    # We could add some tests here to make sure that
    # the attribute have the appropriate
    # accessor/reader/writer/predicate combinations,
    # but that is getting a little excessive so I
    # wont worry about it for now. Maybe if I get
    # bored I will do it.
}

done_testing;
