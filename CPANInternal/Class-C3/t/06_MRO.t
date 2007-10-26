#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 3;

BEGIN {
    use_ok('Class::C3');
    # uncomment this line, and re-run the
    # test to see the normal p5 dispatch order
    #$Class::C3::TURN_OFF_C3 = 1;    
}

=pod

This tests a strange bug found by Matt S. Trout 
while building DBIx::Class. Thanks Matt!!!! 

   <A>
  /   \
<C>   <B>
  \   /
   <D>

=cut

{
    package Diamond_A;
    use Class::C3; 

    sub foo { 'Diamond_A::foo' }
}
{
    package Diamond_B;
    use base 'Diamond_A';
    use Class::C3;     

    sub foo { 'Diamond_B::foo => ' . (shift)->next::method }
}
{
    package Diamond_C;
    use Class::C3;    
    use base 'Diamond_A';     

}
{
    package Diamond_D;
    use base ('Diamond_C', 'Diamond_B');
    use Class::C3;    
    
    sub foo { 'Diamond_D::foo => ' . (shift)->next::method }    
}

Class::C3::initialize();

is_deeply(
    [ Class::C3::calculateMRO('Diamond_D') ],
    [ qw(Diamond_D Diamond_C Diamond_B Diamond_A) ],
    '... got the right MRO for Diamond_D');

is(Diamond_D->foo, 
   'Diamond_D::foo => Diamond_B::foo => Diamond_A::foo', 
   '... got the right next::method dispatch path');
