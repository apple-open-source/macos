#! /usr/bin/perl -w
#
# Class name: Enum
# Synopsis: Holds struct info parsed by headerDoc
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
package HeaderDoc::Enum;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash);
use HeaderDoc::HeaderElement;
use HeaderDoc::MinorAPIElement;
use HeaderDoc::APIOwner;

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
    return($self);
}

sub _initialize {
    my($self) = shift;
    
    $self->SUPER::_initialize();
    $self->{CONSTANTS} = ();
}

sub constants {
    my $self = shift;
    if (@_) { 
        @{ $self->{CONSTANTS} } = @_;
    }
    ($self->{CONSTANTS}) ? return @{ $self->{CONSTANTS} } : return ();
}

sub addConstant {
    my $self = shift;
    if (@_) { 
        push (@{$self->{CONSTANTS}}, @_);
    }
    return @{ $self->{CONSTANTS} };
}

sub processEnumComment {
    my $self = shift;
    my $fieldArrayRef = shift;
    my @fields = @$fieldArrayRef;
	foreach my $field (@fields) {
		SWITCH: {
            ($field =~ /^\/\*\!/)&& do {last SWITCH;}; # ignore opening /*!
            ($field =~ s/^enum\s+//) && 
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
            ($field =~ s/^constant\s+//) && 
            do {
				$field =~ s/^\s+|\s+$//g;
	            $field =~ /(\w*)\s*(.*)/s;
	            my $cName = $1;
	            my $cDesc = $2;
	            my $cObj = HeaderDoc::MinorAPIElement->new();
	            $cObj->outputformat($self->outputformat);
	            $cObj->name($cName);
	            $cObj->discussion($cDesc);
                $self->addConstant($cObj); 
		my $name = $self->name();
		if ($name eq "") {
		    $name = "$cName";
		    $self->name($name);
		}
                last SWITCH;
            };
	    my $filename = $HeaderDoc::headerObject->filename();
            print "$filename:0:Unknown field: $field\n";
		}
	}
}

sub getEnumDeclaration {
    my $self = shift;
    my $dec = shift;
    my $localDebug = 0;
    
    print "============================================================================\n" if ($localDebug);
    print "Raw declaration is: $dec\n" if ($localDebug);
    
    $dec =~ s/\t/  /g;
    $dec =~ s/</&lt;/g;
    $dec =~ s/>/&gt;/g;
    if (length ($dec)) {$dec = "<pre>\n$dec</pre>\n";};
    
    print "Enum: returning declaration:\n\t|$dec|\n" if ($localDebug);
    print "============================================================================\n" if ($localDebug);
    return $dec;
}

sub documentationBlock {
    my $self = shift;
    my $name = $self->name();
    my $abstract = $self->abstract();
    my $availability = $self->availability();
    my $updated = $self->updated();
    my $desc = $self->discussion();
    my $declaration = $self->declarationInHTML();
    my @constants = $self->constants();
    my $contentString;
    my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();
    
    $contentString .= "<hr>";
    my $uid = "//$apiUIDPrefix/c/tag/$name";
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
        # $contentString .= "<b>Abstract:</b> $abstract\n";
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
    my $arrayLength = @constants;
    if ($arrayLength > 0) {
        $contentString .= "<h4>Constants</h4>\n";
        $contentString .= "<blockquote>\n";
	$contentString .= "<dl>\n";
        # $contentString .= "<table border=\"1\"  width=\"90%\">\n";
        # $contentString .= "<thead><tr><th>Name</th><th>Description</th></tr></thead>\n";
        foreach my $element (@constants) {
            my $cName = $element->name();
            my $cDesc = $element->discussion();
            my $uid = "//$apiUIDPrefix/c/econst/$cName";
	    HeaderDoc::APIOwner->register_uid($uid);
            # $contentString .= "<tr><td align=\"center\"><a name=\"$uid\"><tt>$cName</tt></a></td><td>$cDesc</td></tr>\n";
            $contentString .= "<dt><a name=\"$uid\"><tt>$cName</tt></a></dt><dd>$cDesc</dd>\n";
        }
        # $contentString .= "</table>\n</blockquote>\n";
        $contentString .= "</dl>\n</blockquote>\n";
    }
    # $contentString .= "<hr>\n";
    return $contentString;
}

sub XMLdocumentationBlock {
    my $self = shift;
    my $name = $self->name();
    my $abstract = $self->abstract();
    my $availability = $self->availability();
    my $updated = $self->updated();
    my $desc = $self->discussion();
    my $declaration = $self->declarationInHTML();
    my @constants = $self->constants();
    my $contentString;
    my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();
    
    my $uid = "//$apiUIDPrefix/c/tag/$name";
    HeaderDoc::APIOwner->register_uid($uid);
    $contentString .= "<enum id=\"$uid\">\n"; # apple_ref marker
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
    my $arrayLength = @constants;
    if ($arrayLength > 0) {
        $contentString .= "<constantlist>\n";
        foreach my $element (@constants) {
            my $cName = $element->name();
            my $cDesc = $element->discussion();
            my $uid = "//$apiUIDPrefix/c/econst/$cName";
	    HeaderDoc::APIOwner->register_uid($uid);
            $contentString .= "<constant id=\"$uid\">\n";
	    $contentString .= "<name>$cName</name>\n";
	    $contentString .= "<description>$cDesc</description>\n";
	    $contentString .= "</constant>\n";
        }
        $contentString .= "</constantlist>\n";
    }
    $contentString .= "</enum>\n";
    return $contentString;
}

sub printObject {
    my $self = shift;
 
    print "Enum\n";
    $self->SUPER::printObject();
    print "Constants:\n";
    my $fieldArrayRef = $self->{CONSTANTS};
    my $arrayLength = @{$fieldArrayRef};
    if ($arrayLength > 0) {
        &printArray(@{$fieldArrayRef});
    }
    print "\n";
}

1;

