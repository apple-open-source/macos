use Test::More tests => 48;
use strict;
use warnings;
use lib 't/lib';
use AccessorGroupsRO;

my $class = AccessorGroupsRO->new;

{
    my $warned = 0;

    local $SIG{__WARN__} = sub {
        if  (shift =~ /DESTROY/i) {
            $warned++;
        };
    };

    $class->mk_group_ro_accessors('warnings', 'DESTROY');

    ok($warned);

    # restore non-accessorized DESTROY
    no warnings;
    *AccessorGroupsRO::DESTROY = sub {};
};

foreach (qw/singlefield multiple1 multiple2/) {
    my $name = $_;
    my $alias = "_${name}_accessor";

    can_ok($class, $name, $alias);

    is($class->$name, undef);
    is($class->$alias, undef);

    # get via name
    $class->{$name} = 'a';
    is($class->$name, 'a');

    # alias gets same as name
    is($class->$alias, 'a');

    # die on set via name/alias
    eval {
        $class->$name('b');
    };
    ok($@ =~ /cannot alter/);

    eval {
        $class->$alias('b');
    };
    ok($@ =~ /cannot alter/);

    # value should be unchanged
    is($class->$name, 'a');
    is($class->$alias, 'a');
};

foreach (qw/lr1 lr2/) {
    my $name = "$_".'name';
    my $alias = "_${name}_accessor";
    my $field = "$_".'field';

    can_ok($class, $name, $alias);
    ok(!$class->can($field));

    is($class->$name, undef);
    is($class->$alias, undef);

    # get via name
    $class->{$field} = 'c';
    is($class->$name, 'c');

    # alias gets same as name
    is($class->$alias, 'c');

    # die on set via name/alias
    eval {
        $class->$name('d');
    };
    ok($@ =~ /cannot alter/);

    eval {
        $class->$alias('d');
    };
    ok($@ =~ /cannot alter/);

    # value should be unchanged
    is($class->$name, 'c');
    is($class->$alias, 'c');
};
