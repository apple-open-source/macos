use strict;
use Test::More tests => 5;

use IO::Scalar;

use lib 't/lib';
use Foo;

ok(Foo->add_trigger(before_foo => sub { print "before_foo\n" }),
   'add_trigger in Foo');

{
    my $foo = Foo->new;
    tie *STDOUT, 'IO::Scalar', \my $out;
    $foo->add_trigger(after_foo => sub { print "after_foo\n" });
    $foo->foo;
    is $out, "before_foo\nfoo\nafter_foo\n";
}

{
    my $foo = Foo->new;
    tie *STDOUT, 'IO::Scalar', \my $out;
    $foo->foo;
    untie *STDOUT;
    is $out, "before_foo\nfoo\n";
}

{
    my $foo = Foo->new;
    tie *STDOUT, 'IO::Scalar', \my $out;
    $foo->add_trigger(after_foo => sub { print "after_foo1\n" });
    $foo->add_trigger(after_foo => sub { print "after_foo2\n" });
    $foo->foo;
    is $out, "before_foo\nfoo\nafter_foo1\nafter_foo2\n";
}

{
    my $foo = Foo->new;
    tie *STDOUT, 'IO::Scalar', \my $out;
    $foo->foo;
    is $out, "before_foo\nfoo\n";
}

