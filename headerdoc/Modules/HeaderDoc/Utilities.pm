#! /usr/bin/perl
# Utilities.pm
# 
# Common subroutines
# Last Updated: $Date: 2001/03/22 02:27:13 $
# 
# Copyright (c) 1999 Apple Computer, Inc.  All Rights Reserved.
# The contents of this file constitute Original Code as defined in and are
# subject to the Apple Public Source License Version 1.1 (the "License").
# You may not use this file except in compliance with the License.  Please
# obtain a copy of the License at http://www.apple.com/publicsource and
# read it before using this file.
#
# This Original Code and all software distributed under the License are
# distributed on an TAS ISU basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the License for
# the specific language governing rights and limitations under the
# License.
#
######################################################################

package HeaderDoc::Utilities;
use strict;
use vars qw(@ISA @EXPORT $VERSION);
use Carp;
use Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(findRelativePath safeName getAPINameAndDisc convertCharsForFileMaker 
			 printArray printHash updateHashFromConfigFiles getHashFromConfigFile);
$VERSION = 1.00;

########## Portability ##############################
my $pathSeparator;
my $isMacOS;
BEGIN {
	if ($^O =~ /MacOS/i) {
		$pathSeparator = ":";
		$isMacOS = 1;
	} else {
		$pathSeparator = "/";
		$isMacOS = 0;
	}
}

sub findRelativePath {
    my ($fromMe, $toMe) = @_;	
	my @fromMeParts = split (/$pathSeparator/, $fromMe);
	my @toMeParts = split (/$pathSeparator/, $toMe);
	my $localDebug = 0;
	
	print "fromMe --> |$fromMe|\n" if $localDebug;
	print "toMe --> |$toMe|\n" if $localDebug;
	
	# find number of identical parts
	my $i = 0;
	while (($fromMeParts[$i] eq $toMeParts[$i]) && ($i < $#fromMeParts)) {
	    print "$i\n" if $localDebug; 
	    $i++;
	}
	@fromMeParts = splice (@fromMeParts, $i);
	@toMeParts = splice (@toMeParts, $i);
    my $numFromMeParts = @fromMeParts; #number of unique elements left in fromMeParts
  	my $relPart = "../" x ($numFromMeParts - 1);
	my $relPath = $relPart.join("/", @toMeParts);
	return $relPath;
}


# this version of safeName doesn't guard against name collisions
sub safeName {
    my ($filename) = @_;
    my $returnedName="";
    my $safeLimit;
    my $macFileLengthLimit = 31;
    my $longestExtension = 5;
    my $partLength;
    my $nameLength;

    $safeLimit = ($macFileLengthLimit - $longestExtension);
    $partLength = int (($safeLimit/2)-1);

    $filename =~ tr/a-zA-Z0-9./_/cs; # ensure name is entirely alphanumeric
    
    # check for length problems
    $nameLength = length($filename);
    if ($nameLength > $safeLimit) {
        my $safeName =  $filename;
        $safeName =~ s/^(.{$partLength}).*(.{$partLength})$/$1_$2/;
        $returnedName = $safeName;       
    } else {
        $returnedName = $filename;       
    }
    return $returnedName;    
}

sub getAPINameAndDisc {
    my $line = shift;
    my ($name, $disc, $operator);
    # first, get rid of leading space
    $line =~ s/^\s+//;
    ($name, $disc) = split (/\s/, $line, 2);
    
    if ($name =~ /operator/) {  # this is for operator overloading in C++
        ($operator, $name, $disc) = split (/\s/, $line, 3);
        $name = $operator." ".$name;
    }
    return ($name, $disc);
}

sub convertCharsForFileMaker {
    my $line = shift;
    $line =~ s/\t/chr(198)/eg;
    $line =~ s/\n/chr(194)/eg;
    return $line;
}

sub updateHashFromConfigFiles {
    my $configHashRef = shift;
    my $fileArrayRef = shift;
    
    foreach my $file (@{$fileArrayRef}) {
    	my %hash = &getHashFromConfigFile($file);
    	%{$configHashRef} = (%{$configHashRef}, %hash); # updates configHash from hash
    }
    return %{$configHashRef};
}

sub getHashFromConfigFile {
    my $configFile = shift;
    my %hash;
    my $localDebug = 0;
    my @lines;
    
    if ((-e $configFile) && (-f $configFile)) {
		open(INFILE, "<$configFile") || die "Can't open $configFile.\n";
		@lines = <INFILE>;
		close INFILE;
    } else {
        print "No configuration file found at $configFile\n" if ($localDebug);
        return;
    }
    
	foreach my $line (@lines) {
	    if ($line =~/^#/) {next;};
	    chomp $line;
	    my ($key, $value) = split (/\s*=>\s*/, $line);
	    if ((defined($key)) && (length($key))){
		    $hash{$key} = $value;
		}
	}
	undef @lines;
	return %hash;
}


############### Debugging Routines ########################
sub printArray {
    my (@theArray) = @_;
    my $i= 0;
    my $length = @theArray;
    
    while ($i < $length) {
	    print ("\t$theArray[$i++]\n");
    }
}

sub printHash {
    my (%theHash) = @_;
    print ("Printing contents of hash:\n");
    foreach my $keyword (keys(%theHash)) {
	print ("$keyword => $theHash{$keyword}\n");
    }
    print("-----------------------------------\n\n");
}

1;

__END__

