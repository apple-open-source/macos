#!/usr/bin/perl -w

use Test::More;
use strict;

BEGIN
   {
   plan tests => 4;
   chdir 't' if -d 't';
   use lib '../lib';
   use_ok ("Heap::Elem") or die($@);
   };

can_ok ("Heap::Elem", qw/
  new

  val
  heap
  cmp
  /);

my $heap = Heap::Elem->new();

like (ref($heap), qr/Heap::Elem/, 'new returned an object');

my $ver = $Heap::Elem::VERSION;
ok ($ver >= 0.80, "Heap::Elem::VERSION >= 0.80 (is: $ver)");
