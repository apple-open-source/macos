use strict;
use Test::More qw(no_plan);

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

# coverage tests

{
    # pass a non-code ref and catch the carp

    my @die;

    eval {
	local $SIG{__DIE__} = sub {push @die, @_};

	Foo->add_trigger(wrong_type => []);
    };

    like(pop(@die), qr(add_trigger[(][)] needs coderef at ), 'check for right callback param');
}
{
    # pass a multiple triggers and catch the carp

    my @die;

    eval {
       local $SIG{__DIE__} = sub {push @die, @_};

       Foo->add_trigger(hello => sub{}, world => sub{});
    };

    like(pop(@die), qr(mutiple trigger registration in one add_trigger[(][)] call is deprecated.), 'check for depricated multi-trigger add');
}
