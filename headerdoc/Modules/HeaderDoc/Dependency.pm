#! /usr/bin/perl -w
#
# Class name: 	Dependency
# Synopsis: 	Used by headerdoc2html.pl to handle dependency tracking.
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
package HeaderDoc::Dependency;

use strict;
use vars qw($VERSION @ISA);
use HeaderDoc::Utilities qw(isKeyword quote stringToFields casecmp);

$HeaderDoc::Dependency::VERSION = '$Revision: 1.3 $';
################ General Constants ###################################
my $debugging = 0;

my $treeDebug = 0;
my %defaults = (
	NAME => undef,
	DEPNAME => undef,
	MARKED => 0,
	EXISTS => 0,
	PARENT => undef,
	CHILDREN => ()
);

sub new {
    my($param) = shift;
    my($class) = ref($param) || $param;
    my %selfhash = %defaults;
    my $self = \%selfhash;
    
    bless($self, $class);
    $self->_initialize();
    # Now grab any key => value pairs passed in
    my (%attributeHash) = @_;
    foreach my $key (keys(%attributeHash)) {
        my $ucKey = uc($key);
        $self->{$ucKey} = $attributeHash{$key};
    }  
    return ($self);
}

sub _initialize {
    # my($self) = shift;
    # $self->{NAME} = undef;
    # $self->{DEPNAME} = undef;
    # $self->{MARKED} = 0;
    # $self->{EXISTS} = 0;
    # $self->{PARENT} = undef;
    # $self->{CHILDREN} = ();
}

sub clone {
    my $self = shift;
    my $clone = undef;
    if (@_) {
        $clone = shift;
    } else {
        $clone = HeaderDoc::Dependency->new(); 
    }

    # $self->SUPER::clone($clone);

    # now clone stuff specific to Dependency

    $clone->{PARENT} = $self->{PARENT};
    $clone->{CHILDREN} = $self->{CHILDREN};

}

sub addchild {
    my $self = shift;
    my $child = shift;

    push(@{$self->{CHILDREN}}, \$child);
}

my %namehash = ();

sub findname {
    my $self = shift;
    my $name = shift;

    # print STDERR "FINDNAME: $name\n";
    # print STDERR "RETURNING: ".$namehash{$name}."\n";

    return $namehash{$name};
}

sub name {
    my $self = shift;
    if (@_) {
	my $name = shift;
	$self->{NAME} = $name;
    }
    return $self->{NAME};
}

sub depname {
    my $self = shift;
    if (@_) {
	my $depname = shift;
	$self->{DEPNAME} = $depname;
	# print STDERR "Setting \$namehasn{$depname} to $self\n";
	$namehash{$depname} = \$self;
    }
    return $self->{DEPNAME};
}

sub reparent {
    my $self = shift;
    my $name = shift;

    my $node = ${findname($name)};
    bless("HeaderDoc::Dependency", $node);
    my $oldparent = $node->parent;

    my @children = @{$oldparent->{CHILDREN}};
    my @newkids = ();
    foreach my $childref (@children) {
	if ($childref != \$node) {
		push(@newkids, $childref);
	}
    }
    $oldparent->{CHILDREN} = @newkids;
    $self->addchild($node);
}

sub dbprint {
    my $self = shift;
    my $indent = "";
    if (@_) {
	$indent = shift;
    }

    print STDERR $indent."o---+".$self->{NAME}." (DEPTH ".$self->{DEPTH}.")\n";
    if ($self->{PRINTED}) {
	print STDERR $indent."    |--- Infinite recursion detected.  Aborting.\n";
	return;
    }

    my $childindent = $indent."|   ";
    $self->{PRINTED} = 1;

    foreach my $childref (@{$self->{CHILDREN}}) {
	my $childnode = ${$childref};
	bless($childnode, "HeaderDoc::Dependency");
	$childnode->dbprint($childindent);
    }
    # $self->{PRINTED} = 0;
    print STDERR "$indent\n";
}

1;

