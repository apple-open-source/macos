package BaseInheritedGroups;
use strict;
use warnings;
use base 'Class::Accessor::Grouped';

__PACKAGE__->mk_group_accessors('inherited', 'basefield', 'undefined');

sub new {
    return bless {}, shift;
};

1;
