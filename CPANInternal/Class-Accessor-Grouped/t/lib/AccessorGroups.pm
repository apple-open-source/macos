package AccessorGroups;
use strict;
use warnings;
use base 'Class::Accessor::Grouped';

__PACKAGE__->mk_group_accessors('simple', 'singlefield');
__PACKAGE__->mk_group_accessors('simple', qw/multiple1 multiple2/);
__PACKAGE__->mk_group_accessors('simple', [qw/lr1name lr1field/], [qw/lr2name lr2field/]);
__PACKAGE__->mk_group_accessors('component_class', 'result_class');

sub new {
    return bless {}, shift;
};

1;
