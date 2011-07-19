use strict;
use warnings;

use Test::More;
use Test::Exception;

use Class::MOP;
use Class::MOP::Class;
use Class::MOP::Package;
use Class::MOP::Module;

{
    my $class = Class::MOP::Class->initialize('Foo');
    is($class->meta, Class::MOP::Class->meta, '... instance and class both lead to the same meta');
}

my $class_mop_class_meta = Class::MOP::Class->meta();
isa_ok($class_mop_class_meta, 'Class::MOP::Class');

my $class_mop_package_meta = Class::MOP::Package->meta();
isa_ok($class_mop_package_meta, 'Class::MOP::Package');

my $class_mop_module_meta = Class::MOP::Module->meta();
isa_ok($class_mop_module_meta, 'Class::MOP::Module');

my @class_mop_package_methods = qw(
    _new

    initialize reinitialize

    name
    namespace

    add_package_symbol get_package_symbol has_package_symbol remove_package_symbol
    list_all_package_symbols get_all_package_symbols remove_package_glob

    _deconstruct_variable_name

    get_method_map
);

my @class_mop_module_methods = qw(
    _new

    _instantiate_module

    version authority identifier create
);

my @class_mop_class_methods = qw(
    _new

    is_pristine

    initialize create

    update_package_cache_flag
    reset_package_cache_flag

    create_anon_class is_anon_class

    instance_metaclass get_meta_instance
    create_meta_instance _create_meta_instance
    new_object clone_object
    construct_instance _construct_instance
    construct_class_instance _construct_class_instance
    clone_instance _clone_instance
    rebless_instance rebless_instance_back rebless_instance_away
    check_metaclass_compatibility _check_metaclass_compatibility

    add_meta_instance_dependencies remove_meta_instance_dependencies update_meta_instance_dependencies
    add_dependent_meta_instance remove_dependent_meta_instance
    invalidate_meta_instances invalidate_meta_instance

    superclasses subclasses direct_subclasses class_precedence_list
    linearized_isa _superclasses_updated

    alias_method get_all_method_names get_all_methods compute_all_applicable_methods
        find_method_by_name find_all_methods_by_name find_next_method_by_name

        add_before_method_modifier add_after_method_modifier add_around_method_modifier

    _attach_attribute
    _post_add_attribute
    remove_attribute
    find_attribute_by_name
    get_all_attributes

    compute_all_applicable_attributes
    get_attribute_map

    is_mutable is_immutable make_mutable make_immutable
    _initialize_immutable _install_inlined_code _inlined_methods
    _add_inlined_method _inline_accessors _inline_constructor
    _inline_destructor _immutable_options _rebless_as_immutable
    _rebless_as_mutable _remove_inlined_code

    _immutable_metaclass
    immutable_trait immutable_options
    constructor_name constructor_class destructor_class

    DESTROY
);

# check the class ...

is_deeply([ sort $class_mop_class_meta->get_method_list ], [ sort @class_mop_class_methods ], '... got the correct method list for class');

foreach my $method_name (sort @class_mop_class_methods) {
    ok($class_mop_class_meta->has_method($method_name), '... Class::MOP::Class->has_method(' . $method_name . ')');
    {
        no strict 'refs';
        is($class_mop_class_meta->get_method($method_name)->body,
           \&{'Class::MOP::Class::' . $method_name},
           '... Class::MOP::Class->get_method(' . $method_name . ') == &Class::MOP::Class::' . $method_name);
    }
}

## check the package ....

is_deeply([ sort $class_mop_package_meta->get_method_list ], [ sort @class_mop_package_methods ], '... got the correct method list for package');

foreach my $method_name (sort @class_mop_package_methods) {
    ok($class_mop_package_meta->has_method($method_name), '... Class::MOP::Package->has_method(' . $method_name . ')');
    {
        no strict 'refs';
        is($class_mop_package_meta->get_method($method_name)->body,
           \&{'Class::MOP::Package::' . $method_name},
           '... Class::MOP::Package->get_method(' . $method_name . ') == &Class::MOP::Package::' . $method_name);
    }
}

## check the module ....

is_deeply([ sort $class_mop_module_meta->get_method_list ], [ sort @class_mop_module_methods ], '... got the correct method list for module');

foreach my $method_name (sort @class_mop_module_methods) {
    ok($class_mop_module_meta->has_method($method_name), '... Class::MOP::Module->has_method(' . $method_name . ')');
    {
        no strict 'refs';
        is($class_mop_module_meta->get_method($method_name)->body,
           \&{'Class::MOP::Module::' . $method_name},
           '... Class::MOP::Module->get_method(' . $method_name . ') == &Class::MOP::Module::' . $method_name);
    }
}


# check for imported functions which are not methods

foreach my $non_method_name (qw(
    confess
    blessed
    subname
    svref_2object
    )) {
    ok(!$class_mop_class_meta->has_method($non_method_name), '... NOT Class::MOP::Class->has_method(' . $non_method_name . ')');
}

# check for the right attributes

my @class_mop_package_attributes = (
    'package',
    'namespace',
);

my @class_mop_module_attributes = (
    'version',
    'authority'
);

my @class_mop_class_attributes = (
    'superclasses',
    'instance_metaclass',
    'immutable_trait',
    'constructor_name',
    'constructor_class',
    'destructor_class',
);

# check class

is_deeply(
    [ sort $class_mop_class_meta->get_attribute_list ],
    [ sort @class_mop_class_attributes ],
    '... got the right list of attributes'
);

is_deeply(
    [ sort keys %{$class_mop_class_meta->_attribute_map} ],
    [ sort @class_mop_class_attributes ],
    '... got the right list of attributes');

foreach my $attribute_name (sort @class_mop_class_attributes) {
    ok($class_mop_class_meta->has_attribute($attribute_name), '... Class::MOP::Class->has_attribute(' . $attribute_name . ')');
    isa_ok($class_mop_class_meta->get_attribute($attribute_name), 'Class::MOP::Attribute');
}

# check module

is_deeply(
    [ sort $class_mop_package_meta->get_attribute_list ],
    [ sort @class_mop_package_attributes ],
    '... got the right list of attributes');

is_deeply(
    [ sort keys %{$class_mop_package_meta->_attribute_map} ],
    [ sort @class_mop_package_attributes ],
    '... got the right list of attributes');

foreach my $attribute_name (sort @class_mop_package_attributes) {
    ok($class_mop_package_meta->has_attribute($attribute_name), '... Class::MOP::Package->has_attribute(' . $attribute_name . ')');
    isa_ok($class_mop_package_meta->get_attribute($attribute_name), 'Class::MOP::Attribute');
}

# check package

is_deeply(
    [ sort $class_mop_module_meta->get_attribute_list ],
    [ sort @class_mop_module_attributes ],
    '... got the right list of attributes');

is_deeply(
    [ sort keys %{$class_mop_module_meta->_attribute_map} ],
    [ sort @class_mop_module_attributes ],
    '... got the right list of attributes');

foreach my $attribute_name (sort @class_mop_module_attributes) {
    ok($class_mop_module_meta->has_attribute($attribute_name), '... Class::MOP::Module->has_attribute(' . $attribute_name . ')');
    isa_ok($class_mop_module_meta->get_attribute($attribute_name), 'Class::MOP::Attribute');
}

## check the attributes themselves

# ... package

ok($class_mop_package_meta->get_attribute('package')->has_reader, '... Class::MOP::Class package has a reader');
is(ref($class_mop_package_meta->get_attribute('package')->reader), 'HASH', '... Class::MOP::Class package\'s a reader is { name => sub { ... } }');

ok($class_mop_package_meta->get_attribute('package')->has_init_arg, '... Class::MOP::Class package has a init_arg');
is($class_mop_package_meta->get_attribute('package')->init_arg, 'package', '... Class::MOP::Class package\'s a init_arg is package');

# ... package, but inherited from HasMethods
ok($class_mop_package_meta->find_attribute_by_name('method_metaclass')->has_reader, '... Class::MOP::Package method_metaclass has a reader');
is_deeply($class_mop_package_meta->find_attribute_by_name('method_metaclass')->reader,
   { 'method_metaclass' => \&Class::MOP::Mixin::HasMethods::method_metaclass },
   '... Class::MOP::Package method_metaclass\'s a reader is &method_metaclass');

ok($class_mop_package_meta->find_attribute_by_name('method_metaclass')->has_init_arg, '... Class::MOP::Package method_metaclass has a init_arg');
is($class_mop_package_meta->find_attribute_by_name('method_metaclass')->init_arg,
  'method_metaclass',
  '... Class::MOP::Package method_metaclass\'s init_arg is method_metaclass');

ok($class_mop_package_meta->find_attribute_by_name('method_metaclass')->has_default, '... Class::MOP::Package method_metaclass has a default');
is($class_mop_package_meta->find_attribute_by_name('method_metaclass')->default,
   'Class::MOP::Method',
  '... Class::MOP::Package method_metaclass\'s a default is Class::MOP:::Method');

ok($class_mop_package_meta->find_attribute_by_name('wrapped_method_metaclass')->has_reader, '... Class::MOP::Package wrapped_method_metaclass has a reader');
is_deeply($class_mop_package_meta->find_attribute_by_name('wrapped_method_metaclass')->reader,
   { 'wrapped_method_metaclass' => \&Class::MOP::Mixin::HasMethods::wrapped_method_metaclass },
   '... Class::MOP::Package wrapped_method_metaclass\'s a reader is &wrapped_method_metaclass');

ok($class_mop_package_meta->find_attribute_by_name('wrapped_method_metaclass')->has_init_arg, '... Class::MOP::Package wrapped_method_metaclass has a init_arg');
is($class_mop_package_meta->find_attribute_by_name('wrapped_method_metaclass')->init_arg,
  'wrapped_method_metaclass',
  '... Class::MOP::Package wrapped_method_metaclass\'s init_arg is wrapped_method_metaclass');

ok($class_mop_package_meta->find_attribute_by_name('method_metaclass')->has_default, '... Class::MOP::Package method_metaclass has a default');
is($class_mop_package_meta->find_attribute_by_name('method_metaclass')->default,
   'Class::MOP::Method',
  '... Class::MOP::Package method_metaclass\'s a default is Class::MOP:::Method');


# ... class, but inherited from HasAttributes

ok($class_mop_class_meta->find_attribute_by_name('attributes')->has_reader, '... Class::MOP::Class attributes has a reader');
is_deeply($class_mop_class_meta->find_attribute_by_name('attributes')->reader,
   { '_attribute_map' => \&Class::MOP::Mixin::HasAttributes::_attribute_map },
   '... Class::MOP::Class attributes\'s a reader is &_attribute_map');

ok($class_mop_class_meta->find_attribute_by_name('attributes')->has_init_arg, '... Class::MOP::Class attributes has a init_arg');
is($class_mop_class_meta->find_attribute_by_name('attributes')->init_arg,
  'attributes',
  '... Class::MOP::Class attributes\'s a init_arg is attributes');

ok($class_mop_class_meta->find_attribute_by_name('attributes')->has_default, '... Class::MOP::Class attributes has a default');
is_deeply($class_mop_class_meta->find_attribute_by_name('attributes')->default('Foo'),
         {},
         '... Class::MOP::Class attributes\'s a default of {}');

ok($class_mop_class_meta->find_attribute_by_name('attribute_metaclass')->has_reader, '... Class::MOP::Class attribute_metaclass has a reader');
is_deeply($class_mop_class_meta->find_attribute_by_name('attribute_metaclass')->reader,
   { 'attribute_metaclass' => \&Class::MOP::Mixin::HasAttributes::attribute_metaclass },
  '... Class::MOP::Class attribute_metaclass\'s a reader is &attribute_metaclass');

ok($class_mop_class_meta->find_attribute_by_name('attribute_metaclass')->has_init_arg, '... Class::MOP::Class attribute_metaclass has a init_arg');
is($class_mop_class_meta->find_attribute_by_name('attribute_metaclass')->init_arg,
   'attribute_metaclass',
   '... Class::MOP::Class attribute_metaclass\'s a init_arg is attribute_metaclass');

ok($class_mop_class_meta->find_attribute_by_name('attribute_metaclass')->has_default, '... Class::MOP::Class attribute_metaclass has a default');
is($class_mop_class_meta->find_attribute_by_name('attribute_metaclass')->default,
  'Class::MOP::Attribute',
  '... Class::MOP::Class attribute_metaclass\'s a default is Class::MOP:::Attribute');

# check the values of some of the methods

is($class_mop_class_meta->name, 'Class::MOP::Class', '... Class::MOP::Class->name');
is($class_mop_class_meta->version, $Class::MOP::Class::VERSION, '... Class::MOP::Class->version');

ok($class_mop_class_meta->has_package_symbol('$VERSION'), '... Class::MOP::Class->has_package_symbol($VERSION)');
is(${$class_mop_class_meta->get_package_symbol('$VERSION')},
   $Class::MOP::Class::VERSION,
   '... Class::MOP::Class->get_package_symbol($VERSION)');

is_deeply(
    [ $class_mop_class_meta->superclasses ],
    [ qw/Class::MOP::Module Class::MOP::Mixin::HasAttributes/ ],
    '... Class::MOP::Class->superclasses == [ Class::MOP::Module ]');

is_deeply(
    [ $class_mop_class_meta->class_precedence_list ],
    [ qw/
        Class::MOP::Class
        Class::MOP::Module
        Class::MOP::Package
        Class::MOP::Object
        Class::MOP::Mixin::HasMethods
        Class::MOP::Mixin
        Class::MOP::Mixin::HasAttributes
        Class::MOP::Mixin
    / ],
    '... Class::MOP::Class->class_precedence_list == [ Class::MOP::Class Class::MOP::Module Class::MOP::Package ]');

is($class_mop_class_meta->attribute_metaclass, 'Class::MOP::Attribute', '... got the right value for attribute_metaclass');
is($class_mop_class_meta->method_metaclass, 'Class::MOP::Method', '... got the right value for method_metaclass');
is($class_mop_class_meta->instance_metaclass, 'Class::MOP::Instance', '... got the right value for instance_metaclass');

done_testing;
