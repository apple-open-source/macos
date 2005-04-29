#! /usr/bin/perl -w
#
# Class name: Constant
# Synopsis: Holds constant info parsed by headerDoc
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2004/10/13 00:09:27 $
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
package HeaderDoc::Constant;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc convertCharsForFileMaker printArray printHash);
use HeaderDoc::HeaderElement;
use HeaderDoc::APIOwner;

@ISA = qw( HeaderDoc::HeaderElement );

use strict;
use vars qw($VERSION @ISA);
$VERSION = '$Revision: 1.9.2.9.2.20 $';

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
    $self->{CLASS} = "HeaderDoc::Constant";
}

sub clone {
    my $self = shift;
    my $clone = undef;
    if (@_) {
        $clone = shift;
    } else {
        $clone = HeaderDoc::Constant->new();
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
    my $localDebug = 0;

	foreach my $field (@fields) {
    	print "Constant field is |$field|\n" if ($localDebug);
		SWITCH: {
            ($field =~ /^\/\*\!/o)&& do {
                                my $copy = $field;
                                $copy =~ s/^\/\*\!\s*//s;
                                if (length($copy)) {
                                        $self->discussion($copy);
                                }
                        last SWITCH;
                        };
            ($field =~ s/^const(ant)?(\s+)//o) && 
            do {
		if (length($2)) { $field = "$2$field"; }
		else { $field = "$1$field"; }
                my ($name, $disc);
                ($name, $disc) = &getAPINameAndDisc($field); 
                $self->name($name);
                if (length($disc)) {$self->discussion($disc);};
                last SWITCH;
            };
	    ($field =~ s/^serial\s+//io) && do {$self->attribute("Serial Field Info", $field, 1); last SWITCH;};
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
	    # my $filename = $HeaderDoc::headerObject->filename();
            # warn "$filename:$linenum:Unknown field in constant comment: $field\n";
		{
		    if (length($field)) { warn "$filename:$linenum:Unknown field (\@$field) in constant comment (".$self->name().")\n"; }
		};
	    }
	}
}

sub setConstantDeclaration {
    my($self) = shift;
    my ($dec) = @_;
    my $localDebug = 0;
    
    print "============================================================================\n" if ($localDebug);
    print "Raw constant declaration is: $dec\n" if ($localDebug);
    $self->declaration($dec);
    $self->declarationInHTML($dec);
    return $dec;
}


sub printObject {
    my $self = shift;
 
    print "Constant\n";
    $self->SUPER::printObject();
    print "\n";
}

1;

