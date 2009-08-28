use Test::More tests => 58;
use strict;
use warnings;
use lib 't/lib';
use AccessorGroups;

my $class = AccessorGroups->new;

{
    my $warned = 0;

    local $SIG{__WARN__} = sub {
        if  (shift =~ /DESTROY/i) {
            $warned++;
        };
    };

    $class->mk_group_accessors('warnings', 'DESTROY');

    ok($warned);

    # restore non-accessorized DESTROY
    no warnings;
    *AccessorGroups::DESTROY = sub {};
};

foreach (qw/singlefield multiple1 multiple2/) {
    my $name = $_;
    my $alias = "_${name}_accessor";

    can_ok($class, $name, $alias);

    is($class->$name, undef);
    is($class->$alias, undef);

    # get/set via name
    is($class->$name('a'), 'a');
    is($class->$name, 'a');
    is($class->{$name}, 'a');

    # alias gets same as name
    is($class->$alias, 'a');

    # get/set via alias
    is($class->$alias('b'), 'b');
    is($class->$alias, 'b');
    is($class->{$name}, 'b');

    # alias gets same as name
    is($class->$name, 'b');
};

foreach (qw/lr1 lr2/) {
    my $name = "$_".'name';
    my $alias = "_${name}_accessor";
    my $field = "$_".'field';

    can_ok($class, $name, $alias);
    ok(!$class->can($field));

    is($class->$name, undef);
    is($class->$alias, undef);

    # get/set via name
    is($class->$name('c'), 'c');
    is($class->$name, 'c');
    is($class->{$field}, 'c');

    # alias gets same as name
    is($class->$alias, 'c');

    # get/set via alias
    is($class->$alias('d'), 'd');
    is($class->$alias, 'd');
    is($class->{$field}, 'd');

    # alias gets same as name
    is($class->$name, 'd');
};
