#! /usr/bin/perl
#
# Class name: Function
# Synopsis: Holds function info parsed by headerDoc
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
package HeaderDoc::Function;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash validTag);
use HeaderDoc::HeaderElement;
use HeaderDoc::MinorAPIElement;
use HeaderDoc::APIOwner;

@ISA = qw( HeaderDoc::HeaderElement );
use vars qw($VERSION @ISA);
$HeaderDoc::Function::VERSION = '$Revision: 1.14 $';

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

# /*! Some keywords prior to a function name modify the function as a whole, not the return type.
#     These keywords should never be part of the return type. */
sub sanitizedreturntype
{
    my $self = shift;
    my $localDebug = 0;

    my $temp = $self->returntype();

    print STDERR "Debugging return value for NAME: ".$self->name()." RETURNTYPE: $temp\n" if ($localDebug);

    my $ret = ""; my $space = "";
    my @parts = split(/\s+/, $temp);
    foreach my $part (@parts) {
	print STDERR "PART: $part\n" if ($localDebug);
	if (length($part) && $part !~ /(virtual|static)/) {
		$ret .= $space.$part;
		$space = " "
	}
    }
    print STDERR "Returning $ret\n" if ($localDebug);
    return $ret;
}

sub getParamSignature
{
    my $self = shift;

    if ($self->isBlock()) {
	return "";
    }

    my $formatted = 0;
    if (@_) {
	$formatted = shift;
    }

    my $localDebug = 0;
    my $space = "";
    if ($formatted) { $space = " "; }

    # To avoid infinite recursion with debugging on, do NOT change this to $self->name()!
    print STDERR "Function name: ".$self->{NAME}."\n" if ($localDebug);

    my @params = $self->parsedParameters();
    my $signature = "";
    my $returntype = $self->sanitizedreturntype();

    $returntype =~ s/\s*//sg;

    foreach my $param (@params) {
	bless($param, "HeaderDoc::HeaderElement");
	bless($param, $param->class());
	my $name = $param->name();
	my $type = $param->type();

	print STDERR "PARAM NAME: $name\nTYPE: $type\n" if ($localDebug);

	if (!$formatted) {
		$type =~ s/\s//sgo;
	} else {
		$type =~ s/\s+/ /sgo;
	}
	if (!length($type)) {
		# Safety valve, just in case
		$type = $name;
		if (!$formatted) {
			$type =~ s/\s//sgo;
		} else {
			$type =~ s/\s+/ /sgo;
		}
	} else {
		$signature .= ",".$space.$type;
		if ($name =~ /^\s*([*&]+)/) {
			$signature .= $space.$1;
		}
	}
    }
    $signature =~ s/^,\s*//s;

    if (!$formatted) {
	$signature = $returntype.'/('.$signature.')';
    }

    print STDERR "RETURN TYPE WAS $returntype\n" if ($localDebug);

    return $signature;
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
                                $copy =~ s/^s*\*\/\s*$//s; # eliminate a weird edge case for operators --DAG
                                if (length($copy)) {
                                        $self->discussion($copy);
                                }
                        last SWITCH;
                        };
			($field =~ s/^serialData\s+//io) && do {$self->attribute("Serial Data", $field, 1); last SWITCH;};
			($field =~ s/^abstract\s+//io) && do {$self->abstract($field); last SWITCH;};
			($field =~ s/^brief\s+//io) && do {$self->abstract($field, 1); last SWITCH;};
			($field =~ s/^throws\s+//io) && do {$self->throws($field); last SWITCH;};
			($field =~ s/^exception\s+//io) && do {$self->throws($field); last SWITCH;};
			($field =~ s/^availability\s+//io) && do {$self->availability($field); last SWITCH;};
            		($field =~ s/^since\s+//io) && do {$self->availability($field); last SWITCH;};
            		($field =~ s/^author\s+//io) && do {$self->attribute("Author", $field, 0); last SWITCH;};
            		($field =~ s/^group\s+//io) && do {$self->group($field); last SWITCH;};
            		($field =~ s/^(function|method)group\s+//io) && do {$self->group($field); last SWITCH;};
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
			($field =~ s/^templatefield\s+//io) && do {
					$self->attributelist("Template Field", $field);
                                        last SWITCH;
			};
			($field =~ s/^(param|field)\s+//io) && 
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
			($field =~ s/^return\s+//io) && do {$self->result($field); last SWITCH;};
			($field =~ s/^result\s+//io) && do {$self->result($field); last SWITCH;};
			($top_level_field == 1) && do {
				my $keepname = 1;
 				if ($field =~ s/^(method|function)(\s+|$)/$2/io) {
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
			if (length($field)) { warn "$fullpath:$linenum: warning: Unknown field (\@$field) in function comment (".$self->name().")\n"; }
		}
	}
}

sub setDeclaration {
    my $self = shift;
    my ($dec) = @_;
    my ($retval);
    my $localDebug = 0;
    my $noparens = 0;
    
    print STDERR "============================================================================\n" if ($localDebug);
    print STDERR "Raw declaration is: $dec\n" if ($localDebug);
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
    print STDERR "conflict $self->{CONFLICT}\n" if ($localDebug);
    return $self->{CONFLICT};
}

sub printObject {
    my $self = shift;
 
    print STDERR "Function\n";
    $self->SUPER::printObject();
    print STDERR "Result: $self->{RESULT}\n";
}


1;

