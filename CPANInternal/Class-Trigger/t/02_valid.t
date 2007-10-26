use strict;
use Test::More tests => 4;

use lib 't/lib';
use Foo::Valid;			# should be use()

ok(Foo::Valid->add_trigger(before_foo => sub { }));
ok(Foo::Valid->add_trigger(before_foo => sub { }));

eval {
    Foo::Valid->add_trigger(invalid => sub { });
};
like $@, qr/invalid is not valid triggerpoint for Foo::Valid/, $@;

my $foo = Foo::Valid->new;
eval {
    $foo->bar;
};
like $@, qr/invalid is not valid triggerpoint for Foo::Valid/, $@;
