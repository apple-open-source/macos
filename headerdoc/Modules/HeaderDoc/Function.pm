#! /usr/bin/perl
#
# Class name: Function
# Synopsis: Holds function info parsed by headerDoc
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2004/12/04 00:22:52 $
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
package HeaderDoc::Function;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash);
use HeaderDoc::HeaderElement;
use HeaderDoc::MinorAPIElement;
use HeaderDoc::APIOwner;

@ISA = qw( HeaderDoc::HeaderElement );
use vars qw($VERSION @ISA);
$VERSION = '$Revision: 1.10.2.10.2.29 $';

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
    # $self->{RESULT} = undef;
    # $self->{CONFLICT} = 0;
    $self->{CLASS} = "HeaderDoc::Function";
}

sub clone {
    my $self = shift;
    my $clone = undef;
    if (@_) {
	$clone = shift;
    } else {
	$clone = HeaderDoc::Function->new();
    }

    $self->SUPER::clone($clone);

    # now clone stuff specific to function

    $clone->{RESULT} = $self->{RESULT};
    $clone->{CONFLICT} = $self->{CONFLICT};

    return $clone;
}


sub result {
    my $self = shift;
    
    if (@_) {
        $self->{RESULT} = shift;
    }
    return $self->{RESULT};
}


sub getParamSignature
{
    my $self = shift;

    my $localDebug = 0;

    print "Function name: ".$self->name()."\n" if ($localDebug);

    my @params = $self->parsedParameters();
    my $signature = "";
    my $returntype = $self->returntype();

    $returntype =~ s/\s*//sg;

    foreach my $param (@params) {
	bless($param, "HeaderDoc::HeaderElement");
	bless($param, $param->class());
	my $name = $param->name();
	my $type = $param->type();

	print "PARAM NAME: $name\nTYPE: $type\n" if ($localDebug);

	$type =~ s/\s//sgo;
	if (!length($type)) {
		# Safety valve, just in case
		$type = $name;
		$type =~ s/\s//sgo;
	}
	if (length($type)) {
		$signature .= ",".$type;
	}
    }
    $signature =~ s/^,//s;
    $signature = $returntype.'/('.$signature.')';

    print "RETURN TYPE WAS $returntype\n" if ($localDebug);

    return $signature;
}


sub processComment {
    my $self = shift;
    my $fieldArrayRef = shift;
    my @fields = @$fieldArrayRef;
    my $filename = $self->filename();
    my $linenum = $self->linenum();

	foreach my $field (@fields) {
		SWITCH: {
            		($field =~ /^\/\*\!/o)&& do {
                                my $copy = $field;
                                $copy =~ s/^\/\*\!\s*//s;
                                if (length($copy)) {
                                        $self->discussion($copy);
                                }
                        last SWITCH;
                        };
			($field =~ s/^method(\s+)/$1/o) && 
			do {
				my ($name, $disc);
				($name, $disc) = &getAPINameAndDisc($field); 
				$self->name($name);
				if (length($disc)) {$self->discussion($disc);};
				last SWITCH;
			};
			($field =~ s/^function(\s+)/$1/o) && 
			do {
				my ($name, $disc);
				($name, $disc) = &getAPINameAndDisc($field); 
				$self->name($name);
				if (length($disc)) {$self->discussion($disc);};
				last SWITCH;
			};
			($field =~ s/^serialData\s+//io) && do {$self->attribute("Serial Data", $field, 1); last SWITCH;};
			($field =~ s/^abstract\s+//o) && do {$self->abstract($field); last SWITCH;};
			($field =~ s/^throws\s+//o) && do {$self->throws($field); last SWITCH;};
			($field =~ s/^exception\s+//o) && do {$self->throws($field); last SWITCH;};
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
			($field =~ s/^discussion\s+//o) && do {$self->discussion($field); last SWITCH;};
			($field =~ s/^templatefield\s+//o) && do {
					$self->attributelist("Template Field", $field);
                                        last SWITCH;
			};
			($field =~ s/^param\s+//o) && 
			do {
				$field =~ s/^\s+|\s+$//go; # trim leading and trailing whitespace
	            # $field =~ /(\w*)\s*(.*)/so;
		    $field =~ /(\S*)\s*(.*)/so;
	            my $pName = $1;
	            my $pDesc = $2;
	            my $param = HeaderDoc::MinorAPIElement->new();
	            $param->outputformat($self->outputformat);
	            $param->name($pName);
	            $param->discussion($pDesc);
	            $self->addTaggedParameter($param);
				last SWITCH;
			};
			($field =~ s/^return\s+//o) && do {$self->result($field); last SWITCH;};
			($field =~ s/^result\s+//o) && do {$self->result($field); last SWITCH;};
			# my $filename = $HeaderDoc::headerObject->filename();
			my $filename = $self->filename();
			my $linenum = $self->linenum();
			if (length($field)) { warn "$filename:$linenum:Unknown field (\@$field) in function comment (".$self->name().")\n"; }
		}
	}
}

sub setFunctionDeclaration {
    my $self = shift;
    my ($dec) = @_;
    my ($retval);
    my $localDebug = 0;
    my $noparens = 0;
    
    print "============================================================================\n" if ($localDebug);
    print "Raw declaration is: $dec\n" if ($localDebug);
    $self->declaration($dec);

    $self->declarationInHTML($dec);
    return $dec;
}

sub conflict {
    my $self = shift;
    my $localDebug = 0;
    if (@_) { 
        $self->{CONFLICT} = @_;
    }
    print "conflict $self->{CONFLICT}\n" if ($localDebug);
    return $self->{CONFLICT};
}

sub printObject {
    my $self = shift;
 
    print "Function\n";
    $self->SUPER::printObject();
    print "Result: $self->{RESULT}\n";
}


1;

