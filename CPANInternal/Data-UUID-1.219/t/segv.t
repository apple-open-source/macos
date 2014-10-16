#!perl

use strict;
use warnings;

use Test::More tests => 2;

use Data::UUID;

eval {
    Data::UUID->create;
};
like $@, qr{self is not of type Data::UUID};

ok 1;

