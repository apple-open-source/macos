#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 11;

BEGIN {
    use_ok('Class::C3');
    # uncomment this line, and re-run the
    # test to see the normal p5 dispatch order
    #$Class::C3::TURN_OFF_C3 = 1;    
}

=pod

This tests the classic diamond inheritence pattern.

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

is(Diamond_D->hello, 'Diamond_C::hello', '... method resolved itself as expected');

is(Diamond_D->can('hello')->(), 'Diamond_C::hello', '... can(method) resolved itself as expected');
is(UNIVERSAL::can("Diamond_D", 'hello')->(), 'Diamond_C::hello', '... can(method) resolved itself as expected');

# now undo the C3
Class::C3::uninitialize();

is(Diamond_D->hello, 'Diamond_A::hello', '... old method resolution has been restored');

is(Diamond_D->can('hello')->(), 'Diamond_A::hello', '... can(method) resolution has been restored');
is(UNIVERSAL::can("Diamond_D", 'hello')->(), 'Diamond_A::hello', '... can(method) resolution has been restored');

Class::C3::initialize();

is(Diamond_D->hello, 'Diamond_C::hello', '... C3 method restored itself as expected');

is(Diamond_D->can('hello')->(), 'Diamond_C::hello', '... C3 can(method) restored itself as expected');
is(UNIVERSAL::can("Diamond_D", 'hello')->(), 'Diamond_C::hello', '... C3 can(method) restored itself as expected');
