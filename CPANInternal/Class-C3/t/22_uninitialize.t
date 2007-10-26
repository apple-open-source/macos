#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 12;

BEGIN {
    use_ok('Class::C3');
    # uncomment this line, and re-run the
    # test to see the normal p5 dispatch order
    #$Class::C3::TURN_OFF_C3 = 1;
}

=pod

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
    sub goodbye { 'Diamond_C::goodbye' }
    sub hello   { 'Diamond_C::hello'   }
}
{
    package Diamond_D;
    use base ('Diamond_B', 'Diamond_C');
    use Class::C3;    
    
    our @hello = qw(h e l l o);
    our $hello = 'hello';
    our %hello = (h => 1, e => 2, l => "3 & 4", o => 5)
}

Class::C3::initialize();

is(Diamond_D->hello, 'Diamond_C::hello', '... method resolves with the correct MRO');
is(Diamond_D->goodbye, 'Diamond_C::goodbye', '... method resolves with the correct MRO');

{
    no warnings 'redefine';
    no strict 'refs';
    *{"Diamond_D::goodbye"} = sub { 'Diamond_D::goodbye' };
}

is(Diamond_D->goodbye, 'Diamond_D::goodbye', '... method overwritten');

is($Diamond_D::hello, 'hello', '... our SCALAR package vars are here');
is_deeply(
    \@Diamond_D::hello, 
    [ qw(h e l l o) ],
    '... our ARRAY package vars are here');
is_deeply(
    \%Diamond_D::hello, 
    { h => 1, e => 2, l => "3 & 4", o => 5 },
    '... our HASH package vars are here');  

Class::C3::uninitialize();

is(Diamond_D->hello, 'Diamond_A::hello', '... method resolves with reinitialized MRO');
is(Diamond_D->goodbye, 'Diamond_D::goodbye', '... uninitialize does not mess with the manually changed method');

is($Diamond_D::hello, 'hello', '... our SCALAR package vars are still here');
is_deeply(
    \@Diamond_D::hello, 
    [ qw(h e l l o) ],
    '... our ARRAY package vars are still here');
is_deeply(
    \%Diamond_D::hello, 
    { h => 1, e => 2, l => "3 & 4", o => 5 },
    '... our HASH package vars are still here');    

