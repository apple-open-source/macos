#! /usr/bin/perl
#
# Class name: Struct
# Synopsis: Holds struct info parsed by headerDoc
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2003/07/29 21:57:54 $
# 
# Copyright (c) 1999-2001 Apple Computer, Inc.  All Rights Reserved.
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

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash);
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
    $self->_initialize();
    return($self);
}

sub _initialize {
    my($self) = shift;
    $self->SUPER::_initialize();
    $self->{ISUNION} = 0;
    $self->{FIELDS} = ();
}

sub isUnion {
    my $self = shift;
    if (@_) {
	$self->{ISUNION} = shift;
    }
    return $self->{ISUNION};
}

sub fields {
    my $self = shift;
    if (@_) { 
        @{ $self->{FIELDS} } = @_;
    }
    ($self->{FIELDS}) ? return @{ $self->{FIELDS} } : return ();
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
            (($field =~ s/^struct\s+//)  || ($field =~ s/^union\s+//))&& 
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
            ($field =~ s/^field\s+//) && 
            do {
				$field =~ s/^\s+|\s+$//g;
	            $field =~ /(\w*)\s*(.*)/s;
	            my $fName = $1;
	            my $fDesc = $2;
	            my $fObj = HeaderDoc::MinorAPIElement->new();
	            $fObj->outputformat($self->outputformat);
	            $fObj->name($fName);
	            $fObj->discussion($fDesc);
	            $self->addField($fObj);
				last SWITCH;
			};
	    my $filename = $HeaderDoc::headerObject->name();
            print "$filename:0:Unknown field: $field\n";
		}
	}
}

sub setStructDeclaration {
    my $self = shift;
    my $dec = shift;
    my $localDebug = 0;
    
    print "============================================================================\n" if ($localDebug);
    print "Raw declaration is: $dec\n" if ($localDebug);
    
    $dec =~ s/\t/  /g;
    $dec =~ s/^\s+(.*)/$1/; # remove leading whitespace
    $dec =~ s/</&lt;/g;
    $dec =~ s/>/&gt;/g;

    my $decline = $dec;
    $decline =~ s/\s*{.*//smg;
    my $endline = $dec;
    $endline =~ s/.*}\s*//smg;
    my $mid = $dec;
    print "mid $mid\n" if ($localDebug);
    # $mid =~ s/{\s*(.*)\s*}.*?/$1/smg;
    $mid =~ s/^$decline.*?{//sm;
    $mid =~ s/}.*?$endline$//sm;
    $mid =~ s/^\n*//smg;
    $mid =~ s/\n+$/\n/smg;
    print "mid $mid\n" if ($localDebug);

    my $newdec = "$decline {\n";

    my @splitlines = split ('\n', $mid);

    foreach my $line (@splitlines) {
	$line =~ s/^\s*//;
	$newdec .= "    ".$line."\n";
    }
    if ("$endline" eq ";") {
	$newdec .= "}".$endline;
    } else {
	$newdec .= "} ".$endline;
    };

    print "new dec is:\n$newdec\n" if ($localDebug);
    $dec = $newdec;

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
    my $availability = $self->availability();
    my $updated = $self->updated();
    my $desc = $self->discussion();
    my $declaration = $self->declarationInHTML();
    my @fields = $self->fields();
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
        $contentString .= "$abstract\n";
    }
    if (length($availability)) {
        $contentString .= "<b>availability:</b> $availability\n";
    }
    if (length($updated)) {
        $contentString .= "<b>updated:</b> $updated\n";
    }
    $contentString .= "<blockquote>$declaration</blockquote>\n";
    # $contentString .= "<p>$desc</p>\n";
    if (length($desc)) {$contentString .= "<h5><font face=\"Lucida Grande,Helvetica,Arial\">Discussion</font></h5><p>$desc</p>\n"; }
    my $arrayLength = @fields;
    if ($arrayLength > 0) {
        $contentString .= "<h5><font face=\"Lucida Grande,Helvetica,Arial\">Field Descriptions</font></h5>\n";
        $contentString .= "<blockquote>\n";
        # $contentString .= "<table border=\"1\"  width=\"90%\">\n";
        # $contentString .= "<thead><tr><th>Name</th><th>Description</th></tr></thead>\n";
	$contentString .= "<dl>\n";
        foreach my $element (@fields) {
            my $fName = $element->name();
            my $fDesc = $element->discussion();
            $contentString .= "<dt><tt>$fName</tt></dt><dd>$fDesc</dd>\n";
        }
        $contentString .= "</dl>\n</blockquote>\n";
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
    my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();

    my $type = "struct";
    if ($self->isUnion()) {
	$type = "union";
    }
    my $uid = "//$apiUIDPrefix/c/tag/$name";
    HeaderDoc::APIOwner->register_uid($uid);
    $contentString .= "<struct id=\"$uid\" type=\"$type\">\n"; # apple_ref marker
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
    my $arrayLength = @fields;
    if ($arrayLength > 0) {
        $contentString .= "<fieldlist>\n";
        foreach my $element (@fields) {
            my $fName = $element->name();
            my $fDesc = $element->discussion();
            $contentString .= "<field><name>$fName</name><description>$fDesc</description></field>\n";
        }
        $contentString .= "</fieldlist>\n";
    }
    $contentString .= "</struct>\n";
    return $contentString;
}

sub printObject {
    my $self = shift;
 
    print "Struct\n";
    $self->SUPER::printObject();
    print "Field Descriptions:\n";
    my $fieldArrayRef = $self->{FIELDS};
    my $arrayLength = @{$fieldArrayRef};
    if ($arrayLength > 0) {
        &printArray(@{$fieldArrayRef});
    }
    print "\n";
}

1;

