#! /usr/bin/perl -w
#
# Class name: MinorAPIElement
# Synopsis: Class for parameters and members of structs, etc. Primary use
#           is to hold info for data export to Inside Mac Database
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2001/11/30 22:43:17 $
#
# Copyright (c) 1999-2001 Apple Computer, Inc.  All Rights Reserved.
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
#
######################################################################

package HeaderDoc::MinorAPIElement;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc convertCharsForFileMaker printArray printHash);
use HeaderDoc::HeaderElement;
@ISA = qw( HeaderDoc::HeaderElement );

use strict;
use vars qw($VERSION @ISA);
$VERSION = '1.20';

sub new {
    my($param) = shift;
    my($class) = ref($param) || $param;
    my $self = {};
    
    bless($self, $class);
    $self->_initialize();
    return ($self);
}

sub _initialize {
    my($self) = shift;
    $self->SUPER::_initialize();
    $self->{POSITION} = undef;
    $self->{TYPE} = undef;
    $self->{USERDICTARRAY} = ();
}

sub position {
    my $self = shift;

    if (@_) {
        $self->{POSITION} = shift;
    }
    return $self->{POSITION};
}

sub type {
    my $self = shift;

    if (@_) {
        $self->{TYPE} = shift;
    }
    return $self->{TYPE};
}

# for miscellaneous data, such as the parameters within a typedef'd struct of callbacks
# stored as an array to preserve order.
sub userDictArray {
    my $self = shift;

    if (@_) {
        @{ $self->{USERDICTARRAY} } = @_;
    }
    ($self->{USERDICTARRAY}) ? return @{ $self->{USERDICTARRAY} } : return ();
}

sub addToUserDictArray {
    my $self = shift;

    if (@_) {
        foreach my $item (@_) {
            push (@{ $self->{USERDICTARRAY} }, $item);
        }
    }
    return @{ $self->{USERDICTARRAY} };
}


sub addKeyAndValueInUserDict {
    my $self = shift;
    
    if (@_) {
        my $key = shift;
        my $value = shift;
        $self->{USERDICT}{$key} = $value;
    }
    return %{ $self->{USERDICT} };
}

sub printObject {
    my $self = shift;
    my $dec = $self->declaration();
 
    print "position: $self->{POSITION}\n";
    print "type: $self->{TYPE}\n";
}

1;
