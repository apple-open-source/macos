#! /usr/bin/perl
#
# Class name: Var
# Synopsis: Holds class and instance data members parsed by headerDoc
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
package HeaderDoc::Var;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash validTag);
use HeaderDoc::HeaderElement;
use HeaderDoc::Struct;

# making it a subclass of Struct, so that it has the "fields" ivar.
@ISA = qw( HeaderDoc::Struct );
use strict;
use vars qw($VERSION @ISA);
$HeaderDoc::Var::VERSION = '$Revision: 1.12 $';

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
    $self->{CLASS} = "HeaderDoc::Var";
}

sub clone {
    my $self = shift;
    my $clone = undef;
    if (@_) {
        $clone = shift;
    } else {
        $clone = HeaderDoc::Var->new();
    }

    $self->SUPER::clone($clone);

    # now clone stuff specific to var

    return $clone;
}

sub processComment_old {
    my($self) = shift;
    my $fieldArrayRef = shift;
    my @fields = @$fieldArrayRef;
    my $fullpath = $self->fullpath();
    my $linenum = $self->linenum();
    my $isProperty = $self->isProperty();

    # print STDERR "ISPROPERTY: $isProperty\n";

    foreach my $field (@fields) {
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
	    ($field =~ s/^serial\s+//io) && do {$self->attribute("Serial Field Info", $field, 1); last SWITCH;};
	    ($field =~ s/^serialfield\s+//io) && do {
		    if (!($field =~ s/(\S+)\s+(\S+)\s+//so)) {
			warn "$fullpath:$linenum: warning: serialfield format wrong.\n";
		    } else {
			my $name = $1;
			my $type = $2;
			my $description = "(no description)";
			my $att = "$name Type: $type";
			$field =~ s/^(<br>|\s)*//sgio;
			if (length($field)) {
				$att .= "<br>\nDescription: $field";
			}
			$self->attributelist("Serial Fields", $att,  1);
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

		# Added bits for properties.
		($isProperty && $field =~ s/^param\s+//io) && 
			do {
				$field =~ s/^\s+|\s+$//go; # trim leading and trailing whitespace
	            $field =~ /(\w*)\s*(.*)/so;
	            my $pName = $1;
	            my $pDesc = $2;
	            my $param = HeaderDoc::MinorAPIElement->new();
	            $param->outputformat($self->outputformat);
	            $param->name($pName);
	            $param->discussion($pDesc);
	            $self->addTaggedParameter($param);
# my $name = $self->name();
# print STDERR "Adding $pName : $pDesc in $name\n";
# my $class = ref($self) || $self;
# print STDERR "class is $class\n";
				last SWITCH;
			};
 		($isProperty && $field =~ s/^throws\s+//io) && do {$self->throws($field); last SWITCH;};
 		($isProperty && $field =~ s/^exception\s+//io) && do {$self->throws($field); last SWITCH;};
		($isProperty && $field =~ s/^return\s+//io) && do {$self->result($field); last SWITCH;};
		($isProperty && $field =~ s/^result\s+//io) && do {$self->result($field); last SWITCH;};
		($top_level_field == 1) && do {
			my $keepname = 1;
 			if ($field =~ s/^(var|property)(\s+|$)/$2/io) {
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
            # print STDERR "$fullpath:$linenum:Unknown field in Var comment: $field\n";
	    if (length($field)) { warn "$fullpath:$linenum: warning: Unknown field (\@$field) in var comment (".$self->name().")\n"; }
		}
	}
}


sub setDeclaration {
    my($self) = shift;
    my ($dec) = @_;
    my $localDebug = 0;

    $self->declaration($dec);
    
    print STDERR "============================================================================\n" if ($localDebug);
    print STDERR "Raw var declaration is: $dec\n" if ($localDebug);
    $self->declarationInHTML($dec);
    return $dec;
}

sub isProperty {
    my $self = shift;
    if (@_) {
	my $isprop = shift;
	$self->{ISPROPERTY} = $isprop;
    }
    return $self->{ISPROPERTY} || 0;
}

sub printObject {
    my $self = shift;
 
    print STDERR "Var\n";
    $self->SUPER::printObject();
    print STDERR "\n";
}

# Extra stuff for Objective-C Properties

sub result {
    my $self = shift;
    
    if (@_) {
        $self->{RESULT} = shift;
    }
    return $self->{RESULT};
}


1;

