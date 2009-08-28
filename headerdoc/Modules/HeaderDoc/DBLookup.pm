#! /usr/bin/perl -w
#
# Class name: DBLookup
# Synopsis: Encapsulates the look-up tables and routines to get info 
#           from database output. Since we need only one of these DBLookups
#           we implement only class methods
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

## 
## This module is slated to be removed in future releases. It was used 
## for special purpose dumps of the documentation, and is no longer needed.   
## If you rely on this module, please send a note to matt@apple.com and
## we can reconsider its future.
## 


package HeaderDoc::DBLookup;

use strict;
use vars qw($VERSION @ISA);
$HeaderDoc::DBLookup::VERSION = '$Revision: 1.8 $';

################ Portability ###################################
my $isMacOS;
my $pathSeparator;
if ($^O =~ /MacOS/i) {
	$pathSeparator = ":";
	$isMacOS = 1;
} else {
	$pathSeparator = "/";
	$isMacOS = 0;
}

################ General Constants ###################################
my $debugging = 0;

# my $theTime = time();
# my ($sec, $min, $hour, $dom, $moy, $year, @rest);
# ($sec, $min, $hour, $dom, $moy, $year, @rest) = localtime($theTime);
# $moy++;
# $year += 1900;
# my $dateStamp = "$moy/$dom/$year";
######################################################################

################ Lookup Hashes ###################################
my %datatypeNameToIDHash;
my %functionNameToIDHash;
######################################################################


sub loadUsingFolderAndFiles {
    my($class) = shift;
    my $folder = shift;
	my $functionFilename = shift;
	my $typesFilename = shift;
	my $enumsFilename = shift;

    if (ref $class)  { die "Class method called as object method" };
    
	###################### Read in lookup table of functionID to name ######################
	my $functionTable = $folder.$pathSeparator.$functionFilename;
	open(FUNCIDS, "<$functionTable") || die "Can't open $functionTable.\n";
	my @funcIDLines = <FUNCIDS>;
	close FUNCIDS;
	foreach my $line (@funcIDLines) {
	    if ($line =~/^#/) {next;};
	    chomp $line;
	    my ($funcID, $funcName);
	    ($funcID, $funcName) = split (/\t/, $line);
	    if (length($funcID)) {
	        $functionNameToIDHash{$funcName} = $funcID;
	    }
	}
	undef @funcIDLines;
	
	###################### Read in lookup table of typeID to name ######################
	my $typeTable = $folder.$pathSeparator.$typesFilename;
	open(TYPEIDS, "<$typeTable") || die "Can't open $typeTable.\n";
	my @typeIDLines = <TYPEIDS>;
	close TYPEIDS;
	foreach my $line (@typeIDLines) {
	    if ($line =~/^#/) {next;};
	    chomp $line;
	    my ($typeID, $typeName);
	    ($typeID, $typeName) = split (/\t/, $line);
	    if (length($typeID)) {
	        $datatypeNameToIDHash{$typeName} = $typeID;
	    }
	}
	undef @typeIDLines;
	
	###################### Read in lookup table of enumID to name ######################
	##### Add this to the types lookup since enums are often identified by the name of their first constant #####
	my $enumTable = $folder.$pathSeparator.$enumsFilename;
	open(ENUMIDS, "<$enumTable") || die "Can't open $enumTable.\n";
	my @enumIDLines = <ENUMIDS>;
	close ENUMIDS;
	foreach my $line (@enumIDLines) {
	    if ($line =~/^#/) {next;};
	    chomp $line;
	    my ($enumID, $enumName);
	    ($enumID, $enumName) = split (/\t/, $line);
	    if (length($enumID)) {
	        $datatypeNameToIDHash{$enumName} = $enumID;
	    }
	}
	undef @enumIDLines;
}

sub dataTypeNameToIDHash {
    return %datatypeNameToIDHash;
}

sub functionNameToIDHash {
    return %functionNameToIDHash;
}

sub functionIDForName {
    my $class = shift;
    my $name = shift;
    
    if (exists ($functionNameToIDHash{$name})) {
		return $functionNameToIDHash{$name};
    } else {
        return "UNKNOWN_ID";
    }
}

sub typeIDForName {
    my $class = shift;
    my $name = shift;
    
    if (exists ($datatypeNameToIDHash{$name})) {
		return $datatypeNameToIDHash{$name};
    } else {
        return "UNKNOWN_ID";
    }
}

1;
