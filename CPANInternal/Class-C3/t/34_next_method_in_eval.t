#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 2;

BEGIN {
    use lib 'opt', '../opt', '..';    
    use_ok('c3');
}

=pod

This tests the use of an eval{} block to wrap a next::method call.

=cut

{
    package A;
    use c3; 

    sub foo {
      die 'A::foo died';
      return 'A::foo succeeded';
    }
}

{
    package B;
    use base 'A';
    use c3; 
    
    sub foo {
      eval {
        return 'B::foo => ' . (shift)->next::method();
      };

      if ($@) {
        return $@;
      }
    }
}

Class::C3::initialize();  

like(B->foo, 
   qr/^A::foo died/, 
   'method resolved inside eval{}');


