#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 2;

BEGIN {
    use_ok('Class::C3');
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
    use Class::C3;
    
    package Y;
    use Class::C3;    
    
    package XY;
    use Class::C3;
    use base ('X', 'Y');
    
    package YX;
    use Class::C3;
    use base ('Y', 'X');
    
    package Z;
    # use Class::C3; << Dont do this just yet ...
    use base ('XY', 'YX');
}

Class::C3::initialize();

eval { 
    # now try to calculate the MRO
    # and watch it explode :)
    Class::C3::calculateMRO('Z') 
};
#diag $@;
like($@, qr/^Inconsistent hierarchy/, '... got the right error with an inconsistent hierarchy');
