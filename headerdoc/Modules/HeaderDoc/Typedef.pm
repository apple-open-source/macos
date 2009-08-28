#! /usr/bin/perl
#
# Class name: Typedef
# Synopsis: Holds typedef info parsed by headerDoc
#
# Last Updated: $Date: 2009/03/30 19:38:52 $
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
package HeaderDoc::Typedef;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash validTag);
use HeaderDoc::HeaderElement;
use HeaderDoc::MinorAPIElement;
use HeaderDoc::APIOwner;
use Carp qw(cluck);

@ISA = qw( HeaderDoc::HeaderElement );

use strict;
use vars qw($VERSION @ISA);
$HeaderDoc::Typedef::VERSION = '$Revision: 1.17 $';


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
    # $self->{RESULT} = undef;
    $self->{FIELDS} = ();
    # $self->{ISFUNCPTR} = 0;
    # $self->{ISENUMLIST} = 0;
    $self->{CLASS} = "HeaderDoc::Typedef";
}

sub clone {
    my $self = shift;
    my $clone = undef;
    if (@_) {
	$clone = shift;
    } else {
	$clone = HeaderDoc::Typedef->new();
    }

    $self->SUPER::clone($clone);

    # now clone stuff specific to function

    $clone->{RESULT} = $self->{RESULT};
    $clone->{FIELDS} = $self->{FIELDS};
    $clone->{ISFUNCPTR} = $self->{ISFUNCPTR};
    $clone->{ISENUMLIST} = $self->{ISENUMLIST};

    return $clone;
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

# sub addField {
    # my $self = shift;
    # my $localDebug = 0;
    # if (@_) { 
	# foreach my $field (@_) {
		# cluck("added field $field->{NAME} ($field->{TYPE})\n") if ($localDebug);
		# push(@{$self->{FIELDS}}, $field);
	# }
        # # push (@{$self->{FIELDS}}, @_);
    # }
    # return @{ $self->{FIELDS} };
# }


sub isEnumList {
    my $self = shift;

    if (@_) {
        $self->{ISENUMLIST} = shift;
    }
    return $self->{ISENUMLIST};
}


sub processComment_old {
    my $self = shift;
    my $fieldArrayRef = shift;
    my @fields = @$fieldArrayRef;
    my $lastField = scalar(@fields);
    my $fullpath = $self->fullpath();
    my $linenum = $self->linenum();
    my $fieldCounter = 0;
    my $localDebug = 0;
    
    while ($fieldCounter < $lastField) {
        my $field = $fields[$fieldCounter];
	print STDERR "FIELD WAS $field\n" if ($localDebug);

	my $fieldname = "";
	my $top_level_field = 0;
	if ($field =~ /^(\w+)(\s|$)/) {
		$fieldname = $1;
		# print STDERR "FIELDNAME: $fieldname\n";
		$top_level_field = validTag($fieldname, 1);
	}
	# print STDERR "TLF: $top_level_field, FN: \"$fieldname\"\n";
        SWITCH: {
            ($field =~ /^\/\*\!/o)&& do {
                                my $copy = $field;
                                $copy =~ s/^\/\*\!\s*//s;
                                if (length($copy)) {
                                        $self->discussion($copy);
                                }
                        last SWITCH;
                        };
            ($field =~ s/^abstract\s+//io) && do {$self->abstract($field); last SWITCH;};
            ($field =~ s/^brief\s+//io) && do {$self->abstract($field, 1); last SWITCH;};
            ($field =~ s/^availability\s+//io) && do {$self->availability($field); last SWITCH;};
            ($field =~ s/^since\s+//io) && do {$self->availability($field); last SWITCH;};
            ($field =~ s/^author\s+//io) && do {$self->attribute("Author", $field, 0); last SWITCH;};
            ($field =~ s/^group\s+//io) && do {$self->group($field); last SWITCH;};
            ($field =~ s/^indexgroup\s+//io) && do {$self->indexgroup($field); last SWITCH;};
	    ($field =~ s/^version\s+//io) && do {$self->attribute("Version", $field, 0); last SWITCH;};
            ($field =~ s/^deprecated\s+//io) && do {$self->attribute("Deprecated", $field, 0); last SWITCH;};
            ($field =~ s/^updated\s+//io) && do {$self->updated($field); last SWITCH;};
	    ($field =~ s/^attribute\s+//io) && do {
		    my ($attname, $attdisc, $namedisc) = &getAPINameAndDisc($field);
		    if (length($attname) && length($attdisc)) {
			$self->attribute($attname, $attdisc, 0);
		    } else {
			warn "$fullpath:$linenum: warning: Missing name/discussion for attribute\n";
		    }
		    last SWITCH;
		};
	    ($field =~ s/^attributelist\s+//io) && do {
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
			warn "$fullpath:$linenum: warning: Missing name/discussion for attributelist\n";
		    }
		    last SWITCH;
		};
	    ($field =~ s/^attributeblock\s+//io) && do {
		    my ($attname, $attdisc, $namedisc) = &getAPINameAndDisc($field);
		    if (length($attname) && length($attdisc)) {
			$self->attribute($attname, $attdisc, 1);
		    } else {
			warn "$fullpath:$linenum: warning: Missing name/discussion for attributeblock\n";
		    }
		    last SWITCH;
		};
	    ($field =~ /^see(also|)\s+/io) &&
		do {
		    $self->see($field);
		    last SWITCH;
		};
            ($field =~ s/^details(\s+|$)//io) && do {$self->discussion($field); last SWITCH;};
            ($field =~ s/^discussion(\s+|$)//io) && do {$self->discussion($field); last SWITCH;};
            ($field =~ s/^field\s+//io) &&
                do {
                    $field =~ s/^\s+|\s+$//go;
                    $field =~ /(\w*)\s*(.*)/so;
                    my $fName = $1;
                    my $fDesc = $2;
                    my $fObj = HeaderDoc::MinorAPIElement->new();
	            $fObj->outputformat($self->outputformat);
                    $fObj->name($fName);
                    $fObj->discussion($fDesc);
                    $fObj->type("field");
                    $self->addField($fObj);
                    print STDERR "Adding field for typedef.  Field name: $fName.\n" if ($localDebug);
                    last SWITCH;
                };
            ($field =~ s/^const(ant)?\s+//io) &&
                do {
                    $self->isEnumList(1);
                    $field =~ s/^\s+|\s+$//go;
#                    $field =~ /(\w*)\s*(.*)/so;
    	            # $field =~ /(\S*)\s*(.*)/so; # Let's accept any printable char for name, for pseudo enum values
                    # my $fName = $1;
                    # my $fDesc = $2;
		    my ($fName, $fDesc, $namedisc) = getAPINameAndDisc($field);
                    my $fObj = HeaderDoc::MinorAPIElement->new();
	            $fObj->outputformat($self->outputformat);
                    $fObj->name($fName);
                    $fObj->discussion($fDesc);
                    $fObj->type("constant");
                    $self->addConstant($fObj);
                    # print STDERR "Adding constant for enum.  Constant name: $fName.\n" if ($localDebug);
                    last SWITCH;
                };
            # To handle callbacks and their params and results, have to set up loop
            ($field =~ s/^callback\s+//io) &&
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
                    print STDERR "Adding callback.  Callback name: $cbName.\n" if ($localDebug);
                    $fieldCounter++;
                    while ($fieldCounter < $lastField) {
                        my $nextField = $fields[$fieldCounter];
                        print STDERR "In callback: next field is '$nextField'\n" if ($localDebug);
                        
                        if ($nextField =~ s/^param\s+//io) {
                            $nextField =~ s/^\s+|\s+$//go;
                            $nextField =~ /(\w*)\s*(.*)/so;
                            my $paramName = $1;
                            my $paramDesc = $2;
                            $callbackObj->addToUserDictArray({"$paramName" => "$paramDesc"});
                        } elsif ($nextField =~ s/^result\s+//io) {
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
                    print STDERR "Adding callback to typedef.  Callback name: $cbName.\n" if ($localDebug);
                    last SWITCH;
                };
            # param and result have to come last, since they should be handled differently, if part of a callback
            # which is inside a struct (as above).  Otherwise, these cases below handle the simple typedef'd callback 
            # (i.e., a typedef'd function pointer without an enclosing struct.
            ($field =~ s/^param\s+//io) && 
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
                    print STDERR "Adding param for function-pointer typedef.  Param name: $fName.\n" if ($localDebug);
                    last SWITCH;
                };
            ($field =~ s/^result(\s+|$)//io || $field =~ s/^return(\s+|$)//io) && 
                do {
                    $self->isFunctionPointer(1);
                    $self->result($field);
                    last SWITCH;
                };
		($top_level_field == 1) && do {
			my $keepname = 1;
 			if ($field =~ s/^(typedef|function|class)(\s+|$)/$2/io) {
				$keepname = 1;
			} else {
				$field =~ s/(\w+)(\s|$)/$2/io;
				$keepname = 0;
			}
                    	my ($name, $disc, $namedisc);
                    	($name, $disc, $namedisc) = &getAPINameAndDisc($field); 
                    	$self->name($name);
               	    	if (length($disc)) {
				if ($namedisc) {
					$self->nameline_discussion($disc);
				} else {
					$self->discussion($disc);
				}
		    	}
                    	last SWITCH;
                };
	    # my $fullpath = $HeaderDoc::headerObject->name();
	    my $fullpath = $self->fullpath();
	    my $linenum = $self->linenum();
            # print STDERR "$fullpath:$linenum:Unknown field in Typedef comment: $field\n";
	    if (length($field)) { warn "$fullpath:$linenum: warning: Unknown field (\@$field) in typedef comment (".$self->name().")\n"; }
        }
        $fieldCounter++;
    }
}

sub setDeclaration {
    my $self = shift;
    my $dec = shift;
    my $decType;
    my $localDebug = 0;
    my $fullpath = $self->fullpath();
    my $linenum = $self->linenum();

    if ($self->isFunctionPointer() && $dec =~ /typedef(\s+\w+)*\s+\{/o) {
	# Somebody put in an @param instead of an @field
	$self->isFunctionPointer(0);
	warn("$fullpath:$linenum: warning: typedef markup invalid. Non-callback typedefs should use \@field, not \@param.\n");
    }

    $self->declaration($dec);

    $self->declarationInHTML($dec);
    return $dec;
}


sub printObject {
    my $self = shift;
 
    print STDERR "Typedef\n";
    $self->SUPER::printObject();
    SWITCH: {
        if ($self->isFunctionPointer()) {print STDERR "Parameters:\n"; last SWITCH; }
        if ($self->isEnumList()) {print STDERR "Constants:\n"; last SWITCH; }
        print STDERR "Fields:\n";
    }

    my $fieldArrayRef = $self->{FIELDS};
    if ($fieldArrayRef) {
        my $arrayLength = @{$fieldArrayRef};
        if ($arrayLength > 0) {
            &printArray(@{$fieldArrayRef});
        }
    }
    print STDERR "is function pointer: $self->{ISFUNCPTR}\n";
    print STDERR "is enum list: $self->{ISENUMLIST}\n";
    print STDERR "\n";
}

1;

