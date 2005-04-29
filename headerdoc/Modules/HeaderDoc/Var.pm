#! /usr/bin/perl
#
# Class name: Var
# Synopsis: Holds class and instance data members parsed by headerDoc
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2004/10/13 00:09:34 $
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

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash);
use HeaderDoc::HeaderElement;
use HeaderDoc::Struct;

# making it a subclass of Struct, so that it has the "fields" ivar.
@ISA = qw( HeaderDoc::Struct );
use strict;
use vars qw($VERSION @ISA);
$VERSION = '$Revision: 1.8.2.8.2.21 $';

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

sub processComment {
    my($self) = shift;
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
            ($field =~ s/^var(\s+)/$1/o) && 
            do {
                my ($name, $disc);
                ($name, $disc) = &getAPINameAndDisc($field); 
                $self->name($name);
                if (length($disc)) {$self->discussion($disc);};
                last SWITCH;
            };
	    ($field =~ s/^serial\s+//io) && do {$self->attribute("Serial Field Info", $field, 1); last SWITCH;};
	    ($field =~ s/^serialfield\s+//io) && do {
		    if (!($field =~ s/(\S+)\s+(\S+)\s+//so)) {
			warn "$filename:$linenum:Serialfield format wrong.\n";
		    } else {
			my $name = $1;
			my $type = $2;
			my $description = "(no description)";
			my $att = "$name Type: $type";
			$field =~ s/^(<BR>|\s)*//sgio;
			if (length($field)) {
				$att .= "<br>\nDescription: $field";
			}
			$self->attributelist("Serial Fields", $att,  1);
		    }
		    last SWITCH;
		};
            ($field =~ s/^abstract\s+//o) && do {$self->abstract($field); last SWITCH;};
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
	    # my $filename = $HeaderDoc::headerObject->name();
	    my $filename = $self->filename();
	    my $linenum = $self->linenum();
            # print "$filename:$linenum:Unknown field in Var comment: $field\n";
	    if (length($field)) { warn "$filename:$linenum:Unknown field (\@$field) in var comment (".$self->name().")\n"; }
		}
	}
}


sub setVarDeclaration {
    my($self) = shift;
    my ($dec) = @_;
    my $localDebug = 0;

    $self->declaration($dec);
    
    print "============================================================================\n" if ($localDebug);
    print "Raw var declaration is: $dec\n" if ($localDebug);
    $self->declarationInHTML($dec);
    return $dec;
}


sub printObject {
    my $self = shift;
 
    print "Var\n";
    $self->SUPER::printObject();
    print "\n";
}

1;

