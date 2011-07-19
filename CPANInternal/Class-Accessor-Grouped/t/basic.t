#!perl -wT
# $Id: basic.t 3252 2007-05-06 02:24:39Z claco $
use strict;
use warnings;

BEGIN {
    use lib 't/lib';
    use Test::More tests => 1;

    use_ok('Class::Accessor::Grouped');
};
