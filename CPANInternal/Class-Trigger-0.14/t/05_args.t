use strict;
use Test::More tests => 3;

use IO::Scalar;

use lib 't/lib';
use Foo;

ok(Foo->add_trigger(foo => sub { print "foo: $_[1]\n" }),
   'add_trigger in Foo');

{
    my $foo = Foo->new;
    tie *STDOUT, 'IO::Scalar', \my $out;
    $foo->call_trigger(foo => 1);
    is $out, "foo: 1\n";
}
{

    tie *STDOUT, 'IO::Scalar', \my $out;
    Foo->call_trigger(foo => 2);
    is $out, "foo: 2\n";
}

