#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 2;

BEGIN {
    use lib 'opt', '../opt', '..';
    use_ok('c3');
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
    use c3;
    
    package LifeForm;
    use c3;
    use base 'Object';
    
    package Sentient;
    use c3;
    use base 'LifeForm';
    
    package BiPedal;
    use c3;    
    use base 'LifeForm';
    
    package Intelligent;
    use c3;    
    use base 'Sentient';
    
    package Humanoid;
    use c3;    
    use base 'BiPedal';
    
    package Vulcan;
    use c3;    
    use base ('Intelligent', 'Humanoid');
}

Class::C3::initialize();

is_deeply(
    [ c3::calculateMRO('Vulcan') ],
    [ qw(Vulcan Intelligent Sentient Humanoid BiPedal LifeForm Object) ],
    '... got the right MRO for the Vulcan Dylan Example');  