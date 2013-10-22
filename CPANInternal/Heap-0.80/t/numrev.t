#!/usr/bin/perl -w

use Test::More;
use strict;

BEGIN
   {
   plan tests => 4;
   chdir 't' if -d 't';
   use lib '../lib';
   use_ok ("Heap::Elem::NumRev") or die($@);
   };

can_ok ("Heap::Elem::NumRev", qw/
  new
  val
  heap
  cmp
  /);

my $heap = Heap::Elem::NumRev->new();

like (ref($heap), qr/Heap::Elem::NumRev/, 'new returned an object');

my $ver = $Heap::Elem::NumRev::VERSION;
ok ($ver >= 0.80, "Heap::Elem::NumRev::VERSION >= 0.80 (is: $ver)");
