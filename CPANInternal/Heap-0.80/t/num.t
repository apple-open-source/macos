#!/usr/bin/perl -w

use Test::More;
use strict;

BEGIN
   {
   plan tests => 4;
   chdir 't' if -d 't';
   use lib '../lib';
   use_ok ("Heap::Elem::Num") or die($@);
   };

can_ok ("Heap::Elem::Num", qw/
  new
  val
  heap
  cmp
  /);

my $heap = Heap::Elem::Num->new();

like (ref($heap), qr/Heap::Elem::Num/, 'new returned an object');

my $ver = $Heap::Elem::Num::VERSION;
ok ($ver >= 0.80, "Heap::Elem::Num::VERSION >= 0.80 (is: $ver)");
