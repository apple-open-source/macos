#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 2;

BEGIN {
    use_ok('Algorithm::C3');
}

=pod

This example is take from: http://www.python.org/2.3/mro.html

"Serious order disagreement" # From Guido
class O: pass
class X(O): pass
class Y(O): pass
class A(X,Y): pass
class B(Y,X): pass
try:
    class Z(A,B): pass #creates Z(A,B) in Python 2.2
except TypeError:
    pass # Z(A,B) cannot be created in Python 2.3

=cut

{
    package X;
    
    package Y;
    
    package XY;
    our @ISA = ('X', 'Y');
    
    package YX;
    our @ISA = ('Y', 'X');

    package Z;
    our @ISA = ('XY', 'YX');
}

eval { 
    Algorithm::C3::merge('Z' => sub {
        no strict 'refs';
        @{$_[0] . '::ISA'};
    }) 
};
like($@, qr/^Inconsistent hierarchy/, '... got the right error with an inconsistent hierarchy');
