#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 2;

BEGIN {
    use_ok('Class::C3::XS');
}

=pod

example taken from: L<http://gauss.gwydiondylan.org/books/drm/drm_50.html>

         Object
           ^
           |
        LifeForm 
         ^    ^
        /      \
   Sentient    BiPedal
      ^          ^
      |          |
 Intelligent  Humanoid
       ^        ^
        \      /
         Vulcan

 define class <sentient> (<life-form>) end class;
 define class <bipedal> (<life-form>) end class;
 define class <intelligent> (<sentient>) end class;
 define class <humanoid> (<bipedal>) end class;
 define class <vulcan> (<intelligent>, <humanoid>) end class;

=cut

{
    package Object;    
    our @ISA = qw//;
    
    package LifeForm;
    use base 'Object';
    
    package Sentient;
    use base 'LifeForm';
    
    package BiPedal;
    use base 'LifeForm';
    
    package Intelligent;
    use base 'Sentient';
    
    package Humanoid;
    use base 'BiPedal';
    
    package Vulcan;
    use base ('Intelligent', 'Humanoid');
}

is_deeply(
    [ Class::C3::XS::calculateMRO('Vulcan') ],
    [ qw(Vulcan Intelligent Sentient Humanoid BiPedal LifeForm Object) ],
    '... got the right MRO for the Vulcan Dylan Example');  
