#! /usr/bin/perl
#
# Class name: Typedef
# Synopsis: Holds typedef info parsed by headerDoc
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2001/11/30 22:43:18 $
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
package HeaderDoc::Typedef;

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
    $self->{RESULT} = undef;
    $self->{FIELDS} = ();
    $self->{ISFUNCPTR} = 0;
    $self->{ISENUMLIST} = 0;
}

sub result {
    my $self = shift;
    
    if (@_) {
        $self->{RESULT} = shift;
    }
    return $self->{RESULT};
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

sub isFunctionPointer {
    my $self = shift;

    if (@_) {
        $self->{ISFUNCPTR} = shift;
    }
    return $self->{ISFUNCPTR};
}

sub isEnumList {
    my $self = shift;

    if (@_) {
        $self->{ISENUMLIST} = shift;
    }
    return $self->{ISENUMLIST};
}


sub processTypedefComment {
    my $self = shift;
    my $fieldArrayRef = shift;
    my @fields = @$fieldArrayRef;
    my $lastField = scalar(@fields);
    my $fieldCounter = 0;
    my $localDebug = 0;
    
    while ($fieldCounter < $lastField) {
        my $field = $fields[$fieldCounter];
        SWITCH: {
            ($field =~ /^\/\*\!/)&& do {last SWITCH;}; # ignore opening /*!
            ($field =~ s/^typedef\s+//) && 
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
                    $fObj->type("field");
                    $self->addField($fObj);
                    print "Adding field for typedef.  Field name: $fName.\n" if ($localDebug);
                    last SWITCH;
                };
            ($field =~ s/^constant\s+//) &&
                do {
                    $self->isEnumList(1);
                    $field =~ s/^\s+|\s+$//g;
                    $field =~ /(\w*)\s*(.*)/s;
                    my $fName = $1;
                    my $fDesc = $2;
                    my $fObj = HeaderDoc::MinorAPIElement->new();
                    $fObj->name($fName);
                    $fObj->discussion($fDesc);
                    $fObj->type("constant");
                    $self->addField($fObj);
                    print "Adding constant for enum.  Constant name: $fName.\n" if ($localDebug);
                    last SWITCH;
                };
            # To handle callbacks and their params and results, have to set up loop
            ($field =~ s/^callback\s+//) &&
                do {
                    $field =~ s/^\s+|\s+$//g;
                    $field =~ /(\w*)\s*(.*)/s;
                    my $cbName = $1;
                    my $cbDesc = $2;
                    my $callbackObj = HeaderDoc::MinorAPIElement->new();
                    $callbackObj->name($cbName);
                    $callbackObj->discussion($cbDesc);
                    $callbackObj->type("callback");
                    # now get params and result that go with this callback
                    print "Adding callback.  Callback name: $cbName.\n" if ($localDebug);
                    $fieldCounter++;
                    while ($fieldCounter < $lastField) {
                        my $nextField = $fields[$fieldCounter];
                        print "In callback: next field is '$nextField'\n" if ($localDebug);
                        
                        if ($nextField =~ s/^param\s+//) {
                            $nextField =~ s/^\s+|\s+$//g;
                            $nextField =~ /(\w*)\s*(.*)/s;
                            my $paramName = $1;
                            my $paramDesc = $2;
                            $callbackObj->addToUserDictArray({"$paramName" => "$paramDesc"});
                        } elsif ($nextField eq "result") {
                            $nextField =~ s/^\s+|\s+$//g;
                            $nextField =~ /(\w*)\s*(.*)/s;
                            my $resultName = $1;
                            my $resultDesc = $2;
                            $callbackObj->addToUserDictArray({"$resultName" => "$resultDesc"});
                        } else {
                            last;
                        }
                        $fieldCounter++;
                    }
                    $self-> addField($callbackObj);
                    print "Adding callback to typedef.  Callback name: $cbName.\n" if ($localDebug);
                    last SWITCH;
                };
            # param and result have to come last, since they should be handled differently, if part of a callback
            # which is inside a struct (as above).  Otherwise, these cases below handle the simple typedef'd callback 
            # (i.e., a typedef'd function pointer without an enclosing struct.
            ($field =~ s/^param\s+//) && 
                do {
                    $self->isFunctionPointer(1);
                    $field =~ s/^\s+|\s+$//g;
                    $field =~ /(\w*)\s*(.*)/s;
                    my $fName = $1;
                    my $fDesc = $2;
                    my $fObj = HeaderDoc::MinorAPIElement->new();
                    $fObj->name($fName);
                    $fObj->discussion($fDesc);
                    $fObj->type("funcPtr");
                    $self->addField($fObj);
                    print "Adding param for function-pointer typedef.  Param name: $fName.\n" if ($localDebug);
                    last SWITCH;
                };
            ($field =~ s/^result\s+//) && 
                do {
                    $self->isFunctionPointer(1);
                    $self->result($field);
                    last SWITCH;
                };
            print "Unknown field: $field\n";
        }
        $fieldCounter++;
    }
}

sub setTypedefDeclaration {
    my $self = shift;
    my $dec = shift;
    my $decType;
    my $localDebug = 0;
    
    print "============================================================================\n" if ($localDebug);
    print "Raw declaration is: $dec\n" if ($localDebug);
    
    if ($dec =~/{/) { # typedef'd struct of anything
        $decType = "struct";
    } else {          # simple function pointer
        $decType = "funcPtr";
    }
    
    if ($decType eq "struct") {
        print "processing struct-like typedef\n" if ($localDebug); 
	    $dec =~ s/[ \t]+/  /g;
	    if (length ($dec)) {$dec = "<pre>\n$dec</pre>\n";};
	} elsif ($decType eq "funcPtr") {
        print "processing funcPtr-like typedef\n" if ($localDebug); 
		if ($dec =~ /^EXTERN_API_C/) {
	        $dec =~ s/^EXTERN_API_C\(\s*(\w+)\s*\)(.*)/$1 $2/;
	    }
	    my $preOpeningParen = $dec;
	    $preOpeningParen =~ s/^\s+(.*)/$1/; # remove leading whitespace
	    $preOpeningParen =~ s/(\w[^(]+)\(([^)]+)\)\s*;/$1/;
	    	    
	    my $withinParens = $2;
	    my @preParenParts = split ('\s+', $preOpeningParen);
	    # &printArray(@preParenParts);
	    my $funcName = pop @preParenParts;
	    my $return = join (' ', @preParenParts);
	    my $remainder = $withinParens;
	    my @parensElements = split(/,/, $remainder);
	    
	    # eliminate this, unless we want to format args and their types separately
# 	    my @paramNames;
# 	    foreach my $element (@parensElements) {
# 	        my @paramElements = split(/\s+/, $element);
# 	        my $paramName = $paramElements[$#paramElements];
# 	        if ($paramName ne "void") { # some programmers write myFunc(void)
# 	            push(@paramNames, $paramName);
# 	        }
# 	    }
	    $dec = "<tt>$return <b>$funcName</b>($remainder);</tt><br>\n";
	}
    print "Typedef: returning declaration:\n\t|$dec|\n" if ($localDebug);
    print "============================================================================\n" if ($localDebug);
    $self->declarationInHTML($dec);
    return $dec;
}

sub documentationBlock {
    my $self = shift;
    my $name = $self->name();
    my $abstract = $self->abstract();
    my $desc = $self->discussion();
    my $result = $self->result();
    my $declaration = $self->declarationInHTML();
    my @fields = $self->fields();
    my $fieldHeading;
    my $contentString;
    my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();
    
    SWITCH: {
        if ($self->isFunctionPointer()) {$fieldHeading = "Parameters"; last SWITCH; }
        if ($self->isEnumList()) {$fieldHeading = "Constants"; last SWITCH; }
        $fieldHeading = "Fields";
    }
    
    $contentString .= "<a name=\"//$apiUIDPrefix/c/tdef/$name\"></a>\n"; # apple_ref marker
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
            my $fName = $element->name();
            my $fDesc = $element->discussion();
            my $fType = $element->type();
            
            if (($fType eq 'field') || ($fType eq 'constant') || ($fType eq 'funcPtr')){
                $contentString .= "<tr><td><tt>$fName</tt></td><td>$fDesc</td><tr>\n";
            } elsif ($fType eq 'callback') {
                my @userDictArray = $element->userDictArray(); # contains elements that are hashes of param name to param doc
                my $paramString;
                foreach my $hashRef (@userDictArray) {
                    while (my ($param, $disc) = each %{$hashRef}) {
                        $paramString .= "<dt><b><tt>$param</tt></b></dt>\n<dd>$disc</dd>\n";
                    }
                    if (length($paramString)) {$paramString = "<dl>\n".$paramString."\n<dl>\n";};
                }
                $contentString .= "<tr><td><tt>$fName</tt></td><td>$fDesc<br>$paramString</td><tr>\n";
            } else {
                print "### warning: Typedef field with name $fName has unknown type: $fType\n";
            }
        }
        
        $contentString .= "</table>\n</blockquote>\n";
    }
    if (length($result)) {
        $contentString .= "<b>Result:</b> $result\n";
    }
    $contentString .= "<hr>\n";
    return $contentString;
}


sub printObject {
    my $self = shift;
 
    print "Typedef\n";
    $self->SUPER::printObject();
    SWITCH: {
        if ($self->isFunctionPointer()) {print "Parameters:\n"; last SWITCH; }
        if ($self->isEnumList()) {print "Constants:\n"; last SWITCH; }
        print "Fields:\n";
    }

    my $fieldArrayRef = $self->{FIELDS};
    my $arrayLength = @{$fieldArrayRef};
    if ($arrayLength > 0) {
        &printArray(@{$fieldArrayRef});
    }
    print "is function pointer: $self->{ISFUNCPTR}\n";
    print "is enum list: $self->{ISENUMLIST}\n";
    print "\n";
}

1;

