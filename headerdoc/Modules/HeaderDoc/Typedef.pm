#! /usr/bin/perl
#
# Class name: Typedef
# Synopsis: Holds typedef info parsed by headerDoc
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: 12/9/99
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
package HeaderDoc::Typedef;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc convertCharsForFileMaker printArray printHash);
use HeaderDoc::HeaderElement;
use HeaderDoc::MinorAPIElement;
@ISA = qw( HeaderDoc::HeaderElement );

use strict;
use vars qw($VERSION @ISA);
$VERSION = '1.20';


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
    $self->{ISFUNCPTR} = 0;
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

sub isFunctionPointer {
    my $self = shift;

    if (@_) {
        $self->{ISFUNCPTR} = shift;
    }
    return $self->{ISFUNCPTR};
}

sub processTypedefComment {
    my $self = shift;
    my $fieldArrayRef = shift;
    my @fields = @$fieldArrayRef;
    my $localDebug = 0;
    
	foreach my $field (@fields) {
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
	            $field =~ /(\w*)\s+(.*)/s;
	            my $fName = $1;
	            my $fDesc = $2;
	            my $fObj = HeaderDoc::MinorAPIElement->new();
	            $fObj->name($fName);
	            $fObj->discussion($fDesc);
	            $self->addField($fObj);
			    print "Adding field for typedef.  Field name: $fName.\n" if ($localDebug);
				last SWITCH;
			};
            ($field =~ s/^param\s+//) && 
            do {
				$self->isFunctionPointer(1);
				$field =~ s/^\s+|\s+$//g;
	            $field =~ /(\w*)\s+(.*)/s;
	            my $fName = $1;
	            my $fDesc = $2;
	            my $fObj = HeaderDoc::MinorAPIElement->new();
	            $fObj->name($fName);
	            $fObj->discussion($fDesc);
	            $self->addField($fObj);
			    print "Adding param for function-pointer typedef.  Param name: $fName.\n" if ($localDebug);
				last SWITCH;
			};
            print "Unknown field: $field\n";
		}
	}
}

sub setTypedefDeclaration {
    my $self = shift;
    my %defaults = (TYPE=>"struct", DECLARATION=>"");
    my %args = (%defaults, @_);
    my ($decType) = $args{TYPE};
    my ($dec) =$args{DECLARATION};
    my $localDebug = 0;
    
    print "============================================================================\n" if ($localDebug);
    print "Raw declaration is: $dec\n" if ($localDebug);
    
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



sub printObject {
    my $self = shift;
 
    print "Typedef\n";
    $self->SUPER::printObject();
    print "Fields:\n";
    my $fieldArrayRef = $self->{FIELDS};
    my $arrayLength = @{$fieldArrayRef};
    if ($arrayLength > 0) {
        &printArray(@{$fieldArrayRef});
    }
    print "is function pointer: $self->{ISFUNCPTR}\n";
    print "\n";
}

1;

