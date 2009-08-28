use Test::More tests => 8;
use strict;
use warnings;
use lib 't/lib';
use Class::Inspector;
use AccessorGroups;

is(AccessorGroups->result_class, undef);

## croak on set where class can't be loaded and it's a physical class
my $dying = AccessorGroups->new;
eval {
    $dying->result_class('NotReallyAClass');
};
ok($@ =~ /Could not load result_class 'NotReallyAClass'/);
is($dying->result_class, undef);


## don't croak when the class isn't available but not loaded for people
## who create class/packages on the fly
$dying->result_class('JunkiesNeverInstalled');
is($dying->result_class, 'JunkiesNeverInstalled');

ok(!Class::Inspector->loaded('BaseInheritedGroups'));
AccessorGroups->result_class('BaseInheritedGroups');
ok(Class::Inspector->loaded('BaseInheritedGroups'));
is(AccessorGroups->result_class, 'BaseInheritedGroups');

## unset it
AccessorGroups->result_class(undef);
is(AccessorGroups->result_class, undef);