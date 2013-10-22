#!/usr/bin/perl -w

use Test::More;
use strict;

BEGIN
   {
   plan tests => 4;
   chdir 't' if -d 't';
   use lib '../lib';
   use_ok ("Heap::Elem::RefRev") or die($@);
   };

can_ok ("Heap::Elem::RefRev", qw/
  new
  val
  heap
  cmp
  /);

my $heap = Heap::Elem::RefRev->new();

like (ref($heap), qr/Heap::Elem::RefRev/, 'new returned an object');

my $ver = $Heap::Elem::RefRev::VERSION;
ok ($ver >= 0.80, "Heap::Elem::RefRev::VERSION >= 0.80 (is: $ver)");
