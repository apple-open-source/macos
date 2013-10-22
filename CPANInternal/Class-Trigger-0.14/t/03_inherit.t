use strict;
use Test::More tests => 6;

use IO::Scalar;

use lib 't/lib';
use Foo;
use Foo::Bar;

ok(Foo->add_trigger(before_foo => sub { print "before_foo\n" }),
   'add_trigger in Foo');
ok(Foo::Bar->add_trigger(after_foo  => sub { print "after_foo\n" }),
   'add_trigger in Foo::Bar');
ok(Foo::Bar->add_trigger(before_foo  => sub { print "before_foo2\n" }),
   'add_trigger in Foo::Bar');
ok(Foo->add_trigger(before_foo => sub { print "before_foo3\n" }),
   'add_trigger in Foo');

my $foo = Foo::Bar->new;

{
    tie *STDOUT, 'IO::Scalar', \my $out;
    $foo->foo;
    is $out, "before_foo\nbefore_foo3\nbefore_foo2\nfoo\nafter_foo\n";
}

my $foo_parent = Foo->new;
{
    tie *STDOUT, 'IO::Scalar', \my $out;
    $foo_parent->foo;
    is $out, "before_foo\nbefore_foo3\nfoo\n", 'Foo not affected';
}



