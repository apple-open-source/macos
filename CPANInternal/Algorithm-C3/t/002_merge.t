#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 7;

BEGIN {
    use_ok('Algorithm::C3');
}

=pod

This example is take from: http://www.python.org/2.3/mro.html

"My first example"
class O: pass
class F(O): pass
class E(O): pass
class D(O): pass
class C(D,F): pass
class B(D,E): pass
class A(B,C): pass


                          6
                         ---
Level 3                 | O |                  (more general)
                      /  ---  \
                     /    |    \                      |
                    /     |     \                     |
                   /      |      \                    |
                  ---    ---    ---                   |
Level 2        3 | D | 4| E |  | F | 5                |
                  ---    ---    ---                   |
                   \  \ _ /       |                   |
                    \    / \ _    |                   |
                     \  /      \  |                   |
                      ---      ---                    |
Level 1            1 | B |    | C | 2                 |
                      ---      ---                    |
                        \      /                      |
                         \    /                      \ /
                           ---
Level 0                 0 | A |                (more specialized)
                           ---

=cut

{
    package Test::O;
    
    sub supers {
        no strict 'refs';
        @{$_[0] . '::ISA'};
    }    
    
    package Test::F;   
    use base 'Test::O';        
    
    package Test::E;
    use base 'Test::O';    

    package Test::D;
    use base 'Test::O';     
      
    package Test::C;
    use base ('Test::D', 'Test::F');
        
    package Test::B;    
    use base ('Test::D', 'Test::E');    
        
    package Test::A;    
    use base ('Test::B', 'Test::C');
}

is_deeply(
    [ Algorithm::C3::merge('Test::F', 'supers') ],
    [ qw(Test::F Test::O) ],
    '... got the right C3 merge order for Test::F');

is_deeply(
    [ Algorithm::C3::merge('Test::E', 'supers') ],
    [ qw(Test::E Test::O) ],
    '... got the right C3 merge order for Test::E');    

is_deeply(
    [ Algorithm::C3::merge('Test::D', 'supers') ],
    [ qw(Test::D Test::O) ],
    '... got the right C3 merge order for Test::D');       

is_deeply(
    [ Algorithm::C3::merge('Test::C', 'supers') ],
    [ qw(Test::C Test::D Test::F Test::O) ],
    '... got the right C3 merge order for Test::C'); 

is_deeply(
    [ Algorithm::C3::merge('Test::B', 'supers') ],
    [ qw(Test::B Test::D Test::E Test::O) ],
    '... got the right C3 merge order for Test::B');     

is_deeply(
    [ Algorithm::C3::merge('Test::A', 'supers') ],
    [ qw(Test::A Test::B Test::C Test::D Test::E Test::F Test::O) ],
    '... got the right C3 merge order for Test::A');  
    

