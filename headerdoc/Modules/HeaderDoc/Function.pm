#! /usr/bin/perl
#
# Class name: Function
# Synopsis: Holds function info parsed by headerDoc
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2001/03/22 02:27:13 $
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
package HeaderDoc::Function;

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
    $self->{RESULT} = undef;
    $self->{TAGGEDPARAMETERS} = [];
    $self->{PARSEDPARAMETERS} = [];
}


sub result {
    my $self = shift;
    
    if (@_) {
        $self->{RESULT} = shift;
    }
    return $self->{RESULT};
}

sub taggedParameters {
    my $self = shift;
    if (@_) { 
        @{ $self->{TAGGEDPARAMETERS} } = @_;
    }
    return @{ $self->{TAGGEDPARAMETERS} };
}

sub addTaggedParameter {
    my $self = shift;
    if (@_) { 
        push (@{$self->{TAGGEDPARAMETERS}}, @_);
    }
    return @{ $self->{TAGGEDPARAMETERS} };
}


sub parsedParameters {
    my $self = shift;
    if (@_) { 
        @{ $self->{PARSEDPARAMETERS} } = @_;
    }
    return @{ $self->{PARSEDPARAMETERS} };
}

sub addParsedParameter {
    my $self = shift;
    if (@_) { 
        push (@{$self->{PARSEDPARAMETERS}}, @_);
    }
    return @{ $self->{PARSEDPARAMETERS} };
}


sub processFunctionComment {
    my $self = shift;
    my $fieldArrayRef = shift;
    my @fields = @$fieldArrayRef;
	foreach my $field (@fields) {
		SWITCH: {
			($field =~ /^\/\*\!/)&& do {last SWITCH;}; # ignore opening /*!
			($field =~ s/^function\s+//) && 
			do {
				my ($name, $disc);
				($name, $disc) = &getAPINameAndDisc($field); 
				$self->name($name);
				if (length($disc)) {$self->discussion($disc);};
				last SWITCH;
			};
			($field =~ s/^abstract\s+//) && do {$self->abstract($field); last SWITCH;};
			($field =~ s/^discussion\s+//) && do {$self->discussion($field); last SWITCH;};
			($field =~ s/^param\s+//) && 
			do {
				$field =~ s/^\s+|\s+$//g; # trim leading and trailing whitespace
	            $field =~ /(\w*)\s*(.*)/s;
	            my $pName = $1;
	            my $pDesc = $2;
	            my $param = HeaderDoc::MinorAPIElement->new();
	            $param->name($pName);
	            $param->discussion($pDesc);
	            $self->addTaggedParameter($param);
				last SWITCH;
			};
			($field =~ s/^result\s+//) && do {$self->result($field); last SWITCH;};
			print "Unknown field: $field\n";
		}
	}
}

sub getAPINameAndDisc {
    my $line = shift;
    my ($name, $disc, $operator);
    # first, get rid of leading space
    $line =~ s/^\s+//;
    ($name, $disc) = split (/\s/, $line, 2);
    if ($name =~ /operator/) {  # this is for operator overloading in C++
        ($operator, $name, $disc) = split (/\s/, $line, 3);
        $name = $operator." ".$name;
    }
    return ($name, $disc);
}

sub setFunctionDeclaration {
    my $self = shift;
    my ($dec) = @_;
    my ($retval);
    my $localDebug = 0;
    
    print "============================================================================\n" if ($localDebug);
    print "Raw declaration is: $dec\n" if ($localDebug);
    
    #catch the case where this is a function-like macro
    if ($dec =~/^#define/) {
        print "returning #define macro with declaration |$dec|\n" if ($localDebug);
        $dec =~ s/\\\n/\\<br>&nbsp;/g;
        return"<tt>$dec</tt><br>\n";
    }
    # regularize whitespace
    $dec =~ s/^\s+(.*)/$1/; # remove leading whitespace
    $dec =~ s/[ \t]+/ /g;
    
    # remove return from parens of EXTERN_API(_C)(retval)
    if ($dec =~ /^EXTERN_API(_C)?/) {
        $dec =~ s/^EXTERN_API(_C)?\(([^)]+)\)(.*)/$2 $3/;
        $dec =~ s/^\s+//;
    }
    # remove CF_EXPORT and find return value
    $dec =~ s/^CF_EXPORT\s+(.*)/$1/;
    # print "   with CF_EXPORT removed: $dec\n" if ($localDebug);
    
    my $preOpeningParen = $dec;
    $preOpeningParen =~ s/^\s+(.*)/$1/; # remove leading whitespace
    $preOpeningParen =~ s/(\w[^(]+)\(([^)]*)\)(.*;[^;]*)$/$1/s;
    my $withinParens = $2;
    my $postParens = $3;
    # print "-->|$preOpeningParen|\n" if ($localDebug);
    
    my @preParenParts = split ('\s+', $preOpeningParen);
    my $funcName = pop @preParenParts;
    my $return = join (' ', @preParenParts);
        
    my $remainder = $withinParens;
    my @parensElements = split(/,/, $remainder);
    
    # now get parameters
    my $position = 1;  
    foreach my $element (@parensElements) {
        $element =~ s/\n/ /g;
        $element =~ s/^\s+//;
        print "element->|$element|\n" if ($localDebug);
        my @paramElements = split(/\s+/, $element);
        my $paramName = pop @paramElements;
        my $type = join (" ", @paramElements);
        
        #test for pointer asterisks and move to type portion of parameter declaration
        if ($paramName =~ /^\*/) {
            $paramName =~ s/^(\*+)(\w+)/$2/;
            $type .= " $1";
        }
        
        if ($paramName ne "void") { # some programmers write myFunc(void)
            my $param = HeaderDoc::MinorAPIElement->new();
            $param->name($paramName);
            $param->position($position);
            $param->type($type);
            $self->addParsedParameter($param);
        }
        $position++;
    }
    $retval = "<tt>$return <b>$funcName</b>($remainder)$postParens</tt><br>\n";
    print "Function: $funcName -- returning declaration:\n\t|$retval|\n" if ($localDebug);
    print "============================================================================\n" if ($localDebug);
    $self->declarationInHTML($retval);
    return $retval;
}


sub documentationBlock {
    my $self = shift;
    my $contentString;
    my $name = $self->name();
    my $desc = $self->discussion();
    my $abstract = $self->abstract();
    my $declaration = $self->declarationInHTML();
    my @params = $self->taggedParameters();
    my $result = $self->result();
    my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();
    
    $contentString .= "<a name=\"//$apiUIDPrefix/c/func/$name\"></a>\n"; # apple_ref marker
    $contentString .= "<h3><a name=\"$name\">$name</a></h3>\n";
    if (length($abstract)) {
        $contentString .= "<b>Abstract:</b> $abstract\n";
    }
    $contentString .= "<blockquote><pre>$declaration</pre></blockquote>\n";
    $contentString .= "<p>$desc</p>\n";
    my $arrayLength = @params;
    if ($arrayLength > 0) {
        my $paramContentString;
        foreach my $element (@params) {
            my $pName = $element->name();
            my $pDesc = $element->discussion();
            if (length ($pName)) {
                $paramContentString .= "<tr><td align = \"center\"><tt>$pName</tt></td><td>$pDesc</td><tr>\n";
            }
        }
        if (length ($paramContentString)){
            $contentString .= "<h4>Parameters</h4>\n";
            $contentString .= "<blockquote>\n";
            $contentString .= "<table border = \"1\"  width = \"90%\">\n";
            $contentString .= "<thead><tr><th>Name</th><th>Description</th></tr></thead>\n";
            $contentString .= $paramContentString;
            $contentString .= "</table>\n</blockquote>\n";
        }
    }
    if (length($result)) {
        $contentString .= "<b>Result:</b> $result\n";
    }
    $contentString .= "<hr>\n";
    return $contentString;
}

sub printObject {
    my $self = shift;
 
    print "Function\n";
    $self->SUPER::printObject();
    print "Result: $self->{RESULT}\n";
    print "Tagged Parameters:\n";
    my $taggedParamArrayRef = $self->{TAGGEDPARAMETERS};
    my $arrayLength = @{$taggedParamArrayRef};
    if ($arrayLength > 0) {
        &printArray(@{$taggedParamArrayRef});
    }
    print "\n";
}

1;

