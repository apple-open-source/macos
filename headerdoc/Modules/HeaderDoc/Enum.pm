#! /usr/bin/perl -w
#
# Class name: Enum
# Synopsis: Holds struct info parsed by headerDoc
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: 12/9/99
# 
# Copyright (c) 1999 Apple Computer, Inc.  All Rights Reserved.
# The contents of this file constitute Original Code as defined in and are
# subject to the Apple Public Source License Version 1.1 (the "License").
# You may not use this file except in compliance with the License.  Please
# obtain a copy of the License at http://www.apple.com/publicsource and
# read it before using this file.
#
# This Original Code and all software distributed under the License are
# distributed on an TAS ISU basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the License for
# the specific language governing rights and limitations under the
# License.
#
######################################################################
package HeaderDoc::Enum;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc convertCharsForFileMaker printArray printHash);
use HeaderDoc::HeaderElement;
use HeaderDoc::MinorAPIElement;
@ISA = qw( HeaderDoc::HeaderElement );

use strict;
use vars qw($VERSION @ISA);
$VERSION = '1.20';

sub new {
    my($param) = shift;
    my($class) = ref($param) || $param;
    my $self = {};
    
    bless($self, $class);
    $self->SUPER::_initialize();
    $self->_initialize();
    return($self);
}

sub _initialize {
    my($self) = shift;
    $self->{CONSTANTS} = [];
}

sub constants {
    my $self = shift;
    if (@_) { 
        @{ $self->{CONSTANTS} } = @_;
    }
    return @{ $self->{CONSTANTS} };
}

sub addConstant {
    my $self = shift;
    if (@_) { 
        push (@{$self->{CONSTANTS}}, @_);
    }
    return @{ $self->{CONSTANTS} };
}

sub processEnumComment {
    my $self = shift;
    my $fieldArrayRef = shift;
    my @fields = @$fieldArrayRef;
	foreach my $field (@fields) {
		SWITCH: {
            ($field =~ /^\/\*\!/)&& do {last SWITCH;}; # ignore opening /*!
            ($field =~ s/^enum\s+//) && 
            do {
                my ($name, $disc);
                ($name, $disc) = &getAPINameAndDisc($field); 
                $self->name($name);
                if (length($disc)) {$self->discussion($disc);};
                last SWITCH;
            };
            ($field =~ s/^abstract\s+//) && do {$self->abstract($field); last SWITCH;};
            ($field =~ s/^discussion\s+//) && do {$self->discussion($field); last SWITCH;};
            ($field =~ s/^constant\s+//) && 
            do {
				$field =~ s/^\s+|\s+$//g;
	            $field =~ /(\w*)\s+(.*)/s;
	            my $cName = $1;
	            my $cDesc = $2;
	            my $cObj = HeaderDoc::MinorAPIElement->new();
	            $cObj->name($cName);
	            $cObj->discussion($cDesc);
                $self->addConstant($cObj); 
                last SWITCH;
            };
            print "Unknown field: $field\n";
		}
	}
}

sub getEnumDeclaration {
    my $self = shift;
    my $dec = shift;
    my $localDebug = 0;
    
    print "============================================================================\n" if ($localDebug);
    print "Raw declaration is: $dec\n" if ($localDebug);
    
    $dec =~ s/[ \t]+/  /g;
    if (length ($dec)) {$dec = "<pre>\n$dec</pre>\n";};
    
    print "Enum: returning declaration:\n\t|$dec|\n" if ($localDebug);
    print "============================================================================\n" if ($localDebug);
    return $dec;
}

sub printObject {
    my $self = shift;
 
    print "Enum\n";
    $self->SUPER::printObject();
    print "Constants:\n";
    my $fieldArrayRef = $self->{CONSTANTS};
    my $arrayLength = @{$fieldArrayRef};
    if ($arrayLength > 0) {
        &printArray(@{$fieldArrayRef});
    }
    print "\n";
}

1;

