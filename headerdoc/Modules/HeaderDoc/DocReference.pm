#! /usr/bin/perl -w
#
# Class name: 	DocReference
# Synopsis: 	Used by gatherHeaderDoc.pl to hold references to doc 
#		for individual headers and classes
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2003/01/31 23:54:39 $
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
package HeaderDoc::DocReference;

use strict;
use vars qw($VERSION @ISA);
$VERSION = '1.00';
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
    $self->{NAME} = undef;
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


sub name {
    my $self = shift;

    if (@_) {
        $self->{NAME} = shift;
    }
    return $self->{NAME};
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
 
    print "----- DocReference Object ------\n";
    print "name: $self->{NAME}\n";
    print "type: $self->{TYPE}\n";
    print "path: $self->{PATH}\n";
    print "language: $self->{LANGUAGE}\n";
    print "\n";
}

1;
