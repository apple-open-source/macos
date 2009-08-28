package AccessorGroupsWO;
use strict;
use warnings;
use base 'Class::Accessor::Grouped';

__PACKAGE__->mk_group_wo_accessors('single', 'singlefield');
__PACKAGE__->mk_group_wo_accessors('multiple', qw/multiple1 multiple2/);
__PACKAGE__->mk_group_wo_accessors('listref', [qw/lr1name lr1field/], [qw/lr2name lr2field/]);

sub new {
    return bless {}, shift;
};

foreach (qw/single multiple listref/) {
    no strict 'refs';

    *{"set_$_"} = \&Class::Accessor::Grouped::set_simple;
};

1;
