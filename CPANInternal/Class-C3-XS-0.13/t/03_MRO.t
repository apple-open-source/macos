#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 2;

BEGIN {
    use_ok('Class::C3::XS');
}

=pod


This example is take from: http://www.python.org/2.3/mro.html

"My second example"
class O: pass
class F(O): pass
class E(O): pass
class D(O): pass
class C(D,F): pass
class B(E,D): pass
class A(B,C): pass

                           6
                          ---
Level 3                  | O |
                       /  ---  \
                      /    |    \
                     /     |     \
                    /      |      \
                  ---     ---    ---
Level 2        2 | E | 4 | D |  | F | 5
                  ---     ---    ---
                   \      / \     /
                    \    /   \   /
                     \  /     \ /
                      ---     ---
Level 1            1 | B |   | C | 3
                      ---     ---
                       \       /
                        \     /
                          ---
Level 0                0 | A |
                          ---

>>> A.mro()
(<class '__main__.A'>, <class '__main__.B'>, <class '__main__.E'>,
<class '__main__.C'>, <class '__main__.D'>, <class '__main__.F'>,
<type 'object'>)

=cut

{
    package Test::O;
    our @ISA = qw//;
    
    package Test::F;
    use base 'Test::O';
    
    package Test::E;
    use base 'Test::O';
        
    package Test::D;
    use base 'Test::O';    
    
    package Test::C;
    use base ('Test::D', 'Test::F');

    package Test::B;
    use base ('Test::E', 'Test::D');
        
    package Test::A;
    use base ('Test::B', 'Test::C');
}

is_deeply(
    [ Class::C3::XS::calculateMRO('Test::A') ],
    [ qw(Test::A Test::B Test::E Test::C Test::D Test::F Test::O) ],
    '... got the right MRO for Test::A');      
