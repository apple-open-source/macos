use strict;
use warnings;
use Test::More;
use Test::Exception;

require Class::MOP;
use lib 't/lib';

dies_ok {
    Class::MOP::is_class_loaded()
} "is_class_loaded with no argument dies";

ok(!Class::MOP::is_class_loaded(''), "can't load the empty class");
ok(!Class::MOP::is_class_loaded(\"foo"), "can't load a class name reference??");

ok(!Class::MOP::_is_valid_class_name(undef), 'undef is not a valid class name');
ok(!Class::MOP::_is_valid_class_name(''), 'empty string is not a valid class name');
ok(!Class::MOP::_is_valid_class_name(\"foo"), 'a reference is not a valid class name');
ok(!Class::MOP::_is_valid_class_name('bogus name'), q{'bogus name' is not a valid class name});
ok(Class::MOP::_is_valid_class_name('Foo'), q{'Foo' is a valid class name});
ok(Class::MOP::_is_valid_class_name('Foo::Bar'), q{'Foo::Bar' is a valid class name});
ok(Class::MOP::_is_valid_class_name('Foo_::Bar2'), q{'Foo_::Bar2' is a valid class name});
throws_ok { Class::MOP::load_class('bogus name') } qr/Invalid class name \(bogus name\)/;

throws_ok {
    Class::MOP::load_class('__PACKAGE__')
} qr/__PACKAGE__\.pm.*\@INC/, 'errors sanely on __PACKAGE__.pm';

Class::MOP::load_class('BinaryTree');
can_ok('BinaryTree' => 'traverse');

do {
    package Class;
    sub method {}
};


{
    local $@;
    eval { Class::MOP::load_class('Class') };
    ok( ! $@, 'load_class does not die if the package is already defined' );
}

ok( !Class::MOP::does_metaclass_exist("Class"), "no metaclass for non MOP class" );

throws_ok {
    Class::MOP::load_class('FakeClassOhNo');
}
qr/Can't locate /;

throws_ok {
    Class::MOP::load_class('SyntaxError');
}
qr/Missing right curly/;

throws_ok {
    delete $INC{'SyntaxError.pm'};
    Class::MOP::load_first_existing_class(
        'FakeClassOhNo', 'SyntaxError', 'Class'
    );
}
qr/Missing right curly/,
    'load_first_existing_class does not pass over an existing (bad) module';

throws_ok {
    Class::MOP::load_class('This::Does::Not::Exist');
}
qr{Can't locate This/Does/Not/Exist\.pm in \@INC},
    'load_first_existing_class throws a familiar error for a single module';

{
    package Other;
    use constant foo => "bar";
}

lives_ok {
    ok(Class::MOP::is_class_loaded("Other"), 'is_class_loaded(Other)');
}
"a class with just constants is still a class";

{
    package Lala;
    use metaclass;
}

lives_ok {
    is(Class::MOP::load_first_existing_class("Lala", "Does::Not::Exist"), "Lala", 'load_first_existing_class 1/2 params ok, class name returned');
    is(Class::MOP::load_first_existing_class("Does::Not::Exist", "Lala"), "Lala", 'load_first_existing_class 2/2 params ok, class name returned');
} 'load_classes works';

throws_ok {
    Class::MOP::load_first_existing_class("Does::Not::Exist", "Also::Does::Not::Exist")
} qr/Does::Not::Exist.*Also::Does::Not::Exist/s, 'Multiple non-existant classes cause exception';

{
    sub whatever {
        TestClassLoaded::this_method_does_not_even_exist();
    }

    ok( ! Class::MOP::is_class_loaded('TestClassLoaded'),
        'the mere mention of TestClassLoaded in the whatever sub does not make us think it has been loaded' );
}

{
    require TestClassLoaded::Sub;
    ok( ! Class::MOP::is_class_loaded('TestClassLoaded'),
        'requiring TestClassLoaded::Sub does not make us think TestClassLoaded is loaded' );
}

{
    require TestClassLoaded;
    ok( Class::MOP::is_class_loaded('TestClassLoaded'),
        'We see that TestClassLoaded is loaded after requiring it (it has methods but no $VERSION or @ISA)' );
}

{
    require TestClassLoaded2;
    ok( Class::MOP::is_class_loaded('TestClassLoaded2'),
        'We see that TestClassLoaded2 is loaded after requiring it (it has a $VERSION but no methods or @ISA)' );
}

{
    require TestClassLoaded3;
    ok( Class::MOP::is_class_loaded('TestClassLoaded3'),
        'We see that TestClassLoaded3 is loaded after requiring it (it has an @ISA but no methods or $VERSION)' );
}

{
    {
        package Not::Loaded;
        our @ISA;
    }

    ok( ! Class::MOP::is_class_loaded('Not::Loaded'),
        'the mere existence of an @ISA for a package does not mean a class is loaded' );
}

{
    {
        package Loaded::Ish;
        our @ISA = 'Foo';
    }

    ok( Class::MOP::is_class_loaded('Loaded::Ish'),
        'an @ISA with members does mean a class is loaded' );
}

done_testing;
