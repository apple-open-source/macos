#! /usr/bin/perl -w
#
# Class name: TypeHelper
# Synopsis: Helper code for block parser data type returns
#
# Last Updated: $Date: 2009/03/30 19:38:52 $
# 
# Copyright (c) 2006 Apple Computer, Inc.  All rights reserved.
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
package HeaderDoc::TypeHelper;

BEGIN {
	foreach (qw(Mac::Files)) {
	    $MOD_AVAIL{$_} = eval "use $_; 1";
    }
}
use HeaderDoc::HeaderElement;
use HeaderDoc::DBLookup;
use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc convertCharsForFileMaker printArray printHash resolveLink quote sanitize);
use File::Basename;
use Cwd;
use Carp qw(cluck);

use strict;
use vars qw($VERSION @ISA);
$HeaderDoc::TypeHelper::VERSION = '$Revision: 1.2 $';

# Inheritance
# @ISA = qw(HeaderDoc::HeaderElement);
################ Portability ###################################
my $isMacOS;
my $pathSeparator;
if ($^O =~ /MacOS/io) {
	$pathSeparator = ":";
	$isMacOS = 1;
} else {
	$pathSeparator = "/";
	$isMacOS = 0;
}
################ General Constants ###################################
my $debugging = 0;
my $theTime = time();
my ($sec, $min, $hour, $dom, $moy, $year, @rest);
($sec, $min, $hour, $dom, $moy, $year, @rest) = localtime($theTime);
# $moy++;
$year += 1900;
my $dateStamp = HeaderDoc::HeaderElement::strdate($moy, $dom, $year);
######################################################################

my $depth = 0;

sub new {
    my($param) = shift;
    my($class) = ref($param) || $param; 
    my $self = {}; 
    
    bless($self, $class);
    $self->_initialize();
    return($self);
} 

# class variables and accessors
{
    sub _initialize
    {
	my ($self) = shift;
	$self->{NAME} = 0;
	$self->{TYPE} = 0;
	$self->{POSSTYPES} = "";
    }
}

1;

