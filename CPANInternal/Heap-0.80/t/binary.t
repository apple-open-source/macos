#!/usr/bin/perl -w

use Test::More;
use strict;

BEGIN
   {
   plan tests => 4;
   chdir 't' if -d 't';
   use lib '../lib';
   use_ok ("Heap::Binary") or die($@);
   };

can_ok ("Heap::Binary", qw/
  new

  absorb
  add
  decrease_key
  delete

  minimum
  top

  extract_top
  extract_minimum


  moveto
  heapup
  heapdown
  /);

my $heap = Heap::Binary->new();

like (ref($heap), qr/Heap::Binary/, 'new returned an object');

my $ver = $Heap::Binary::VERSION;
ok ($ver >= 0.80, "Heap::Binary::VERSION >= 0.80 (is: $ver)");

