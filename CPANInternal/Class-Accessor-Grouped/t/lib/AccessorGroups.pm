package AccessorGroups;
use strict;
use warnings;
use base 'Class::Accessor::Grouped';

__PACKAGE__->mk_group_accessors('single', 'singlefield');
__PACKAGE__->mk_group_accessors('multiple', qw/multiple1 multiple2/);
__PACKAGE__->mk_group_accessors('listref', [qw/lr1name lr1field/], [qw/lr2name lr2field/]);
__PACKAGE__->mk_group_accessors('component_class', 'result_class');

sub new {
    return bless {}, shift;
};

foreach (qw/single multiple listref/) {
    no strict 'refs';

    *{"get_$_"} = \&Class::Accessor::Grouped::get_simple;
    *{"set_$_"} = \&Class::Accessor::Grouped::set_simple;
};

1;
