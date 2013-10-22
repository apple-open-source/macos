use strict;
use Test::More qw(no_plan);

use IO::Scalar;

use lib 't/lib';
use Foo;

ok( Foo->add_trigger(
        name     => 'before_foo',
        callback => sub { print "before_foo\n" }
    ),
    'add_trigger in Foo'
);
ok( Foo->add_trigger(
        callback => sub { print "after_foo\n" },
        name     => 'after_foo', # change the param order to test from hash -> list
    ),
    'add_trigger in foo'
);

my $foo = Foo->new;

{
    my $out;
    tie *STDOUT, 'IO::Scalar', \$out;
    $foo->foo;
    is $out, "before_foo\nfoo\nafter_foo\n";
}

ok( Foo->add_trigger(
        name     => 'after_foo',
        callback => sub { print "after_foo2\n" }
    ),
    'add_trigger in Foo'
);

{
    tie *STDOUT, 'IO::Scalar', \my $out;
    $foo->foo;
    is $out, "before_foo\nfoo\nafter_foo\nafter_foo2\n";
}

ok( Foo->add_trigger(
        name     => 'after_foo',
        callback => sub { print ref $_[0] }
    ),
    'add_trigger in Foo'
);

{
    tie *STDOUT, 'IO::Scalar', \my $out;
    $foo->foo;
    is $out, "before_foo\nfoo\nafter_foo\nafter_foo2\nFoo", 'class name';
}
ok( Foo->add_trigger(
        name      => 'after_foo',
        callback  => sub { print "\ngets_here"; return 'YAY'; },
        abortable => 1
    ),
    'add_trigger in Foo'
);

{
    tie *STDOUT, 'IO::Scalar', \my $out;
    is( $foo->foo, 4, "Success returned" );
    is_deeply ($foo->last_trigger_results->[-1], ['YAY']);
    is $out,
        "before_foo\nfoo\nafter_foo\nafter_foo2\nFoo\ngets_here",
        'class name';
}

ok( Foo->add_trigger(
        name      => 'after_foo',
        callback  => sub { print "\nstopping_after"; return undef; },
        abortable => 1

    ),
    'add_trigger in Foo'
);
ok( Foo->add_trigger(
        name     => 'after_foo',
        callback => sub { print "should not get here" }
    ),
    'add_trigger in Foo'
);

{
    tie *STDOUT, 'IO::Scalar', \my $out;
    is( $foo->foo, undef, "The lat thing we ran was 'stopping_after', then returned failure " );
    is $out,
        "before_foo\nfoo\nafter_foo\nafter_foo2\nFoo\ngets_here\nstopping_after",
        'class name';
    unlike( $out, qr/should not get here/ );
}

# coverage tests

{

    # pass a non-code ref and catch the carp

    my @die;

    eval {
        local $SIG{__DIE__} = sub { push @die, @_ };

        Foo->add_trigger(
            name     => 'wrong_type',
            callback => []
        );
    };

    like(
        pop(@die),
        qr(add_trigger[(][)] needs coderef at ),
        'check for right callback param'
    );
}
