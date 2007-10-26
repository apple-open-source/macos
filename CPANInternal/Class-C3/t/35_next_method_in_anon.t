#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 3;

BEGIN {
    use lib 'opt', '../opt', '../blib/lib';    
    use_ok('c3');
}

=pod

This tests the successful handling of a next::method call from within an
anonymous subroutine.

=cut

{
    package A;
    use c3; 

    sub foo {
      return 'A::foo';
    }

    sub bar {
      return 'A::bar';
    }
}

{
    package B;
    use base 'A';
    use c3; 
    
    sub foo {
      my $code = sub {
        return 'B::foo => ' . (shift)->next::method();
      };
      return (shift)->$code;
    }

    sub bar {
      my $code1 = sub {
        my $code2 = sub {
          return 'B::bar => ' . (shift)->next::method();
        };
        return (shift)->$code2;
      };
      return (shift)->$code1;
    }
}

Class::C3::initialize();  

is(B->foo, "B::foo => A::foo",
   'method resolved inside anonymous sub');

is(B->bar, "B::bar => A::bar",
   'method resolved inside nested anonymous subs');


