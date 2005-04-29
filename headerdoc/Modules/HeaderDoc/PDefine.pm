#! /usr/bin/perl
#
# Class name: PDefined
# Synopsis: Holds headerDoc comments of the @define type, which
#           are used to comment symbolic constants declared with #define
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2004/12/15 21:50:30 $
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
package HeaderDoc::PDefine;
use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc convertCharsForFileMaker printArray printHash);
use HeaderDoc::HeaderElement;

@ISA = qw( HeaderDoc::HeaderElement );
use strict;
use vars qw($VERSION @ISA);
$VERSION = '$Revision: 1.9.2.8.2.31 $';

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
    # $self->{ISBLOCK} = 0;
    # $self->{RESULT} = undef;
    $self->{PARSETREELIST} = ();
    $self->{PARSEONLY} = ();
    $self->{CLASS} = "HeaderDoc::PDefine";
}

sub clone {
    my $self = shift;
    my $clone = undef;
    if (@_) {
	$clone = shift;
    } else {
	$clone = HeaderDoc::PDefine->new();
    }

    $self->SUPER::clone($clone);

    # now clone stuff specific to function

    $clone->{ISBLOCK} = $self->{ISBLOCK};
    $clone->{RESULT} = $self->{RESULT};
    $clone->{PARSETREELIST} = $self->{PARSETREELIST};
    $clone->{PARSEONLY} = $self->{PARSEONLY};

    return $clone;
}


sub processComment {
    my $localDebug = 0;
    my($self) = shift;
    my $fieldArrayRef = shift;
    my @fields = @$fieldArrayRef;
    my $filename = $self->filename();
    my $linenum = $self->linenum();

	foreach my $field (@fields) {
        chomp($field);
		SWITCH: {
            ($field =~ /^\/\*\!/o)&& do {
                                my $copy = $field;
                                $copy =~ s/^\/\*\!\s*//s;
                                if (length($copy)) {
                                        $self->discussion($copy);
                                }
                        last SWITCH;
                        };
            ($field =~ s/^define(d)?(\s+)//o || $field =~ s/^function\s+//o) && do {
		    if (length($2)) { $field = "$2$field"; }
		    else { $field = "$1$field"; }
		    my ($defname, $defabstract_or_disc) = getAPINameAndDisc($field);
		    if ($self->isBlock()) {
			# print "ISBLOCK\n";
			# my ($defname, $defabstract_or_disc) = getAPINameAndDisc($field);
			# In this case, we get a name and abstract.
			print "Added alternate name $defname\n" if ($localDebug);
			$self->attributelist("Included Defines", $field);
		    } else {
			# print "NOT BLOCK\n";
			$self->name($defname);
			if (length($defabstract_or_disc)) {
				$self->discussion($defabstract_or_disc);
			}
		    }
		    last SWITCH;
		};
            ($field =~ s/^define(d)?block(\s+)//o) && do {
		    if (length($2)) { $field = "$2$field"; }
		    else { $field = "$1$field"; }
		    my ($defname, $defdisc) = getAPINameAndDisc($field);
		    $self->isBlock(1);
		    $self->name($defname);
		    $self->discussion($defdisc);
		    last SWITCH;
		};
            ($field =~ s/^abstract\s+//o) && do {$self->abstract($field); last SWITCH;};
            ($field =~ s/^discussion\s+//o) && do {
			if ($self->inDefineBlock() && length($self->discussion())) {
				# Silently drop these....
				$self->{DISCUSSION} = "";
			}
			$self->discussion($field);
			last SWITCH;
		};
            ($field =~ s/^availability\s+//o) && do {$self->availability($field); last SWITCH;};
            ($field =~ s/^since\s+//o) && do {$self->availability($field); last SWITCH;};
            ($field =~ s/^author\s+//o) && do {$self->attribute("Author", $field, 0); last SWITCH;};
	    ($field =~ s/^version\s+//o) && do {$self->attribute("Version", $field, 0); last SWITCH;};
            ($field =~ s/^deprecated\s+//o) && do {$self->attribute("Deprecated", $field, 0); last SWITCH;};
            ($field =~ s/^updated\s+//o) && do {$self->updated($field); last SWITCH;};
            ($field =~ s/^parseOnly\s+//io) && do { $self->parseOnly(1); last SWITCH; };
            ($field =~ s/^(param|field)\s+//o) && do {
                    $field =~ s/^\s+|\s+$//go; # trim leading and trailing whitespace
                    # $field =~ /(\w*)\s*(.*)/so;
                    $field =~ /(\S*)\s*(.*)/so;
                    my $pName = $1;
                    my $pDesc = $2;
                    my $param = HeaderDoc::MinorAPIElement->new();
                    $param->outputformat($self->outputformat);
                    $param->name($pName);                    $param->discussion($pDesc);
                    $self->addTaggedParameter($param);
                                last SWITCH;
		};
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
	    ($field =~ s/^return\s+//o) && do {$self->result($field); last SWITCH;};
	    ($field =~ s/^result\s+//o) && do {$self->result($field); last SWITCH;};
	    # my $filename = $HeaderDoc::headerObject->name();
	    my $filename = $self->filename();
	    my $linenum = $self->linenum();
            # print "$filename:$linenum:Unknown field in #define comment: $field\n";
		if (length($field)) { warn "$filename:$linenum:Unknown field (\@$field) in #define comment (".$self->name().")\n"; }
		}
	}
}

sub setPDefineDeclaration {
    my($self) = shift;
    my ($dec) = @_;
    my $localDebug = 0;
    $self->declaration($dec);
    my $filename = $self->filename();
    my $line = $self->linenum();

    # if ($dec =~ /#define.*#define/so && !($self->isBlock)) {
	# warn("$filename:$line:WARNING: Multiple #defines in \@define.  Use \@defineblock instead.\n");
    # }
    
    print "============================================================================\n" if ($localDebug);
    print "Raw #define declaration is: $dec\n" if ($localDebug);
    $self->declarationInHTML($dec);
    return $dec;
}

sub isBlock {
    my $self = shift;

    if (@_) {
	$self->{ISBLOCK} = shift;
    }

    return $self->{ISBLOCK};
}


sub result {
    my $self = shift;
    
    if (@_) {
        $self->{RESULT} = shift;
    }
    return $self->{RESULT};
}


sub printObject {
    my $self = shift;
 
    print "#Define\n";
    $self->SUPER::printObject();
    print "Result: $self->{RESULT}\n";
    print "\n";
}

sub parseTreeList
{
    my $self = shift;
    my $localDebug = 0;

    if ($localDebug) {
      print "OBJ ".$self->name().":\n";
      foreach my $tree (@{$self->{PARSETREELIST}}) {
	print "PARSE TREE: $tree\n";
      }
    }

    return $self->{PARSETREELIST};
}

sub addParseTree
{
    my $self = shift;
    my $tree = shift;

    push(@{$self->{PARSETREELIST}}, $tree);
}

sub parseTree
{
    my $self = shift;
    my $xmlmode = 0;
    if ($self->outputformat eq "hdxml") {
	$xmlmode = 1;
    }

    if (@_) {
	my $treeref = shift;

	$self->SUPER::parseTree($treeref);
	my $tree = ${$treeref};
	my ($success, $value) = $tree->getPTvalue();
	# print "SV: $success $value\n";
	if ($success) {
		my $vstr = "";
		if ($xmlmode) {
			$vstr = sprintf("0x%x", $value)
		} else {
			$vstr = sprintf("0x%x (%d)", $value, $value)
		}
		$self->attribute("Value", $vstr, 0);
	}
	return $treeref;
    }
    return $self->SUPER::parseTree();
}

sub parseOnly
{
    my $self = shift;
    if (@_) {
	$self->{PARSEONLY} = shift;
    }
    return $self->{PARSEONLY};
}

1;

