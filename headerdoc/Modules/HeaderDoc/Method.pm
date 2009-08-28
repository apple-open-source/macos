#! /usr/bin/perl
#
# Class name: Method
# Synopsis: Holds Objective C method info parsed by headerDoc (not used for C++)
#
# Original Author: SKoT McDonald  <skot@tomandandy.com> Aug 2001
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
package HeaderDoc::Method;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash validTag);
use HeaderDoc::HeaderElement;
use HeaderDoc::MinorAPIElement;
use HeaderDoc::APIOwner;

use strict;
use vars qw($VERSION @ISA);
$HeaderDoc::Method::VERSION = '$Revision: 1.7 $';

# Inheritance
@ISA = qw( HeaderDoc::HeaderElement );

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
    # $self->{OWNER} = undef;
    $self->{ISINSTANCEMETHOD} = "UNKNOWN";
    $self->{CLASS} = "HeaderDoc::Method";
}

sub clone {
    my $self = shift;
    my $clone = undef;
    if (@_) {
	$clone = shift;
    } else {
	$clone = HeaderDoc::Method->new();
    }

    $self->SUPER::clone($clone);

    # now clone stuff specific to function

    $clone->{RESULT} = $self->{RESULT};
    $clone->{CONFLICT} = $self->{CONFLICT};
    $clone->{OWNER} = $self->{OWNER};
    $clone->{ISINSTANCEMETHOD} = $self->{ISINSTANCEMETHOD};

    return $clone;
}


sub setIsInstanceMethod {
    my $self = shift;
    
    if (@_) {
        $self->{ISINSTANCEMETHOD} = shift;
    }
    return $self->{ISINSTANCEMETHOD};
}

sub isInstanceMethod {
    my $self = shift;
    return $self->{ISINSTANCEMETHOD};
}

# Made redundant by apiOwner in HeaderElement
# sub owner {  # class or protocol that this method belongs to
    # my $self = shift;
# 
    # if (@_) {
        # my $name = shift;
        # $self->{OWNER} = $name;
    # } else {
    	# my $n = $self->{OWNER};
		# return $n;
	# }
# }

sub result {
    my $self = shift;
    
    if (@_) {
        $self->{RESULT} = shift;
    }
    return $self->{RESULT};
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


sub processComment_old {
    my $self = shift;
    my $fieldArrayRef = shift;
    my @fields = @$fieldArrayRef;
    my $fullpath = $self->fullpath();
    my $linenum = $self->linenum();
    my $localDebug = 0;

	foreach my $field (@fields) {
		my $fieldname = "";
		my $top_level_field = 0;
		if ($field =~ /^(\w+)(\s|$)/) {
			$fieldname = $1;
			# print STDERR "FIELDNAME: $fieldname\n";
			$top_level_field = validTag($fieldname, 1);
		}
		# print STDERR "TLF: $top_level_field, FN: \"$fieldname\"\n";
		print STDERR "FIELD: $field\n" if ($localDebug);
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
 			($field =~ s/^throws\s+//io) && do {$self->throws($field); last SWITCH;};
 			($field =~ s/^exception\s+//io) && do {$self->throws($field); last SWITCH;};
			($field =~ s/^details(\s+|$)//io) && do {$self->discussion($field); last SWITCH;};
			($field =~ s/^discussion(\s+|$)//io) && do {$self->discussion($field); last SWITCH;};
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
			($field =~ s/^param\s+//io) && 
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
			($field =~ s/^return\s+//io) && do {$self->result($field); last SWITCH;};
			($field =~ s/^result\s+//io) && do {$self->result($field); last SWITCH;};
			($top_level_field == 1) && do {
 				if ($field !~ s/^(method)(\s+|$)/$2/io) {
					$field =~ s/(\w+)(\s|$)/$2/io;
				}
				my ($name, $disc, $namedisc);
				($name, $disc, $namedisc) = &getAPINameAndDisc($field, "/:/"); 
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
			# print STDERR "$fullpath:$linenum:Unknown field in Method comment: $field\n";
			if (length($field)) { warn "$fullpath:$linenum: warning: Unknown field (\@$field) in method comment (".$self->name().")\n"; }
		}
	}
}

# sub getAPINameAndDisc {
    # my $line = shift;
    # my ($name, $disc, $operator);
    # # first, get rid of leading space
    # $line =~ s/^\s+//o;
    # ($name, $disc) = split (/\s/, $line, 2);
    # if ($name =~ /operator/o) {  # this is for operator overloading in C++
        # ($operator, $name, $disc) = split (/\s/, $line, 3);
        # $name = $operator." ".$name;
    # }
    # return ($name, $disc);
# }

sub setDeclaration {
    my $self = shift;
    my ($dec) = @_[0];
    my $classType = @_[1];
    my ($retval);
    my $localDebug = 0;
    
    print STDERR "============================================================================\n" if ($localDebug);
    print STDERR "Raw declaration is: $dec\n" if ($localDebug);
    $self->declaration($dec);
    $self->declarationInHTML($dec);
    return $dec;
}


sub getMethodType {
	my $self = shift;
	my $fullpath = $self->fullpath();
	my $linenum = $self->linenum();
	my $declaration = shift;
	my $methodType = "";
		
if (0) {
	if ($declaration =~ /^\s*-/o) {
	    $methodType = "instm";
	    $self->setIsInstanceMethod("YES");
	} elsif ($declaration =~ /^\s*\+/o) {
	    $methodType = "clm";
	    $self->setIsInstanceMethod("NO");
	} elsif ($declaration =~ /#define/o) {
	    $methodType = "defn";
	    $self->setIsInstanceMethod("NO");
	} else {
		my $fullpath = $HeaderDoc::headerObject->fullpath();
		if (!$HeaderDoc::ignore_apiuid_errors) {
			print STDERR "$fullpath:$linenum: warning: Unable to determine whether declaration is for an instance or class method[method]. '$declaration'\n";
		}
		# We have to take an educated guess so the UID is legal
		$methodType = "instm";
	    $self->setIsInstanceMethod("YES");
	}
} else {
    my $apio = $self->apiOwner();
    my $apioclass = ref($apio) || $apio;
    $self->setIsInstanceMethod("YES");
    $methodType = "instm";

    if ($apioclass =~ /HeaderDoc::ObjCProtocol/) {
	$methodType = "intfm";
    }

    my $ptref = $self->parseTree();
    if (!$ptref) {
	if (!$HeaderDoc::ignore_apiuid_errors) {
	    warn "$fullpath:$linenum: warning: Unable to find parse tree. File a bug.\n";
	}
    } else {
	my $pt = ${$ptref};
	my $ps = undef;

	while ($pt && ($pt->token() =~ /\s/ || !length($pt->token()))) { $pt = $pt->next();}

	if ($pt) {
		# print STDERR "PT TOKEN: ".$pt->token()."\n";
		$ps = $pt->parserState();
	} else {
		# This case is always bad, since it means the declaration is
		# essentially blank....
		warn "$fullpath:$linenum: warning: Unable to find parser state for ".$self->name().". File a bug.\n";
	}

	if (!$ps) {
		# This could be a user error or a bug.
		if ($apioclass =~ /HeaderDoc::Header/) {
			warn "$fullpath:$linenum: warning: Objective-C method found outside a class or interface (or in a class or interface that lacks HeaderDoc markup).\n";
		} else {
			warn "$fullpath:$linenum: warning: Unable to find parser state for ".$self->name().". File a bug.\n";
		}
		# print STDERR "PT TOKEN WAS: ".$pt->token()."\n";
	} else {
		my $token = $ps->{occmethodtype};
		if (!length($token)) {
			warn "$fullpath:$linenum: warning: Unable to find Objective-C method type. File a bug.\n";
		} elsif ($token =~ /\+/) {
			$self->setIsInstanceMethod("NO");
			$methodType = "clm";
			if ($apioclass =~ /HeaderDoc::ObjCProtocol/) {
				$methodType = "intfcm";
			}
		}
	}
    }
}

# print STDERR "GMT NAME: ".$self->name()." TYPE: $methodType DEC:$declaration\n";

	return $methodType;
}

sub printObject {
    my $self = shift;
 
    print STDERR "Method\n";
    $self->SUPER::printObject();
    print STDERR "Result: $self->{RESULT}\n";
}

1;

