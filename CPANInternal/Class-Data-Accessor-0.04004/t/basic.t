#!perl -wT
# $Id$
use strict;
use warnings;

BEGIN {
    use lib 't/lib';
    use Test::More tests => 1;

    use_ok('Class::Data::Accessor');
};
