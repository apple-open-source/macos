use Test::More tests => 38;
use strict;
use warnings;
use lib 't/lib';
use AccessorGroupsWO;

my $class = AccessorGroupsWO->new;

{
    my $warned = 0;

    local $SIG{__WARN__} = sub {
        if  (shift =~ /DESTROY/i) {
            $warned++;
        };
    };

    $class->mk_group_wo_accessors('warnings', 'DESTROY');

    ok($warned);

    # restore non-accessorized DESTROY
    no warnings;
    *AccessorGroupsWO::DESTROY = sub {};
};

foreach (qw/singlefield multiple1 multiple2/) {
    my $name = $_;
    my $alias = "_${name}_accessor";

    can_ok($class, $name, $alias);

    # set via name
    is($class->$name('a'), 'a');
    is($class->{$name}, 'a');

    # alias sets same as name
    is($class->$alias('b'), 'b');
    is($class->{$name}, 'b');

    # die on get via name/alias
    eval {
        $class->$name;
    };
    ok($@ =~ /cannot access/);

    eval {
        $class->$alias;
    };
    ok($@ =~ /cannot access/);
};

foreach (qw/lr1 lr2/) {
    my $name = "$_".'name';
    my $alias = "_${name}_accessor";
    my $field = "$_".'field';

    can_ok($class, $name, $alias);
    ok(!$class->can($field));

    # set via name
    is($class->$name('c'), 'c');
    is($class->{$field}, 'c');

    # alias sets same as name
    is($class->$alias('d'), 'd');
    is($class->{$field}, 'd');

    # die on get via name/alias
    eval {
        $class->$name;
    };
    ok($@ =~ /cannot access/);

    eval {
        $class->$alias;
    };
    ok($@ =~ /cannot access/);
};
