#! /usr/bin/perl
#
# Class name: Struct
# Synopsis: Holds struct info parsed by headerDoc
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2001/06/06 18:02:45 $
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
package HeaderDoc::Struct;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc convertCharsForFileMaker printArray printHash);
use HeaderDoc::HeaderElement;
use HeaderDoc::MinorAPIElement;
use HeaderDoc::APIOwner;

@ISA = qw( HeaderDoc::HeaderElement );

use vars qw($VERSION @ISA);
$VERSION = '1.20';

use strict;


sub new {
    my($param) = shift;
    my($class) = ref($param) || $param;
    my $self = {};
    
    bless($self, $class);
    $self->SUPER::_initialize();
    $self->_initialize();
    return($self);
}

sub _initialize {
    my($self) = shift;
    $self->{FIELDS} = [];
}

sub fields {
    my $self = shift;
    if (@_) { 
        @{ $self->{FIELDS} } = @_;
    }
    return @{ $self->{FIELDS} };
}

sub addField {
    my $self = shift;
    if (@_) { 
        push (@{$self->{FIELDS}}, @_);
    }
    return @{ $self->{FIELDS} };
}

sub processStructComment {
    my $self = shift;
    my $fieldArrayRef = shift;
    my @fields = @$fieldArrayRef;
	foreach my $field (@fields) {
		SWITCH: {
            ($field =~ /^\/\*\!/)&& do {last SWITCH;}; # ignore opening /*!
            ($field =~ s/^struct\s+//) && 
            do {
                my ($name, $disc);
                ($name, $disc) = &getAPINameAndDisc($field); 
                $self->name($name);
                if (length($disc)) {$self->discussion($disc);};
                last SWITCH;
            };
            ($field =~ s/^abstract\s+//) && do {$self->abstract($field); last SWITCH;};
            ($field =~ s/^discussion\s+//) && do {$self->discussion($field); last SWITCH;};
            ($field =~ s/^field\s+//) && 
            do {
				$field =~ s/^\s+|\s+$//g;
	            $field =~ /(\w*)\s*(.*)/s;
	            my $fName = $1;
	            my $fDesc = $2;
	            my $fObj = HeaderDoc::MinorAPIElement->new();
	            $fObj->name($fName);
	            $fObj->discussion($fDesc);
	            $self->addField($fObj);
				last SWITCH;
			};
            print "Unknown field: $field\n";
		}
	}
}

sub setStructDeclaration {
    my $self = shift;
    my $dec = shift;
    my $localDebug = 0;
    
    print "============================================================================\n" if ($localDebug);
    print "Raw declaration is: $dec\n" if ($localDebug);
    
    $dec =~ s/[ \t]+/  /g;
    if (length ($dec)) {$dec = "<pre>\n$dec</pre>\n";};
    
    print "Typedef: returning declaration:\n\t|$dec|\n" if ($localDebug);
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
    my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();

    
    $contentString .= "<a name=\"//$apiUIDPrefix/c/tag/$name\"></a>\n"; # apple_ref marker
    $contentString .= "<h3><a name=\"$name\">$name</a></h3>\n";
    if (length($abstract)) {
        $contentString .= "<b>Abstract:</b> $abstract\n";
    }
    $contentString .= "<blockquote>$declaration</blockquote>\n";
    $contentString .= "<p>$desc</p>\n";
    my $arrayLength = @fields;
    if ($arrayLength > 0) {
        $contentString .= "<h4>Fields</h4>\n";
        $contentString .= "<blockquote>\n";
        $contentString .= "<table border = \"1\"  width = \"90%\">\n";
        $contentString .= "<thead><tr><th>Name</th><th>Description</th></tr></thead>\n";
        foreach my $element (@fields) {
            my $fName = $element->name();
            my $fDesc = $element->discussion();
            $contentString .= "<tr><td align = \"center\"><tt>$fName</tt></td><td>$fDesc</td><tr>\n";
        }
        $contentString .= "</table>\n</blockquote>\n";
    }
    $contentString .= "<hr>\n";
    return $contentString;
}

sub printObject {
    my $self = shift;
 
    print "Struct\n";
    $self->SUPER::printObject();
    print "Fields:\n";
    my $fieldArrayRef = $self->{FIELDS};
    my $arrayLength = @{$fieldArrayRef};
    if ($arrayLength > 0) {
        &printArray(@{$fieldArrayRef});
    }
    print "\n";
}

1;

