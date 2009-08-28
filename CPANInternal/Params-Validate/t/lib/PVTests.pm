package PVTests;

use strict;
use warnings;

use Test::More;

# 5.6.0 core dumps all over during the tests
if ( $] == 5.006 )
{
    plan skip_all => '5.6.0 core dumps all over during the tests.';
    exit;
}


1;
