#! /usr/bin/perl -Tw

use strict;
use warnings;

use Test::More tests => 1;
use Test::Exception;

{
    package MockFooException;
    
    sub new { bless {}, shift };
    sub isa { 
        my ( $self, $class ) = @_;
        return 1 if $class eq 'Foo';
        return $self->SUPER::isa( $class );
    }
}

throws_ok { die MockFooException->new } 'Foo', 
    'Understand exception classes that override isa';