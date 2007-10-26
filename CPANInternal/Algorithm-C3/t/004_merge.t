#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 2;

BEGIN {
    use_ok('Algorithm::C3');
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
    
    sub my_ISA {
        no strict 'refs';
        @{$_[0] . '::ISA'};
    }    
    
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
    [ Algorithm::C3::merge('Vulcan', 'my_ISA') ],
    [ qw(Vulcan Intelligent Sentient Humanoid BiPedal LifeForm Object) ],
    '... got the right C3 merge order for the Vulcan Dylan Example');