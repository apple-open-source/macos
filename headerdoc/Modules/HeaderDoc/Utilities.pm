#! /usr/bin/perl -w
# Utilities.pm
# 
# Common subroutines
# Last Updated: $Date: 2003/08/01 21:23:56 $
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
foreach (qw(Mac::Files Mac::MoreFiles)) {
    eval "use $_";
}

$VERSION = 1.02;
@ISA = qw(Exporter);
@EXPORT = qw(findRelativePath safeName safeNameNoCollide linesFromFile makeAbsolutePath
             printHash printArray fileNameFromPath folderPathForFile convertCharsForFileMaker 
             updateHashFromConfigFiles getHashFromConfigFile getVarNameAndDisc
             getAPINameAndDisc openLogs
             logMsg logMsgAndWarning logWarning logToAllFiles closeLogs);

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
########## Name length constants ##############################
my $macFileLengthLimit;
BEGIN {
	if ($isMacOS) {
		$macFileLengthLimit = 31;
	} else {
		$macFileLengthLimit = 255;
	}
}
my $longestExtension = 5;
###############################################################

########### Log File Handling  ################################
my $logFile;
my $warningsFile;
###############################################################

sub openLogs {
    $logFile = shift;
    $warningsFile = shift;
    
    if (-e $logFile) {
        unlink $logFile || die "Couldn't delete old log file $logFile\n";
    }
    
    if (-e $warningsFile) {
        unlink $warningsFile || die "Couldn't delete old log file $warningsFile\n";
    }
    
	open(LOGFILE, ">$logFile") || die "Can't open output file $logFile.\n";
	if ($isMacOS) {MacPerl::SetFileInfo('R*ch', 'TEXT', $logFile);};

	open(WARNINGSFILE, ">$warningsFile") || die "Can't open output file $warningsFile.\n";
	if ($isMacOS) {MacPerl::SetFileInfo('R*ch', 'TEXT', $warningsFile);};
}

sub logMsg {
    my $msg = shift;
    my $toConsole = shift;
    
    if ($toConsole) {
	    print "$msg";
    }
    print LOGFILE "$msg";
}

sub logWarning {
    my $msg = shift;
    my $toConsole = shift;
    
    if ($toConsole) {
	    print "$msg";
    }
    print LOGFILE "$msg";
    print WARNINGSFILE "$msg";
}

sub logToAllFiles {  # print to all outs, without the "warning" overtone
    my $msg = shift;    
    &logWarning($msg, 1);
}

sub closeLogs {
	close LOGFILE;
	close WARNINGSFILE;
	undef $logFile;
	undef $warningsFile;
}

sub findRelativePath {
    my ($fromMe, $toMe) = @_;
    if ($fromMe eq $toMe) {return "";}; # link to same file
	my @fromMeParts = split (/$pathSeparator/, $fromMe);
	my @toMeParts = split (/$pathSeparator/, $toMe);
	
	# find number of identical parts
	my $i = 0;
	# figure out why perl complain of uninitialized var in while loop
	my $oldWarningLevel = $^W;
	{
	    $^W = 0;
		while ($fromMeParts[$i] eq $toMeParts[$i]) { $i++;};
	}
	$^W = $oldWarningLevel;
	
	@fromMeParts = splice (@fromMeParts, $i);
	@toMeParts = splice (@toMeParts, $i);
    my $numFromMeParts = @fromMeParts; #number of unique elements left in fromMeParts
  	my $relPart = "../" x ($numFromMeParts - 1);
	my $relPath = $relPart.join("/", @toMeParts);
	return $relPath;
}

sub fileNameFromPath {
    my $path = shift;
    my @pathParts = split (/$pathSeparator/, $path);
	my $fileName = pop (@pathParts);
    return $fileName;
}

sub folderPathForFile {
    my $path = shift;
    my @pathParts = split (/$pathSeparator/, $path);
	my $fileName = pop (@pathParts);
    my $folderPath = join("$pathSeparator", @pathParts);
    return $folderPath;
}

# set up default values for safeName and safeNameNoCollide
my %safeNameDefaults  = (filename => "", fileLengthLimit =>"$macFileLengthLimit", longestExtension => "$longestExtension");

sub safeName {
    my %args = (%safeNameDefaults, @_);
    my ($filename) = $args{"filename"};
    my $returnedName="";
    my $safeLimit;
    my $partLength;
    my $nameLength;

    $safeLimit = ($args{"fileLengthLimit"} - $args{"longestExtension"});
    $partLength = int (($safeLimit/2)-1);

    $filename =~ tr/a-zA-Z0-9./_/cs; # ensure name is entirely alphanumeric
    $nameLength = ($filename =~ tr/a-zA-Z0-9._//);

    #check for length problems
    if ( $nameLength > $safeLimit) {
        my $safeName =  $filename;
        $safeName =~ s/^(.{$partLength}).*(.{$partLength})$/$1_$2/;
        $returnedName = $safeName;       
    } else {
        $returnedName = $filename;       
    }
    return $returnedName;
    
}


my %dispensedSafeNames;

sub safeNameNoCollide {
    my %args = (%safeNameDefaults, @_);
    
    my ($filename) = $args{"filename"};
    my $returnedName="";
    my $safeLimit;
    my $partLength;
    my $nameLength;
    my $localDebug = 0;
    
    $filename =~ tr/a-zA-Z0-9./_/cs; # ensure name is entirely alphanumeric
    # check if name would collide case insensitively
    if (exists $dispensedSafeNames{lc($filename)}) {
        while (exists $dispensedSafeNames{lc($filename)}) {
            # increment numeric part of name
            $filename =~ /(\D+)(\d*)((\.\w*)*)/;
            my $rootTextPart = $1;
            my $rootNumPart = $2;
            my $extension = $4;
            if (defined $rootNumPart) {
                $rootNumPart++;
            } else {
                $rootNumPart = 2
            }
            if (!$extension){$extension = '';};
            $filename = $rootTextPart.$rootNumPart.$extension;
        }
    }
    $returnedName = $filename;       

    # check for length problems
    $safeLimit = ($args{"fileLengthLimit"} - $args{"longestExtension"});
    $partLength = int (($safeLimit/2)-1);
    $nameLength = length($filename);
    if ($nameLength > $safeLimit) {
        my $safeName =  $filename;
        $safeName =~ s/^(.{$partLength}).*(.{$partLength})$/$1_$2/;
        if (exists $dispensedSafeNames{lc($safeName)}) {
            my $i = 1;
	        while (exists $dispensedSafeNames{lc($safeName)}) {
	            $safeName =~ s/^(.{$partLength}).*(.{$partLength})$/$1$i$2/;
	            $i++;
	        }
	    }
        my $lcSafename = lc($safeName);
        print "\t $lcSafename\n" if ($localDebug);
        $returnedName = $safeName;       
    } else {
        $returnedName = $filename;       
    }
    $dispensedSafeNames{lc($returnedName)}++;
    return $returnedName;    
}

#sub linesFromFile {
#	my $filePath = shift;
#	my $oldRecSep = $/;
#	my $fileString;
#	
#	undef $/; # read in files as strings
#	open(INFILE, "<$filePath") || die "Can't open $filePath.\n";
#	$fileString = <INFILE>;
#    $fileString =~ s/\015/\n/g;
#	close INFILE;
#	$/ = $oldRecSep;
#	return (split (/\n/, $fileString));
#}
#
sub makeAbsolutePath {
   my $relPath = shift;
   my $relTo = shift;
   if ($relPath !~ /^\//) { # doesn't start with a slash
       $relPath = $relTo."/".$relPath;
   }
   return $relPath;
}

sub getAPINameAndDisc {
    my $line = shift;
    return getNameAndDisc($line, 0);
}

sub getVarNameAndDisc {
    my $line = shift;
    return getNameAndDisc($line, 1);
}

sub getNameAndDisc {
    my $line = shift;
    my $multiword = shift;
    my ($name, $disc, $operator);
    # first, get rid of leading space
    $line =~ s/^\s+//;
    # DAG changed \s in next line to \n to fix bug w/ multi-word names
    # but have to keep it as \n for @var to allow these very simple
    # descriptions to be single-line (as described in the docs)
    if ($multiword) {
	($name, $disc) = split (/\n/, $line, 2);
    } else {
	($name, $disc) = split (/\s/, $line, 2);
    }
    
    if ($name =~ /operator/) {  # this is for operator overloading in C++
        ($operator, $name, $disc) = split (/\s/, $line, 3);
        $name = $operator." ".$name;
    }
    # print "name is $name, disc is $disc";
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
    	print "reading $configFile\n" if ($localDebug);
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
			print "    $key => $value\n" if ($localDebug);
		    $hash{$key} = $value;
		}
	}
	undef @lines;
	return %hash;
}

sub linesFromFile {
	my $filePath = shift;
	my $oldRecSep;
	my $fileString;
	
	$oldRecSep = $/;
	undef $/;    # read in files as strings
	open(INFILE, "<$filePath") || die "Can't open $filePath: $!\n";
	$fileString = <INFILE>;
	close INFILE;
	$/ = $oldRecSep;

	$fileString =~ s/\015\012/\n/g;
	$fileString =~ s/\r/\n/g;
	my @lineArray = split (/\n/, $fileString);
	
	# put the newline back on the end of each element of the array
	# we can't use split (/(\n)/, $fileString); because that adds the 
	# newlines as new elements in the array.
	return map($_."\n", @lineArray);
}

############### Debugging Routines ########################
sub printArray {
    my (@theArray) = @_;
    my ($i, $length);
    $i = 0;
    $length = @theArray;
    
    print ("Printing contents of array:\n");
    while ($i < $length) {
	print ("Element $i ---> |$theArray[$i++]|\n");
    }
    print("\n\n");
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

