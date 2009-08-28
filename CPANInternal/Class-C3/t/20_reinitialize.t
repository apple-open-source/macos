#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 5;

BEGIN {
    use_ok('Class::C3');
    # uncomment this line, and re-run the
    # test to see the normal p5 dispatch order
    #$Class::C3::TURN_OFF_C3 = 1;
}

=pod

Start with this:

   <A>
  /   \
<B>   <C>
  \   /
   <D>

=cut

{
    package Diamond_A;
    use Class::C3; 
    sub hello { 'Diamond_A::hello' }
}
{
    package Diamond_B;
    use base 'Diamond_A';
    use Class::C3;        
}
{
    package Diamond_C;
    use Class::C3;    
    use base 'Diamond_A';     
    sub hello { 'Diamond_C::hello' }
}
{
    package Diamond_D;
    use base ('Diamond_B', 'Diamond_C');
    use Class::C3;    
}

Class::C3::initialize();

is_deeply(
    [ Class::C3::calculateMRO('Diamond_D') ],
    [ qw(Diamond_D Diamond_B Diamond_C Diamond_A) ],
    '... got the right MRO for Diamond_D');

=pod

Then change it to this:

<E>   <A>
  \  /   \
   <B>   <C>
     \   /
      <D>

=cut

{
    package Diamond_E;
    use Class::C3;      
    sub hello { 'Diamond_E::hello' }      
}

{
    no strict 'refs';
    unshift @{"Diamond_B::ISA"} => 'Diamond_E';
}

is_deeply(
    [ Class::C3::calculateMRO('Diamond_D') ],
    [ qw(Diamond_D Diamond_B Diamond_E Diamond_C Diamond_A) ],
    '... got the new MRO for Diamond_D');

# Doesn't work with core support, since reinit is not neccesary and the change
#  takes effect immediately
SKIP: {
    skip "This test does not work with a c3-patched perl interpreter", 1
        if $Class::C3::C3_IN_CORE;
    is(Diamond_D->hello, 'Diamond_C::hello', '... method still resolves with old MRO');
}

Class::C3::reinitialize();

is(Diamond_D->hello, 'Diamond_E::hello', '... method resolves with reinitialized MRO');
