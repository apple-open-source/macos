#! /usr/bin/perl
#
# Class name: PDefined
# Synopsis: Holds headerDoc comments of the @define type, which
#           are used to comment symbolic constants declared with #define
#
# Last Updated: $Date: 2009/03/30 19:38:51 $
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
use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc convertCharsForFileMaker printArray printHash validTag);
use HeaderDoc::HeaderElement;

@ISA = qw( HeaderDoc::HeaderElement );
use strict;
use vars qw($VERSION @ISA);
use Carp qw(cluck);
$HeaderDoc::PDefine::VERSION = '$Revision: 1.16 $';

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
    $self->{BLOCKDISCUSSION} = "";
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
    $clone->{BLOCKDISCUSSION} = $self->{BLOCKDISCUSSION};
    $clone->{PARSETREELIST} = $self->{PARSETREELIST};
    $clone->{PARSEONLY} = $self->{PARSEONLY};

    return $clone;
}


sub discussion
{
    my $self = shift;
    my $localDebug = 0;

    if (@_) {
	if ($localDebug) {
		print STDERR "Set Discussion for #define (or block) to ".@_[0]."..\n";
	}
	return $self->SUPER::discussion(@_);
    }

    my $realdisc = $self->SUPER::discussion();
    my $realCheckDisc = $self->SUPER::halfbaked_discussion();

    if (!length($realCheckDisc) || ($realCheckDisc !~ /\S/)) {
	# print "RETURNING BLOCK DISC FOR $self (".$self->name().")\n";
	return $self->blockDiscussion();
    }
    return $realdisc;
}


sub processComment_old {
    my $localDebug = 0;
    my($self) = shift;
    my $fieldArrayRef = shift;
    my @fields = @$fieldArrayRef;
    my $fullpath = $self->fullpath();
    my $linenum = $self->linenum();
    my $olddisc = $self->discussion();

    foreach my $field (@fields) {
	my $fieldname = "";
	my $top_level_field = 0;
	if ($field =~ /^(\w+)(\s|$)/) {
		$fieldname = $1;
		# print STDERR "FIELDNAME: $fieldname\n";
		$top_level_field = validTag($fieldname, 1);
	}
	# print STDERR "TLF: $top_level_field, FN: \"$fieldname\"\n";
	if ($localDebug) { print STDERR "FIELD: $field\n"; }
        chomp($field);
	# if ($localDebug) { print STDERR "CHOMPED FIELD: $field\n"; }
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
            ($field =~ s/^(?:discussion|details)(\s+|$)//io) && do {
			if ($self->inDefineBlock() && length($olddisc)) {
				# Silently drop these....
				$self->{DISCUSSION} = "";
			}
			if (!length($field)) { $field = "\n"; }
			$self->discussion($field);
			last SWITCH;
		};
            ($field =~ s/^availability\s+//io) && do {$self->availability($field); last SWITCH;};
            ($field =~ s/^since\s+//io) && do {$self->availability($field); last SWITCH;};
            ($field =~ s/^author\s+//io) && do {$self->attribute("Author", $field, 0); last SWITCH;};
            ($field =~ s/^group\s+//io) && do {$self->group($field); last SWITCH;};
            ($field =~ s/^indexgroup\s+//io) && do {$self->indexgroup($field); last SWITCH;};
	    ($field =~ s/^version\s+//io) && do {$self->attribute("Version", $field, 0); last SWITCH;};
            ($field =~ s/^deprecated\s+//io) && do {$self->attribute("Deprecated", $field, 0); last SWITCH;};
            ($field =~ s/^updated\s+//io) && do {$self->updated($field); last SWITCH;};
            ($field =~ s/^parseOnly(\s+|$)//io) && do { $self->parseOnly(1); last SWITCH; };
            ($field =~ s/^noParse(\s+|$)//io) && do { print STDERR "Parsing will be skipped.\n" if ($localDebug); $HeaderDoc::skipNextPDefine = 1; last SWITCH; };
            ($field =~ s/^(param|field)\s+//io) && do {
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
	    ($field =~ s/^return\s+//io) && do {$self->result($field); last SWITCH;};
	    ($field =~ s/^result\s+//io) && do {$self->result($field); last SWITCH;};
		($top_level_field == 1) && do {
			my $keepname = 1;
			my $blockrequest = 0;
 			if ($field =~ s/^(availabilitymacro)(\s+|$)/$2/io) {
				$self->isAvailabilityMacro(1);
				$keepname = 1;
				$self->parseOnly(1);
 			} elsif ($field =~ s/^(define(?:d)?|function)(\s+|$)/$2/io) {
				$keepname = 1;
				$blockrequest = 0;
            		} elsif ($field =~ s/^(define(?:d)?block)(\s+)/$2/io) {
				$keepname = 1;
				$self->isBlock(1);
				$blockrequest = 1;
			} else {
				$field =~ s/(\w+)(\s|$)/$2/io;
				$keepname = 0;
				$blockrequest = 0;
			}
		    	my ($defname, $defabstract_or_disc, $namedisc) = getAPINameAndDisc($field);
		    	if ($self->isBlock()) {
				print STDERR "ISBLOCK (BLOCKREQUEST=$blockrequest)\n" if ($localDebug);
				# my ($defname, $defabstract_or_disc, $namedisc) = getAPINameAndDisc($field);
				# In this case, we get a name and abstract.
				if ($blockrequest) {
					print STDERR "Added block name $defname\n" if ($localDebug);
					$self->name($defname);
					if (length($defabstract_or_disc)) {
						if ($namedisc) {
							$self->nameline_discussion($defabstract_or_disc);
						} else {
							$self->discussion($defabstract_or_disc);
						}
					}
				} else {
					print STDERR "Added block member $defname\n" if ($localDebug);
					$self->attributelist("Included Defines", $field);
				}
		    	} else {
				if (length($defname)) {
					# print STDERR "NOT BLOCK.  NAME IS \"$defname\"\n";
					$self->name($defname);
				}
				if (length($defabstract_or_disc)) {
					if ($namedisc) {
						$self->nameline_discussion($defabstract_or_disc);
					} else {
						$self->discussion($defabstract_or_disc);
					}
				}
		    	}
		    	last SWITCH;
		};
	    # my $fullpath = $HeaderDoc::headerObject->name();
	    my $fullpath = $self->fullpath();
	    my $linenum = $self->linenum();
            # print STDERR "$fullpath:$linenum:Unknown field in #define comment: $field\n";
		if (length($field)) { warn "$fullpath:$linenum: warning: Unknown field (\@$field) in #define comment (".$self->name().")\n"; }
		}
	}
}

sub setDeclaration {
    my($self) = shift;
    my ($dec) = @_;
    my $localDebug = 0;
    $self->declaration($dec);
    my $fullpath = $self->fullpath();
    my $line = $self->linenum();

    # if ($dec =~ /#define.*#define/so && !($self->isBlock)) {
	# warn("$fullpath:$line:WARNING: Multiple #defines in \@define.  Use \@defineblock instead.\n");
    # }
    
    print STDERR "============================================================================\n" if ($localDebug);
    print STDERR "Raw #define declaration is: $dec\n" if ($localDebug);
    $self->declarationInHTML($dec);
    return $dec;
}

sub isAvailabilityMacro {
    my $self = shift;

    if (@_) {
	$self->{ISAVAILABILITYMACRO} = shift;
    }

    return $self->{ISAVAILABILITYMACRO};
}


sub blockDiscussion {
    my $self = shift;
    my $localDebug = 0;
    
    if (@_) {
	if ($localDebug) {
		print STDERR "Set Discussion for #define (or block) to ".@_[0]."..\n";
	}
        $self->{BLOCKDISCUSSION} = shift;
    }
    return $self->{BLOCKDISCUSSION};
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
 
    print STDERR "#Define\n";
    $self->SUPER::printObject();
    print STDERR "Result: $self->{RESULT}\n";
    print STDERR "\n";
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
	# print STDERR "SV: $success $value\n";
	if ($success) {
		my $vstr = "";
		if ($xmlmode) {
			$vstr = sprintf("0x%x", $value)
		} else {
			$vstr = sprintf("0x%x (%d)", $value, $value)
		}
		if (!$self->isBlock) { $self->attribute("Value", $vstr, 0); }
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

sub free
{
    my $self = shift;
    $self->{PARSETREELIST} = ();
    $self->SUPER::free();
}

1;

