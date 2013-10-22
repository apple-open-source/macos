#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 2;

BEGIN { use_ok('Class::C3::XS') }

=pod

This tests the use of an eval{} block to wrap a next::method call.

=cut

{
    package A;

    sub foo {
      die 'A::foo died';
      return 'A::foo succeeded';
    }
}

{
    package B;
    use base 'A';
    
    sub foo {
      eval {
        return 'B::foo => ' . (shift)->next::method();
      };

      if ($@) {
        return $@;
      }
    }
}

like(B->foo, 
   qr/^A::foo died/, 
   'method resolved inside eval{}');


