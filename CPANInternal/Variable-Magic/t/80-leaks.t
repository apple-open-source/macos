#!perl -T

use strict;
use warnings;

use Test::More tests => 11;

use Variable::Magic qw/wizard cast getdata/;

our $destroyed;

{
 package Variable::Magic::TestDestructor;

 sub new { bless { }, shift }

 sub DESTROY { ++$::destroyed }
}

sub D () { 'Variable::Magic::TestDestructor' }

{
 local $destroyed = 0;

 my $w = wizard data => sub { $_[1] };

 {
  my $obj = D->new;

  {
   my $x = 1;
   cast $x, $w, $obj;
   is $destroyed, 0;
  }

  is $destroyed, 0;
 }

 is $destroyed, 1;
}

{
 local $destroyed = 0;

 my $w = wizard data => sub { $_[1] };

 {
  my $copy;

  {
   my $obj = D->new;

   {
    my $x = 1;
    cast $x, $w, $obj;
    is $destroyed, 0;
    $copy = getdata $x, $w;
   }

   is $destroyed, 0;
  }

  is $destroyed, 0;
 }

 is $destroyed, 1;
}

{
 local $destroyed = 0;

 {
  my $obj = D->new;

  {
   my $w  = wizard set => $obj;

   {
    my $x = 1;
    cast $x, $w;
    is $destroyed, 0;
   }

   is $destroyed, 0;
  }

  is $destroyed, 0;
 }

 is $destroyed, 1;
}
