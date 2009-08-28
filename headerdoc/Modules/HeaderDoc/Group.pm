#! /usr/bin/perl -w
#
# Class name: Group
# Synopsis: Holds group info parsed by headerDoc
#
# Last Updated: $Date: 2009/03/30 19:38:50 $
# 
# Copyright (c) 2007 Apple Computer, Inc.  All rights reserved.
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
package HeaderDoc::Group;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash validTag);
use HeaderDoc::HeaderElement;
use HeaderDoc::MinorAPIElement;
use HeaderDoc::APIOwner;

@ISA = qw( HeaderDoc::HeaderElement );

use strict;
use vars qw($VERSION @ISA);
$HeaderDoc::Group::VERSION = '$Revision: 1.5 $';

sub new {
    my($param) = shift;
    my($class) = ref($param) || $param;
    my $self = {};
    
    bless($self, $class);
    $self->_initialize();
    return($self);
}

sub _initialize {
    my($self) = shift;
    
    $self->SUPER::_initialize();
    $self->{CLASS} = "HeaderDoc::Group";
}

sub clone {
    my $self = shift;
    my $clone = undef;
    if (@_) {
	$clone = shift;
    } else {
	$clone = HeaderDoc::Group->new();
    }

    $self->SUPER::clone($clone);

    # now clone stuff specific to enum

    return $clone;
}

sub processComment {
    my $self = shift;
    my $fieldref = shift;

    my @fields = @{$fieldref};

    my $first = 1;
    foreach my $field (@fields) {
	if ($first) { $first = 0; next; }
	SWITCH: {
		($field =~ s/^(group|name|functiongroup|methodgroup)\s+//si) && do {
			my ($name, $desc, $is_nameline_disc) = getAPINameAndDisc($field);

			# Preserve compatibility.  Group names may be multiple words without a discussion.
			if ($is_nameline_disc) { $name .= " ".$desc; $desc = ""; }

			$self->discussion($desc);
		}
	}
    }

}

sub printObject {
    my $self = shift;
 
    print STDERR "Enum\n";
    $self->SUPER::printObject();
    print STDERR "Constants:\n";
}

1;

