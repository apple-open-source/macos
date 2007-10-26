#! /usr/bin/perl

use strict;
use warnings;
use Test::More 'no_plan';
use Test::Exception;

{   package Foo;
    use Carp qw( confess );
    sub an_abstract_method { shift->subclass_responsibility; }
    sub subclass_responsibility {
        my $class = shift;
        my $method = (caller(1))[3];
        $method =~ s/.*:://;
        confess( "abstract method '$method' not implemented for $class" );
    }
}

throws_ok { Foo->an_abstract_method }
    qr/abstract method 'an_abstract_method'/, 'RT 11846: throws_ok breaks tests that depend on caller stack: working';
