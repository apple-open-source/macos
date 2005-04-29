#! /usr/bin/perl
#
# Class name: Struct
# Synopsis: Holds struct info parsed by headerDoc
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2004/10/13 00:09:33 $
# 
# Copyright (c) 1999-2004 Apple Computer, Inc.  All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
#
# This file contains Original Code and/or Modifications of Original Code
# as defined in and that are subject to the Apple Public Source License
# Version 2.0 (the 'License'). You may not use this file except in
# compliance with the License. Please obtain a copy of the License at
# http://www.opensource.apple.com/apsl/ and read it before using this
# file.
# 
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
# Please see the License for the specific language governing rights and
# limitations under the License.
#
# @APPLE_LICENSE_HEADER_END@
#
######################################################################
package HeaderDoc::Struct;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash);
use HeaderDoc::HeaderElement;
use HeaderDoc::MinorAPIElement;
use HeaderDoc::APIOwner;

@ISA = qw( HeaderDoc::HeaderElement );

use vars qw($VERSION @ISA);
$VERSION = '$Revision: 1.11.2.10.2.25 $';

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
    # $self->{ISUNION} = 0;
    $self->{FIELDS} = ();
    $self->{CLASS} = "HeaderDoc::Struct";
}

sub clone {
    my $self = shift;
    my $clone = undef;
    if (@_) {
	$clone = shift;
    } else {
	$clone = HeaderDoc::Struct->new();
    }

    $self->SUPER::clone($clone);

    # now clone stuff specific to function

    $clone->{ISUNION} = $self->{ISUNION};
    $clone->{FIELDS} = $self->{FIELDS};

    return $clone;
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

sub processComment {
    my $self = shift;
    my $fieldArrayRef = shift;
    my $filename = $self->filename();
    my $linenum = $self->linenum();
    my @fields = @$fieldArrayRef;

    my $fieldCounter = 0;
    my $lastField = scalar(@fields);
    my $localDebug = 0;

    while ($fieldCounter < $lastField) {
	my $field = $fields[$fieldCounter];

	print "FIELD WAS $field\n" if ($localDebug);
	SWITCH: {
            ($field =~ /^\/\*\!/o)&& do {
                                my $copy = $field;
                                $copy =~ s/^\/\*\!\s*//s;
                                if (length($copy)) {
                                        $self->discussion($copy);
                                }
                        last SWITCH;
                        };
            (($field =~ s/^struct(\s+)/$1/o)  || ($field =~ s/^union(\s+)/$1/o))&& 
            do {
                my ($name, $disc);
                ($name, $disc) = &getAPINameAndDisc($field); 
                $self->name($name);
                if (length($disc)) {$self->discussion($disc);};
                last SWITCH;
            };
            ($field =~ s/^abstract\s+//o) && do {$self->abstract($field); last SWITCH;};
            ($field =~ s/^discussion\s+//o) && do {$self->discussion($field); last SWITCH;};
            ($field =~ s/^availability\s+//o) && do {$self->availability($field); last SWITCH;};
            ($field =~ s/^since\s+//o) && do {$self->availability($field); last SWITCH;};
            ($field =~ s/^author\s+//o) && do {$self->attribute("Author", $field, 0); last SWITCH;};
	    ($field =~ s/^version\s+//o) && do {$self->attribute("Version", $field, 0); last SWITCH;};
            ($field =~ s/^deprecated\s+//o) && do {$self->attribute("Deprecated", $field, 0); last SWITCH;};
            ($field =~ s/^updated\s+//o) && do {$self->updated($field); last SWITCH;};
	    ($field =~ s/^attribute\s+//o) && do {
		    my ($attname, $attdisc) = &getAPINameAndDisc($field);
		    if (length($attname) && length($attdisc)) {
			$self->attribute($attname, $attdisc, 0);
		    } else {
			warn "$filename:$linenum:Missing name/discussion for attribute\n";
		    }
		    last SWITCH;
		};
	    ($field =~ s/^attributelist\s+//o) && do {
		    $field =~ s/^\s*//so;
		    $field =~ s/\s*$//so;
		    my ($name, $lines) = split(/\n/, $field, 2);
		    $name =~ s/^\s*//so;
		    $name =~ s/\s*$//so;
		    $lines =~ s/^\s*//so;
		    $lines =~ s/\s*$//so;
		    if (length($name) && length($lines)) {
			my @attlines = split(/\n/, $lines);
			foreach my $line (@attlines) {
			    $self->attributelist($name, $line);
			}
		    } else {
			warn "$filename:$linenum:Missing name/discussion for attributelist\n";
		    }
		    last SWITCH;
		};
	    ($field =~ s/^attributeblock\s+//o) && do {
		    my ($attname, $attdisc) = &getAPINameAndDisc($field);
		    if (length($attname) && length($attdisc)) {
			$self->attribute($attname, $attdisc, 1);
		    } else {
			warn "$filename:$linenum:Missing name/discussion for attributeblock\n";
		    }
		    last SWITCH;
		};
	    ($field =~ /^see(also|)\s+/o) &&
		do {
		    $self->see($field);
		    last SWITCH;
		};
            ($field =~ s/^field\s+//o) && 
            do {
				$field =~ s/^\s+|\s+$//go;
	            $field =~ /(\w*)\s*(.*)/so;
	            my $fName = $1;
	            my $fDesc = $2;
	            my $fObj = HeaderDoc::MinorAPIElement->new();
	            $fObj->outputformat($self->outputformat);
	            $fObj->name($fName);
		    $fObj->type("field");
	            $fObj->discussion($fDesc);
	            $self->addField($fObj);
				last SWITCH;
			};
            # To handle callbacks and their params and results, have to set up loop
            ($field =~ s/^callback\s+//o) &&
                do {
                    $field =~ s/^\s+|\s+$//go;
                    $field =~ /(\w*)\s*(.*)/so;
                    my $cbName = $1;
                    my $cbDesc = $2;
                    my $callbackObj = HeaderDoc::MinorAPIElement->new();
	            $callbackObj->outputformat($self->outputformat);
                    $callbackObj->name($cbName);
                    $callbackObj->discussion($cbDesc);
                    $callbackObj->type("callback");
                    # now get params and result that go with this callback
                    print "Adding callback.  Callback name: $cbName.\n" if ($localDebug);
                    $fieldCounter++;
                    while ($fieldCounter < $lastField) {
                        my $nextField = $fields[$fieldCounter];
                        print "In callback: next field is '$nextField'\n" if ($localDebug);
                        
                        if ($nextField =~ s/^param\s+//o) {
                            $nextField =~ s/^\s+|\s+$//go;
                            $nextField =~ /(\w*)\s*(.*)/so;
                            my $paramName = $1;
                            my $paramDesc = $2;
                            $callbackObj->addToUserDictArray({"$paramName" => "$paramDesc"});
                        } elsif ($nextField eq "result") {
                            $nextField =~ s/^\s+|\s+$//go;
                            $nextField =~ /(\w*)\s*(.*)/so;
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
            ($field =~ s/^param\s+//o) && 
                do {
                    $self->isFunctionPointer(1);
                    $field =~ s/^\s+|\s+$//go;
                    $field =~ /(\w*)\s*(.*)/so;
                    my $fName = $1;
                    my $fDesc = $2;
                    my $fObj = HeaderDoc::MinorAPIElement->new();
	            $fObj->outputformat($self->outputformat);
                    $fObj->name($fName);
                    $fObj->discussion($fDesc);
                    $fObj->type("funcPtr");
                    $self->addField($fObj);
                    print "Adding param for function-pointer typedef.  Param name: $fName.\n" if ($localDebug);
                    last SWITCH;
                };
	    {
		# default case
		# my $filename = $HeaderDoc::headerObject->name();
	        # print "$filename:$linenum:Unknown field in Struct comment: $field\n";
		my $struct_or_union = "struct";
		if ($self->isUnion()) { $struct_or_union = "union"; }
		if (length($field)) { warn "$filename:$linenum:Unknown field (\@$field) in $struct_or_union comment (".$self->name().")\n"; }
	    }
	}
	++$fieldCounter;
    }
}

sub setStructDeclaration {
    my $self = shift;
    my $dec = shift;
    my $localDebug = 0;
    $self->declaration($dec);
    
    print "============================================================================\n" if ($localDebug);
    print "Raw declaration is: $dec\n" if ($localDebug);

    # my $newdec = $self->structformat($dec, 1);
    
    # print "new dec is:\n$newdec\n" if ($localDebug);
    # $dec = $newdec;

    if (length ($dec)) {$dec = "<pre>\n$dec</pre>\n";};
    
    print "Struct: returning declaration:\n\t|$dec|\n" if ($localDebug);
    print "============================================================================\n" if ($localDebug);
    $self->declarationInHTML($dec);
    return $dec;
}


sub printObject {
    my $self = shift;
 
    print "Struct\n";
    $self->SUPER::printObject();
    print "Field Descriptions:\n";
    my $fieldArrayRef = $self->{FIELDS};
    if ($fieldArrayRef) {
	my $arrayLength = @{$fieldArrayRef};
	if ($arrayLength > 0) {
            &printArray(@{$fieldArrayRef});
	}
    }
    print "\n";
}

1;

