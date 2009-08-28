#! /usr/bin/perl -w
#
# Class name: Enum
# Synopsis: Holds struct info parsed by headerDoc
#
# Last Updated: $Date: 2009/03/30 19:38:50 $
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
package HeaderDoc::Enum;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash validTag);
use HeaderDoc::HeaderElement;
use HeaderDoc::MinorAPIElement;
use HeaderDoc::APIOwner;

@ISA = qw( HeaderDoc::HeaderElement );

use strict;
use vars qw($VERSION @ISA);
$HeaderDoc::Enum::VERSION = '$Revision: 1.13 $';

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
    $self->{CLASS} = "HeaderDoc::Enum";
}

sub clone {
    my $self = shift;
    my $clone = undef;
    if (@_) {
	$clone = shift;
    } else {
	$clone = HeaderDoc::Enum->new();
    }

    $self->SUPER::clone($clone);

    # now clone stuff specific to enum

    return $clone;
}


sub processComment_old {
    my $self = shift;
    my $fieldArrayRef = shift;
    my @fields = @$fieldArrayRef;
    my $fullpath = $self->fullpath();
    my $linenum = $self->linenum();

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
            ($field =~ s/^abstract\s+//io) && do {$self->abstract($field); last SWITCH;};
            ($field =~ s/^brief\s+//io) && do {$self->abstract($field, 1); last SWITCH;};
            ($field =~ s/^details(\s+|$)//io) && do {$self->discussion($field); last SWITCH;};
            ($field =~ s/^discussion(\s+|$)//io) && do {$self->discussion($field); last SWITCH;};
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
            ($field =~ s/^const(ant)?\s+//io) && 
            do {
				$field =~ s/^\s+|\s+$//go;
#	            $field =~ /(\w*)\s*(.*)/so;
	            $field =~ /(\S*)\s*(.*)/so; # Let's accept any printable char for name, for pseudo enum values
	            my $cName = $1;
	            my $cDesc = $2;
	            my $cObj = HeaderDoc::MinorAPIElement->new();
	            $cObj->outputformat($self->outputformat);
	            $cObj->name($cName);
	            $cObj->discussion($cDesc);
                $self->addConstant($cObj); 
		my $name = $self->name();
		if ($name eq "") {
		    $name = "$cName";
		    $self->name($name);
		}
                last SWITCH;
            };
		($top_level_field == 1) &&
			do {
				my $keepname = 1;
 				if ($field =~ s/^(enum)(\s+|$)/$2/io) {
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
	    # my $fullpath = $HeaderDoc::headerObject->fullpath();
	    my $fullpath = $self->fullpath();
	    my $linenum = $self->linenum();
            # print STDERR "$fullpath:$linenum: warning: Unknown field in Enum comment: $field\n";
		    if (length($field)) { warn "$fullpath:$linenum: warning: Unknown field (\@$field) in enum comment (".$self->name().")\n"; }
		}
	}
}

sub getEnumDeclaration {
    my $self = shift;
    my $dec = shift;
    my $localDebug = 0;
    
    print STDERR "============================================================================\n" if ($localDebug);
    print STDERR "Raw declaration is: $dec\n" if ($localDebug);
    if ($HeaderDoc::use_styles) {
	return $dec;
    }
    
    $dec =~ s/\t/  /go;
    $dec =~ s/</&lt;/go;
    $dec =~ s/>/&gt;/go;
    if (length ($dec)) {$dec = "<pre>\n$dec</pre>\n";};
    
    print STDERR "Enum: returning declaration:\n\t|$dec|\n" if ($localDebug);
    print STDERR "============================================================================\n" if ($localDebug);
    return $dec;
}


sub printObject {
    my $self = shift;
 
    print STDERR "Enum\n";
    $self->SUPER::printObject();
    print STDERR "Constants:\n";
}

1;

