#! /usr/bin/perl -w
#
# Class name: 	DocReference
# Synopsis: 	Used by gatherHeaderDoc.pl to hold references to doc 
#		for individual headers and classes
# Last Updated: $Date: 2009/03/30 19:38:50 $
# 
# Copyright (c) 1999-2004 Apple Computer, Inc.  All rights reserved.
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
package HeaderDoc::DocReference;

use strict;
use vars qw($VERSION @ISA);
$HeaderDoc::DocReference::VERSION = '$Revision: 1.4 $';

################ General Constants ###################################
my $debugging = 0;

sub new {
    my($param) = shift;
    my($class) = ref($param) || $param;
    my $self = {};
    
    bless($self, $class);
    $self->_initialize();
    # Now grab any key => value pairs passed in
    my (%attributeHash) = @_;
    foreach my $key (keys(%attributeHash)) {
        my $ucKey = uc($key);
        $self->{$ucKey} = $attributeHash{$key};
    }  
    return ($self);
}

sub _initialize {
    my($self) = shift;
    $self->{OUTPUTFORMAT} = undef;
    $self->{UID} = undef;
    $self->{NAME} = undef;
    $self->{GROUP} = " ";
    $self->{TYPE} = undef; # Header, CPPClass, etc
    $self->{PATH} = undef;
    $self->{LANGUAGE} = "";
}

sub path {
    my $self = shift;

    if (@_) {
        $self->{PATH} = shift;
    }
    return $self->{PATH};
}


sub language {
    my $self = shift;

    if (@_) {
        $self->{LANGUAGE} = shift;
    }
    return $self->{LANGUAGE};
}


sub outputformat {
    my $self = shift;

    if (@_) {
        $self->{OUTPUTFORMAT} = shift;
    }
    return $self->{OUTPUTFORMAT};
}


sub uid {
    my $self = shift;

    if (@_) {
        $self->{UID} = shift;
    }
    return $self->{UID};
}


sub name {
    my $self = shift;

    if (@_) {
        $self->{NAME} = shift;
    }
    return $self->{NAME};
}


sub group {
    my $self = shift;

    if (@_) {
	my $newgroupname = shift;
	if (!length($newgroupname)) { $newgroupname = " "; }
        $self->{GROUP} = $newgroupname;
    }
    return $self->{GROUP};
}


sub shortname {
    my $self = shift;

    if (@_) {
        $self->{SHORTNAME} = shift;
    }
    return $self->{SHORTNAME};
}


sub type {
    my $self = shift;

    if (@_) {
        $self->{TYPE} = shift;
    }
    return $self->{TYPE};
}


sub printObject {
    my $self = shift;
 
    print STDERR "----- DocReference Object ------\n";
    print STDERR "uid:  $self->{UID}\n";
    print STDERR "name: $self->{NAME}\n";
    print STDERR "type: $self->{TYPE}\n";
    print STDERR "path: $self->{PATH}\n";
    print STDERR "language: $self->{LANGUAGE}\n";
    print STDERR "\n";
}

1;
