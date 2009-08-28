#!perl -wT
# $Id: /local/CPAN/Class-Accessor-Grouped/t/basic.t 20 2007-05-06T02:24:39.381139Z claco  $
use strict;
use warnings;

BEGIN {
    use lib 't/lib';
    use Test::More tests => 1;

    use_ok('Class::Accessor::Grouped');
};
