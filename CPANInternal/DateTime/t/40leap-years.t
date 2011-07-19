#!/usr/bin/perl -w

use strict;

use Test::More tests => 8;

use DateTime;


for my $y ( 0, 400, 2000, 2004 )
{
    ok( DateTime->_is_leap_year($y), "$y is a leap year" );
}

for my $y ( 1, 100, 1900, 2133 )
{
    ok( ! DateTime->_is_leap_year($y), "$y is not a leap year" );
}
