package ExtraInheritedGroups;
use strict;
use warnings;
use base 'Class::Accessor::Grouped';

__PACKAGE__->mk_group_accessors('inherited', 'basefield');
__PACKAGE__->set_inherited (basefield => 'your extra base!');

1;
