#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 3;

BEGIN {
    use lib 'opt', '../opt', '..';
    use_ok('c3');
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

Class::C3::initialize();

is_deeply(
    [ c3::calculateMRO('t::lib::F') ],
    [ qw(t::lib::F t::lib::C t::lib::D t::lib::A t::lib::B t::lib::E) ],
    '... got the right MRO for t::lib::F');  

