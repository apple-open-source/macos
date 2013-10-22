#!/usr/bin/perl -w

use Test::More;
use strict;

BEGIN
   {
   plan tests => 4;
   chdir 't' if -d 't';
   use lib '../lib';
   use_ok ("Heap::Elem::Str") or die($@);
   };

can_ok ("Heap::Elem::Str", qw/
  new
  val
  heap
  cmp
  /);

my $heap = Heap::Elem::Str->new();

like (ref($heap), qr/Heap::Elem::Str/, 'new returned an object');

my $ver = $Heap::Elem::Str::VERSION;
ok ($ver >= 0.80, "Heap::Elem::Str::VERSION >= 0.80 (is: $ver)");
