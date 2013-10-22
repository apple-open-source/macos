#!/usr/bin/perl -w

use Test::More;
use strict;

BEGIN
   {
   plan tests => 4;
   chdir 't' if -d 't';
   use lib '../lib';
   use_ok ("Heap::Fibonacci") or die($@);
   };

can_ok ("Heap::Fibonacci", qw/
  new

  elem
  absorb
  add
  ascending_cut
  decrease_key
  delete
  consolidate
  link_to_left_of
  link_as_parent_of 

  minimum
  top

  extract_top
  extract_minimum
  /);

my $heap = Heap::Fibonacci->new();

like (ref($heap), qr/Heap::Fibonacci/, 'new returned an object');

my $ver = $Heap::Fibonacci::VERSION;
ok ($ver >= 0.80, "Heap::Fibonacci::VERSION >= 0.80 (is: $ver)");
