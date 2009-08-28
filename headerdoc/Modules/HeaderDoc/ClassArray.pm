#! /usr/bin/perl -w
#
# Class name: ClassArray
# Synopsis: Holds info about a class line array
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
package HeaderDoc::ClassArray;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash);

# use HeaderDoc::HeaderElement;
# use HeaderDoc::MinorAPIElement;
# use HeaderDoc::APIOwner;

# @ISA = qw( HeaderDoc::HeaderElement );

use strict;
use vars qw($VERSION @ISA);
$HeaderDoc::ClassArray::VERSION = '$Revision: 1.3 $';

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
    
    # $self->SUPER::_initialize();
    $self->{NAME} = "";
    $self->{BRACECOUNT} = 0;
    $self->{LINEARRAY} = ();
}

sub getarray
{
    my $self = shift;

    return @{ $self->{LINEARRAY} };
}

sub pushlines
{
    my $self = shift;
    my @lines = shift;

    foreach my $line (@lines) {
	$self->push($line);
    }
}

sub push
{
    my $self = shift;
    my $line = shift;
    my $pushDebug = 0;

    if ($pushDebug) {
	my $bc = $self->bracecount();
	print STDERR "pushing (bc=$bc) $line\n";
    }

    push(@{ $self->{LINEARRAY} }, $line);
    return $line;
}

sub name {
    my $self = shift;
    if (@_) {
	$self->{NAME} = shift;
    }
    return $self->{NAME};
}

sub bracecount {
    my $self = shift;
    return $self->{BRACECOUNT};
}

sub bracecount_dec {
    my $self = shift;
    if (@_) {
	my $n = shift;
	$self->{BRACECOUNT} -= $n;
    } else {
	$self->{BRACECOUNT}--;
    }
    return $self->{BRACECOUNT};
}

sub bracecount_inc {
    my $self = shift;
    if (@_) {
	my $n = shift;
	$self->{BRACECOUNT} += $n;
    } else {
	$self->{BRACECOUNT}++;
    }
    return $self->{BRACECOUNT};
}

1;

