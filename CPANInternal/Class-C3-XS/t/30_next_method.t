#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 5;

BEGIN { use_ok('Class::C3::XS') }

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
    sub hello { 'Diamond_A::hello' }
    sub foo { 'Diamond_A::foo' }       
}
{
    package Diamond_B;
    use base 'Diamond_A';
    sub foo { 'Diamond_B::foo => ' . (shift)->next::method() }       
}
{
    package Diamond_C;
    use base 'Diamond_A';     

    sub hello { 'Diamond_C::hello => ' . (shift)->next::method() }
    sub foo { 'Diamond_C::foo => ' . (shift)->next::method() }   
}
{
    package Diamond_D;
    use base ('Diamond_B', 'Diamond_C');
    
    sub foo { 'Diamond_D::foo => ' . (shift)->next::method() }   
}

is(Diamond_C->hello, 'Diamond_C::hello => Diamond_A::hello', '... method resolved itself as expected');

is(Diamond_C->can('hello')->('Diamond_C'), 
   'Diamond_C::hello => Diamond_A::hello', 
   '... can(method) resolved itself as expected');
   
is(UNIVERSAL::can("Diamond_C", 'hello')->('Diamond_C'), 
   'Diamond_C::hello => Diamond_A::hello', 
   '... can(method) resolved itself as expected');

is(Diamond_D->foo, 
    'Diamond_D::foo => Diamond_B::foo => Diamond_C::foo => Diamond_A::foo', 
    '... method foo resolved itself as expected');
