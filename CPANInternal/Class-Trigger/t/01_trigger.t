use strict;
use Test::More tests => 7;

use IO::Scalar;

use lib 't/lib';
use Foo;

ok(Foo->add_trigger(before_foo => sub { print "before_foo\n" }),
   'add_trigger in Foo');
ok(Foo->add_trigger(after_foo  => sub { print "after_foo\n" }),
   'add_trigger in foo');

my $foo = Foo->new;

{
    my $out;
    tie *STDOUT, 'IO::Scalar', \$out;
    $foo->foo;
    is $out, "before_foo\nfoo\nafter_foo\n";
}

ok(Foo->add_trigger(after_foo  => sub { print "after_foo2\n" }),
   'add_trigger in Foo');

{
    tie *STDOUT, 'IO::Scalar', \my $out;
    $foo->foo;
    is $out, "before_foo\nfoo\nafter_foo\nafter_foo2\n";
}

ok(Foo->add_trigger(after_foo  => sub { print ref $_[0] }),
   'add_trigger in Foo');

{
    tie *STDOUT, 'IO::Scalar', \my $out;
    $foo->foo;
    is $out, "before_foo\nfoo\nafter_foo\nafter_foo2\nFoo", 'class name';
}

