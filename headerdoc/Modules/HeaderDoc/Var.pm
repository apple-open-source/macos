#! /usr/bin/perl
#
# Class name: Var
# Synopsis: Holds class and instance data members parsed by headerDoc
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2001/11/30 22:43:18 $
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
package HeaderDoc::Var;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash);
use HeaderDoc::HeaderElement;
use HeaderDoc::Struct;

# making it a subclass of Struct, so that it has the "fields" ivar.
@ISA = qw( HeaderDoc::Struct );
use strict;
use vars qw($VERSION @ISA);
$VERSION = '1.20';

sub processVarComment {
    my($self) = shift;
    my $fieldArrayRef = shift;
    my @fields = @$fieldArrayRef;
	foreach my $field (@fields) {
		SWITCH: {
            ($field =~ /^\/\*\!/)&& do {last SWITCH;}; # ignore opening /*!
            ($field =~ s/^var\s+//) && 
            do {
                my ($name, $disc);
                ($name, $disc) = &getAPINameAndDisc($field); 
                $self->name($name);
                if (length($disc)) {$self->discussion($disc);};
                last SWITCH;
            };
            ($field =~ s/^abstract\s+//) && do {$self->abstract($field); last SWITCH;};
            ($field =~ s/^discussion\s+//) && do {$self->discussion($field); last SWITCH;};
            print "Unknown field: $field\n";
		}
	}
}


sub setVarDeclaration {
    my($self) = shift;
    my ($dec) = @_;
    my $localDebug = 0;
    
    print "============================================================================\n" if ($localDebug);
    print "Raw var declaration is: $dec\n" if ($localDebug);
    
    $dec =~ s/^extern\s+//;
    $dec =~ s/[ \t]+/ /g;
    if (length ($dec)) {$dec = "<pre>\n$dec</pre>\n";};
    print "Var: returning declaration:\n\t|$dec|\n" if ($localDebug);
    print "============================================================================\n" if ($localDebug);
    $self->declarationInHTML($dec);
    return $dec;
}

sub documentationBlock {
    my $self = shift;
    my $contentString;
    my $name = $self->name();
    my $abstract = $self->abstract();
    my $desc = $self->discussion();
    my $declaration = $self->declarationInHTML();
    my @fields = $self->fields();
    my $fieldHeading = "Fields";
    
    if ($self->can('isFunctionPointer')) {
        if ($self->isFunctionPointer()) {
            $fieldHeading = "Parameters";
        }
    }
    
    $contentString .= "<h3><a name=\"$name\">$name</a></h3>\n";
    if (length($abstract)) {
        $contentString .= "<b>Abstract:</b> $abstract\n";
    }
    $contentString .= "<blockquote>$declaration</blockquote>\n";
    $contentString .= "<p>$desc</p>\n";
    my $arrayLength = @fields;
    if ($arrayLength > 0) {
        $contentString .= "<h4>$fieldHeading</h4>\n";
        $contentString .= "<blockquote>\n";
        $contentString .= "<table border = \"1\"  width = \"90%\">\n";
        $contentString .= "<thead><tr><th>Name</th><th>Description</th></tr></thead>\n";
        foreach my $element (@fields) {
            my $fName;
            my $fDesc;
            $element =~ s/^\s+|\s+$//g;
            $element =~ /(\w*)\s*(.*)/;
            $fName = $1;
            $fDesc = $2;
            $contentString .= "<tr><td align = \"center\"><tt>$fName</tt></td><td>$fDesc</td><tr>\n";
        }
        $contentString .= "</table>\n</blockquote>\n";
    }
    $contentString .= "<hr>\n";
    return $contentString;
}


sub printObject {
    my $self = shift;
 
    print "Var\n";
    $self->SUPER::printObject();
    print "\n";
}

1;

