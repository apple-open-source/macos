#! /usr/bin/perl -w
#
# Class name: MinorAPIElement
# Synopsis: Class for parameters and members of structs, etc.
#
# Last Updated: $Date: 2009/03/30 19:38:51 $
#
# Copyright (c) 1999-2008 Apple Computer, Inc.  All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
#
# This file contains Original Code and/or Modifications of Original Code
# as defined in and that are subject to the Apple Public Source License
# Version 2.0 (the 'License'). You may not use this file except in
# compliance with the License. Please obtain a copy of the License at
# http://www.opensource.apple.com/apsl/ and read it before using this
# file.
# 
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
# Please see the License for the specific language governing rights and
# limitations under the License.
#
# @APPLE_LICENSE_HEADER_END@
#
######################################################################

package HeaderDoc::MinorAPIElement;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc convertCharsForFileMaker printArray printHash);
use HeaderDoc::HeaderElement;
# use Carp qw(cluck);
@ISA = qw( HeaderDoc::HeaderElement );

use strict;
use vars qw($VERSION @ISA);
$HeaderDoc::MinorAPIElement::VERSION = '$Revision: 1.9 $';

sub new {
    my($param) = shift;
    my($class) = ref($param) || $param;
    my $self = {};
    
    # cluck("Created $self\n");

    bless($self, $class);
    $self->_initialize();
    return ($self);
}

sub _initialize {
    my($self) = shift;
    $self->SUPER::_initialize();
    # $self->{POSITION} = undef;
    # $self->{TYPE} = undef;
    $self->{USERDICTARRAY} = ();
    # $self->{HIDDEN} = 0;
    $self->{CLASS} = "HeaderDoc::MinorAPIElement";
}

sub clone {
    my $self = shift;
    my $clone = undef;
    if (@_) {
	$clone = shift;
    } else {
	$clone = HeaderDoc::MinorAPIElement->new();
    }

    $self->SUPER::clone($clone);

    # now clone stuff specific to function

    $clone->{POSITION} = $self->{POSITION};
    $clone->{TYPE} = $self->{TYPE};
    $clone->{HIDDEN} = $self->{HIDDEN};
    $clone->{USERDICTARRAY} = $self->{USERDICTARRAY};

    return $clone;
}

# Objective-C bits.
sub tagname {
    my $self = shift;

    if (@_) {
        $self->{TAGNAME} = shift;
    }
    return $self->{TAGNAME};
}

sub position {
    my $self = shift;

    if (@_) {
        $self->{POSITION} = shift;
    }
    return $self->{POSITION};
}

sub hidden {
    my $self = shift;

    if (@_) {
        $self->{HIDDEN} = shift;
    }
    return $self->{HIDDEN};
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
 
    $self->SUPER::printObject();
    print STDERR "position: $self->{POSITION}\n";
    print STDERR "type: $self->{TYPE}\n";
}

# /*! @function appleRefIsDoc
#     @param value
#     @abstract Sets or gets a state flag.
#     @discussion The APPLEREFISDOC state flag controls whether to use a
#     language-specific or doc-specific apple_ref marker for a doc block.
#  */
sub appleRefIsDoc
{
    my $self = shift;
    if (@_) {
	my $value = shift;
	$self->{APPLEREFISDOC} = $value;
    }   
	# print STDERR "ARID: ".$self->{APPLEREFISDOC}." for $self\n";
    if ($self->{APPLEREFISDOC}) {
	return $self->{APPLEREFISDOC};
    } elsif ($self->apiOwner()) {
	my $apio = $self->apiOwner();
	bless($apio, "HeaderDoc::HeaderElement");
	bless($apio, $apio->class());
	my $apioval = $apio->appleRefIsDoc();
	# print STDERR "APIOVAL: $apioval for $apio (".$apio->name().")\n";
	if ($apioval) { return $apioval; }
    }
    return $self->{APPLEREFISDOC};
}

1;
