#!/usr/bin/perl -w

use Test::More;
use strict;

BEGIN
   {
   plan tests => 4;
   chdir 't' if -d 't';
   use lib '../lib';
   use_ok ("Heap::Elem::Ref") or die($@);
   };

can_ok ("Heap::Elem::Ref", qw/
  new
  val
  heap
  cmp
  /);

my $heap = Heap::Elem::Ref->new();

like (ref($heap), qr/Heap::Elem::Ref/, 'new returned an object');

my $ver = $Heap::Elem::Ref::VERSION;
ok ($ver >= 0.80, "Heap::Elem::Ref::VERSION >= 0.80 (is: $ver)");
