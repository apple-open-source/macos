package NotHashBased;
use strict;
use warnings;
use base 'Class::Accessor::Grouped';

sub new {
    return bless [], shift;
};

__PACKAGE__->mk_group_accessors('inherited', 'killme');

1;
