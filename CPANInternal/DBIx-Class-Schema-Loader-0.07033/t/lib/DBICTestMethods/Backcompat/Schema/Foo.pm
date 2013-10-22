package DBICTestMethods::Backcompat::Schema::Foo;

use strict;
use warnings FATAL => 'all';
use English '-no_match_vars';

sub biz {
    my ($self) = @_;
    return 'foo bar biz baz boz noz schnozz';
}

1;
