#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 3;

BEGIN {
    use_ok('Class::C3::XS');
    use_ok('t::lib::F');    
}

=pod 

From the parrot test t/pmc/object-meths.t

 A   B A   E
  \ /   \ /
   C     D
    \   /
     \ /
      F

=cut

is_deeply(
    [ Class::C3::XS::calculateMRO('t::lib::F') ],
    [ qw(t::lib::F t::lib::C t::lib::D t::lib::A t::lib::B t::lib::E) ],
    '... got the right MRO for t::lib::F');  

