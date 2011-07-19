use strict;
use warnings;

use Test::More;

use metaclass;

# meta classes
{
    package Foo::Meta;
    use base 'Class::MOP::Class';

    package Bar::Meta;
    use base 'Class::MOP::Class';

    package FooBar::Meta;
    use base 'Foo::Meta', 'Bar::Meta';
}

$@ = undef;
eval {
    package Foo;
    metaclass->import('Foo::Meta');
};
ok(!$@, '... Foo.meta => Foo::Meta is compatible') || diag $@;

$@ = undef;
eval {
    package Bar;
    metaclass->import('Bar::Meta');
};
ok(!$@, '... Bar.meta => Bar::Meta is compatible') || diag $@;

$@ = undef;
eval {
    package Foo::Foo;
    use base 'Foo';
    metaclass->import('Bar::Meta');
};
ok($@, '... Foo::Foo.meta => Bar::Meta is not compatible') || diag $@;

$@ = undef;
eval {
    package Bar::Bar;
    use base 'Bar';
    metaclass->import('Foo::Meta');
};
ok($@, '... Bar::Bar.meta => Foo::Meta is not compatible') || diag $@;

$@ = undef;
eval {
    package FooBar;
    use base 'Foo';
    metaclass->import('FooBar::Meta');
};
ok(!$@, '... FooBar.meta => FooBar::Meta is compatible') || diag $@;

$@ = undef;
eval {
    package FooBar2;
    use base 'Bar';
    metaclass->import('FooBar::Meta');
};
ok(!$@, '... FooBar2.meta => FooBar::Meta is compatible') || diag $@;

done_testing;
