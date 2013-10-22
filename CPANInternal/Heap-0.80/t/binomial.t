#!/usr/bin/perl -w

use Test::More;
use strict;

BEGIN
   {
   plan tests => 4;
   chdir 't' if -d 't';
   use lib '../lib';
   use_ok ("Heap::Binomial") or die($@);
   };

can_ok ("Heap::Binomial", qw/
  new

  elem
  absorb
  add
  decrease_key
  delete

  minimum
  top

  extract_top
  extract_minimum

  moveto
  link_to
  absorb_children
  self_union_once
  self_union
  /);

my $heap = Heap::Binomial->new();

like (ref($heap), qr/Heap::Binomial/, 'new returned an object');

my $ver = $Heap::Binomial::VERSION;
ok ($ver >= 0.80, "Heap::Binomial::VERSION >= 0.80 (is: $ver)");
