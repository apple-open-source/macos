#! /usr/bin/perl -w
#
# Class name: Constant
# Synopsis: Holds constant info parsed by headerDoc
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2003/07/29 20:41:19 $
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
            ($field =~ s/^availability\s+//) && do {$self->availability($field); last SWITCH;};
            ($field =~ s/^updated\s+//) && do {$self->updated($field); last SWITCH;};
	    my $filename = $HeaderDoc::headerObject->filename();
            print "$filename:0:Unknown field in constant comment: $field\n";
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
    $dec =~ s/\t/ /g;
    $dec =~ s/</&lt;/g;
    $dec =~ s/>/&gt;/g;
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
    my $availability = $self->availability();
    my $updated = $self->updated();
    my $desc = $self->discussion();
    my $declaration = $self->declarationInHTML();
    my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();
    
    $contentString .= "<hr>";
    my $uid = "//$apiUIDPrefix/c/data/$name";
    HeaderDoc::APIOwner->register_uid($uid);
    $contentString .= "<a name=\"$uid\"></a>\n"; # apple_ref marker
    $contentString .= "<table border=\"0\"  cellpadding=\"2\" cellspacing=\"2\" width=\"300\">";
    $contentString .= "<tr>";
    $contentString .= "<td valign=\"top\" height=\"12\" colspan=\"5\">";
    $contentString .= "<h2><a name=\"$name\">$name</a></h2>\n";
    $contentString .= "</td>";
    $contentString .= "</tr></table>";
    $contentString .= "<hr>";
    if (length($abstract)) {
        # $contentString .= "<b>Abstract:</b> $abstract<br>\n";
        $contentString .= "$abstract<br>\n";
    }
    if (length($availability)) {
        $contentString .= "<b>Availability:</b> $availability<br>\n";
    }
    if (length($updated)) {
        $contentString .= "<b>Updated:</b> $updated<br>\n";
    }
    $contentString .= "<blockquote>$declaration</blockquote>\n";
    # $contentString .= "<p>$desc</p>\n";
    if (length($desc)) {$contentString .= "<h5><font face=\"Lucida Grande,Helvetica,Arial\">Discussion</font></h5><p>$desc</p>\n"; }
    # $contentString .= "<hr>\n";
    return $contentString;
}

sub XMLdocumentationBlock {
    my $self = shift;
    my $contentString;
    my $name = $self->name();
    my $abstract = $self->abstract();
    my $availability = $self->availability();
    my $updated = $self->updated();
    my $desc = $self->discussion();
    my $declaration = $self->declarationInHTML();
    my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();
    
    my $uid = "//$apiUIDPrefix/c/data/$name";
    HeaderDoc::APIOwner->register_uid($uid);
    $contentString .= "<const id=\"$uid\">\n"; # apple_ref marker
    $contentString .= "<name>$name</name>\n";
    if (length($abstract)) {
        $contentString .= "<abstract>$abstract</abstract>\n";
    }
    if (length($availability)) {
        $contentString .= "<availability>$availability</availability>\n";
    }
    if (length($updated)) {
        $contentString .= "<updated>$updated</updated>\n";
    }
    $contentString .= "<declaration>$declaration</declaration>\n";
    $contentString .= "<description>$desc</description>\n";
    $contentString .= "</const>\n";
    return $contentString;
}

sub printObject {
    my $self = shift;
 
    print "Constant\n";
    $self->SUPER::printObject();
    print "\n";
}

1;

