#! /usr/bin/perl -w
#
# Class name: Constant
# Synopsis: Holds constant info parsed by headerDoc
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
package HeaderDoc::Constant;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc convertCharsForFileMaker printArray printHash);
use HeaderDoc::HeaderElement;
use HeaderDoc::APIOwner;

@ISA = qw( HeaderDoc::HeaderElement );

use strict;
use vars qw($VERSION @ISA);
$VERSION = '1.20';

sub processConstantComment {
    my($self) = shift;
    my $fieldArrayRef = shift;
    my @fields = @$fieldArrayRef;
    my $localDebug = 0;

	foreach my $field (@fields) {
    	print "Constant field is |$field|\n" if ($localDebug);
		SWITCH: {
            ($field =~ /^\/\*\!/)&& do {last SWITCH;}; # ignore opening /*!
            ($field =~ s/^const(ant)?\s+//) && 
            do {
                my ($name, $disc);
                ($name, $disc) = &getAPINameAndDisc($field); 
                $self->name($name);
                if (length($disc)) {$self->discussion($disc);};
                last SWITCH;
            };
            ($field =~ s/^abstract\s+//) && do {$self->abstract($field); last SWITCH;};
            ($field =~ s/^discussion\s+//) && do {$self->discussion($field); last SWITCH;};
            print "Unknown field in constant comment: $field\n";
		}
	}
}

sub setConstantDeclaration {
    my($self) = shift;
    my ($dec) = @_;
    my $localDebug = 0;
    
    print "============================================================================\n" if ($localDebug);
    print "Raw constant declaration is: $dec\n" if ($localDebug);
    
    $dec =~ s/^extern\s+//;
    $dec =~ s/[ \t]+/ /g;
    if (length ($dec)) {$dec = "<pre>\n$dec</pre>\n";};
    print "Constant: returning declaration:\n\t|$dec|\n" if ($localDebug);
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
    my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();
    
    $contentString .= "<a name=\"//$apiUIDPrefix/c/data/$name\"></a>\n"; # apple_ref marker
    $contentString .= "<h3><a name=\"$name\">$name</a></h3>\n";
    if (length($abstract)) {
        $contentString .= "<b>Abstract:</b> $abstract\n";
    }
    $contentString .= "<blockquote>$declaration</blockquote>\n";
    $contentString .= "<p>$desc</p>\n";
    $contentString .= "<hr>\n";
    return $contentString;
}

sub printObject {
    my $self = shift;
 
    print "Constant\n";
    $self->SUPER::printObject();
    print "\n";
}

1;

