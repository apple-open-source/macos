use strict;
use warnings;

use Test::More;
use Test::Exception;

{
    package Foo;
    use metaclass;
    sub foo {}
}

sub check_meta_sanity {
    my ($meta) = @_;
    isa_ok($meta, 'Class::MOP::Class');
    is($meta->name, 'Foo');
    ok($meta->has_method('foo'));
}

can_ok('Foo', 'meta');

my $meta = Foo->meta;
check_meta_sanity($meta);

lives_ok {
    $meta = $meta->reinitialize($meta->name);
};
check_meta_sanity($meta);

lives_ok {
    $meta = $meta->reinitialize($meta);
};
check_meta_sanity($meta);

throws_ok {
    $meta->reinitialize('');
} qr/You must pass a package name or an existing Class::MOP::Package instance/;

throws_ok {
    $meta->reinitialize($meta->new_object);
} qr/You must pass a package name or an existing Class::MOP::Package instance/;

done_testing;
