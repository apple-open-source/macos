#! /usr/bin/perl -w
#
# Class name: HeaderElement
# Synopsis: Root class for Function, Typedef, Constant, etc. -- used by HeaderDoc.
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2001/11/30 22:43:17 $
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

package HeaderDoc::HeaderElement;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash);
use strict;
use vars qw($VERSION @ISA);
$VERSION = '1.20';

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
    $self->{ABSTRACT} = undef;
    $self->{DISCUSSION} = undef;
    $self->{DECLARATION} = undef;
    $self->{DECLARATIONINHTML} = undef;
    $self->{NAME} = undef;
    $self->{LINKAGESTATE} = undef;
    $self->{ACCESSCONTROL} = undef;
}

sub name {
    my $self = shift;

    if (@_) {
        my $name = shift;
        $self->{NAME} = $name;
    } else {
    	my $n = $self->{NAME};
		return $n;
	}
}

sub abstract {
    my $self = shift;

    if (@_) {
        $self->{ABSTRACT} = shift;
    }
    return $self->{ABSTRACT};
}


sub discussion {
    my $self = shift;

    if (@_) {
        my $discussion = "";
        $discussion = shift;
        $discussion =~ s/\n\n/<br>\n/g;
        $self->{DISCUSSION} = $discussion;
    }
    return $self->{DISCUSSION};
}


sub declaration {
    my $self = shift;
    my $dec = $self->declarationInHTML();
    # remove simple markup that we add to declarationInHTML
    $dec =~s/<br>/\n/gi;
    $dec =~s/<(\/)?tt>//gi;
    $dec =~s/<(\/)?b>//gi;
    $dec =~s/<(\/)?pre>//gi;
    $self->{DECLARATION} = $dec;  # don't really have to have this ivar
    return $dec;
}

sub declarationInHTML {
    my $self = shift;

    if (@_) {
        $self->{DECLARATIONINHTML} = shift;
    }
    return $self->{DECLARATIONINHTML};
}

sub linkageState {
    my $self = shift;
    
    if (@_) {
        $self->{LINKAGESTATE} = shift;
    }
    return $self->{LINKAGESTATE};
}

sub accessControl {
    my $self = shift;
    
    if (@_) {
        $self->{ACCESSCONTROL} = shift;
    }
    return $self->{ACCESSCONTROL};
}


sub printObject {
    my $self = shift;
    my $dec = $self->declaration();
 
    print "------------------------------------\n";
    print "HeaderElement\n";
    print "name: $self->{NAME}\n";
    print "abstract: $self->{ABSTRACT}\n";
    print "declaration: $dec\n";
    print "declaration in HTML: $self->{DECLARATIONINHTML}\n";
    print "discussion: $self->{DISCUSSION}\n";
    print "linkageState: $self->{LINKAGESTATE}\n";
    print "accessControl: $self->{ACCESSCONTROL}\n\n";
}

1;
