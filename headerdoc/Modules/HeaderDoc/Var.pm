#! /usr/bin/perl
#
# Class name: Var
# Synopsis: Holds class and instance data members parsed by headerDoc
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2003/09/03 01:47:59 $
# 
# Copyright (c) 1999 Apple Computer, Inc.  All Rights Reserved.
# The contents of this file constitute Original Code as defined in and are
# subject to the Apple Public Source License Version 1.1 (the "License").
# You may not use this file except in compliance with the License.  Please
# obtain a copy of the License at http://www.apple.com/publicsource and
# read it before using this file.
#
# This Original Code and all software distributed under the License are
# distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
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
            ($field =~ s/^availability\s+//) && do {$self->availability($field); last SWITCH;};
            ($field =~ s/^updated\s+//) && do {$self->updated($field); last SWITCH;};
            ($field =~ s/^discussion\s+//) && do {$self->discussion($field); last SWITCH;};
	    my $filename = $HeaderDoc::headerObject->name();
            print "$filename:0:Unknown field: $field\n";
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
    $dec =~ s/\t/ /g;
    $dec =~ s/^\s*//g;
    $dec =~ s/</&lt;/g;
    $dec =~ s/>/&gt;/g;
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
    my $availability = $self->availability();
    my $updated = $self->updated();
    my $desc = $self->discussion();
    my $declaration = $self->declarationInHTML();
    my @fields = $self->fields();
    my $fieldHeading = "Field Descriptions";

    if ($self->can('isFunctionPointer')) {
        if ($self->isFunctionPointer()) {
            $fieldHeading = "Parameter Descriptions";
        }
    }

    # add apple_ref markup

    my $methodType = "var"; # $self->getMethodType($declarationRaw);
    my $methodType = "defn"; # $self->getMethodType($declarationRaw);  

    $contentString .= "<hr>";
    $contentString .= $self->appleref($methodType);
    # "<a name=\"//$apiUIDPrefix/occ/$methodType/$className/$name\"></a>\n";

    $contentString .= "<table border=\"0\"  cellpadding=\"2\" cellspacing=\"2\" width=\"300\">";
    $contentString .= "<tr>";
    $contentString .= "<td valign=\"top\" height=\"12\" colspan=\"5\">";
    $contentString .= "<h2><a name=\"$name\">$name</a></h2>\n";
    $contentString .= "</td>";
    $contentString .= "</tr></table>";
    $contentString .= "<hr>";
    if (length($abstract)) {
        # $contentString .= "<b>Abstract:</b> $abstract<br>\n";
        $contentString .= "$abstract\n";
    }
    if (length($availability)) {
        $contentString .= "<b>Availability:</b> $availability<br>\n";
    }
    if (length($updated)) {
        $contentString .= "<b>Updated:</b> $updated<br>\n";
    }
    $contentString .= "<blockquote>$declaration</blockquote>\n";
    if (length($desc)) {$contentString .= "<h5><font face=\"Lucida Grande,Helvetica,Arial\">Discussion</font></h5><p>$desc</p>\n"; }
    # $contentString .= "<p>$desc</p>\n";
    my $arrayLength = @fields;
    if ($arrayLength > 0) {
        $contentString .= "<h5><font face=\"Lucida Grande,Helvetica,Arial\">$fieldHeading</font></h5>\n";
        $contentString .= "<blockquote>\n";

	# DAG Not converting table to definition list because this code can never be
	# called unless something subclasses Var without overriding this method or unless
	# someone adds code to parse an @field or @param in the @var tag.

        $contentString .= "<table border=\"1\"  width=\"90%\">\n";
        $contentString .= "<thead><tr><th>Name</th><th>Description</th></tr></thead>\n";
        foreach my $element (@fields) {
            my $fName;
            my $fDesc;
            $element =~ s/^\s+|\s+$//g;
            $element =~ /(\w*)\s*(.*)/;
            $fName = $1;
            $fDesc = $2;
            $contentString .= "<tr><td align=\"center\"><tt>$fName</tt></td><td>$fDesc</td></tr>\n";
        }
        $contentString .= "</table>\n</blockquote>\n";
    }
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
    my @fields = $self->fields();
    my $fieldHeading = "Field Descriptions";
    
    if ($self->can('isFunctionPointer')) {
        if ($self->isFunctionPointer()) {
            $fieldHeading = "Parameter Descriptions";
        }
    }
    
    $contentString .= "<variable id=\"$name\">\n";
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
    my $arrayLength = @fields;
    if ($arrayLength > 0) {
        $contentString .= "<heading>$fieldHeading</heading>\n";
        $contentString .= "<fieldlist>\n";
        foreach my $element (@fields) {
            my $fName;
            my $fDesc;
            $element =~ s/^\s+|\s+$//g;
            $element =~ /(\w*)\s*(.*)/;
            $fName = $1;
            $fDesc = $2;
            $contentString .= "<field><name>$fName</name><description>$fDesc</description></field>\n";
        }
        $contentString .= "</fieldlist\n";
    }
    $contentString .= "</variable>\n";
    return $contentString;
}


sub printObject {
    my $self = shift;
 
    print "Var\n";
    $self->SUPER::printObject();
    print "\n";
}

1;

