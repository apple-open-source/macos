#!/usr/bin/perl -w

use Test::More;
use strict;

BEGIN
   {
   plan tests => 4;
   chdir 't' if -d 't';
   use lib '../lib';
   use_ok ("Heap::Elem::StrRev") or die($@);
   };

can_ok ("Heap::Elem::StrRev", qw/
  new
  val
  heap
  cmp
  /);

my $heap = Heap::Elem::StrRev->new();

like (ref($heap), qr/Heap::Elem::StrRev/, 'new returned an object');

my $ver = $Heap::Elem::StrRev::VERSION;
ok ($ver >= 0.80, "Heap::Elem::StrRev::VERSION >= 0.80 (is: $ver)");
