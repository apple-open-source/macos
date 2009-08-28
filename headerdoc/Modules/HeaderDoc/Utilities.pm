#! /usr/bin/perl -w
# Utilities.pm
# 
# Common subroutines
# Last Updated: $Date: 2009/04/13 22:19:31 $
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

package HeaderDoc::Utilities;
use strict;
use vars qw(@ISA @EXPORT $VERSION);
use Carp qw(cluck);

use Cwd;
use Encode;
use Encode::Guess;

use Exporter;
foreach (qw(Mac::Files Mac::MoreFiles)) {
    eval "use $_";
}

$HeaderDoc::Utilities::VERSION = '$Revision: 1.45 $';
@ISA = qw(Exporter);
@EXPORT = qw(findRelativePath safeName safeNameNoCollide linesFromFile makeAbsolutePath
             printHash printArray fileNameFromPath folderPathForFile convertCharsForFileMaker 
             updateHashFromConfigFiles getHashFromConfigFile getVarNameAndDisc
             getAPINameAndDisc openLogs
             logMsg logMsgAndWarning logWarning logToAllFiles closeLogs
             registerUID resolveLink quote parseTokens isKeyword html2xhtml
             resolveLinks stringToFields sanitize warnHDComment
             classTypeFromFieldAndBPinfo get_super casecmp unregisterUID
	     unregister_force_uid_clear dereferenceUIDObject validTag emptyHDok
	     addAvailabilityMacro complexAvailabilityToArray
	     filterHeaderDocComment filterHeaderDocTagContents processTopLevel
	     processHeaderComment getLineArrays objectForUID
	     loadHashes saveHashes getAbsPath allow_everything
	     getAvailabilityMacros);

my %uid_list_by_uid = ();
my %uid_list = ();
my %uid_conflict = ();
my $xmllintversion = "";
my $xmllint = "/usr/bin/xmllint";

my %objid_hash;

########## Portability ##############################
my $pathSeparator;
my $isMacOS;
BEGIN {
	if ($^O =~ /MacOS/io) {
		$pathSeparator = ":";
		$isMacOS = 1;
	} else {
		$pathSeparator = "/";
		$isMacOS = 0;
	}
}

$xmllint = "/usr/bin/xmllint";

if ( -x "/usr/local/bin/xmllint" ) {
	$xmllint = "/usr/local/bin/xmllint";
} elsif (-x "/sw/bin/xmllint" ) {
	$xmllint = "/sw/bin/xmllint";
}

open(XMLLINTPIPE, "$xmllint --version 2>&1 |");
$xmllintversion = <XMLLINTPIPE>;
close(XMLLINTPIPE);
# print STDERR "STRING \"$xmllintversion\".\n";
$xmllintversion =~ s/\n.*//sg;
$xmllintversion =~ s/.*?(\d+)/$1/s;
if ($xmllintversion eq "20607") {
	warn "Old LibXML2 version.  XML Output may not work correctly.\n";
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
	    print STDERR "$msg";
    }
    print LOGFILE "$msg";
}

sub logWarning {
    my $msg = shift;
    my $toConsole = shift;
    
    if ($toConsole) {
	    print STDERR "$msg";
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
            $filename =~ /(\D+)(\d*)((\.\w*)*)/o;
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
        print STDERR "\t $lcSafename\n" if ($localDebug);
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
#    $fileString =~ s/\015/\n/go;
#	close INFILE;
#	$/ = $oldRecSep;
#	return (split (/\n/, $fileString));
#}
#
sub makeAbsolutePath {
   my $relPath = shift;
   my $relTo = shift;
   if ($relPath !~ /^\//o) { # doesn't start with a slash
       $relPath = $relTo."/".$relPath;
   }
   return $relPath;
}

# /*! This function gets an API name and discussion from a tag, e.g. \@function.
#  *  The second parameter is the contents of a regular expression.  If nonempty,
#  *  This expression determines a list of tokens which are considered to
#  *  automatically get merged with the name if they appear before or after a
#  *  space that would otherwise terminate the name.  This allows a space prior
#  *  to a leading parenthesis in a category name, for example.
#  */
sub getAPINameAndDisc {
    my $line = shift;
    my $joinpattern = shift;
    my ($name, $disc, $operator);
    my $localDebug = 0;

    # If we start with a newline (e.g.
    #     @function
    #       discussion...
    # treat it like JavaDoc and let the block parser
    # pick up a name.
    print STDERR "LINE: $line\n" if ($localDebug);
    if ($line =~ /^\s*\n\s*/o) {
	print STDERR "returning discussion only.\n" if ($localDebug);
	$line =~ s/^\s+//o;
	return ("", "$line", 0);
    }
    my $nameline = 0;
    # otherwise, get rid of leading space
    $line =~ s/^\s+//o;

    # If we have something like
    #
    #    @define this that
    #     Description here
    #
    # we split on the newline, else split on the first
    # whitespace.
    if ($line =~ /\S+.*\n.*\S+/o) {
	$nameline = 0;
	($name, $disc) = split (/\n/, $line, 2);
    } else {
	$nameline = 1;
	($name, $disc) = smartsplit($line, $joinpattern);
    }

	# print STDERR "NAME: $name DISC: $disc\n";
    # ensure that if the discussion is empty, we return an empty
    # string....
    $disc =~ s/\s*$//o;
    
    if ($name =~ /operator/o) {  # this is for operator overloading in C++
        ($operator, $name, $disc) = split (/\s/, $line, 3);
        $name = $operator." ".$name;
    }
    print STDERR "name is $name, disc is $disc, nameline is $nameline" if ($localDebug);
    return ($name, $disc, $nameline);
}

sub smartsplit
{
    my $line = shift;
    my $pattern = shift;
    my $localDebug = 0;

    print STDERR "LINE: $line\n" if ($localDebug);
    print STDERR "PATTERN: $pattern\n" if ($localDebug);

    # The easy case.
    if (!$pattern || $pattern eq "") {
	return split (/\s/, $line, 2);
    }

    # The hard case.
    my @parts = split(/(\s+|$pattern)/, $line);

    my $leading = 1;
    my $lastspace = "";
    my $name = "";
    my $desc = "";
    my @matchstack = ();
    foreach my $part (@parts) {
	if ($part eq "") { next; }
	print STDERR "PART: $part\n" if ($localDebug);
	if ($desc eq "") {
		print STDERR "Working on name.\n" if ($localDebug);
		if ($part =~ /\s/) {
			if ($leading) {
				print STDERR "Clear leading (space).\n" if ($localDebug);
				$name .= $part;
				$leading = 0;
			} else {
				print STDERR "Set lastspace.\n" if ($localDebug);
				$lastspace = $part;
			}
		} else {
			if ($leading) {
				print STDERR "Clear leading (text).\n" if ($localDebug);
				$leading = 0;
				$name .= $part;
			} else {
				if ($part =~ /($pattern)/) {
					print STDERR "Appending to name (pattern match).\n" if ($localDebug);
					$name .= $lastspace.$part;
					$lastspace = "";
					$leading = 1;

					my $isbrace = HeaderDoc::BlockParse::bracematching($part);
					# print STDERR "IB: \"$isbrace\"\n" if ($localDebug);
					if ($isbrace ne "") {
						print STDERR "Adding to match stack\n" if ($localDebug);
						push(@matchstack, $part);
					} elsif ($part eq HeaderDoc::BlockParse::peekmatch(\@matchstack)) {
						print STDERR "Popping from match stack\n" if ($localDebug);
						pop(@matchstack);
					}
				} elsif (scalar(@matchstack)) {
					print STDERR "Stack not empty.  Appending to name\n" if ($localDebug);
					$name .= $lastspace.$part;
					$lastspace = "";
					$leading = 1;
				} elsif ($lastspace eq "") {
					print STDERR "Appending to name.\n" if ($localDebug);
					$name .= $part;
				} else {
					print STDERR "Starting description.\n" if ($localDebug);
					$desc = $part;
				}
			}
		}
	} else {
		$desc .= $part;
	}
    }

    $name =~ s/^\s*//s;

    print STDERR "Returning NAME: $name DESC: $desc\n" if ($localDebug);

    return ($name, $desc);
}

sub convertCharsForFileMaker {
    my $line = shift;
    $line =~ s/\t/chr(198)/ego;
    $line =~ s/\n/chr(194)/ego;
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
    	print STDERR "reading $configFile\n" if ($localDebug);
		open(INFILE, "<$configFile") || die "Can't open $configFile.\n";
		@lines = <INFILE>;
		close INFILE;
    } else {
        print STDERR "No configuration file found at $configFile\n" if ($localDebug);
        return;
    }
    
	foreach my $line (@lines) {
	    if ($line =~/^#/o) {next;};
	    chomp $line;
	    my ($key, $value) = split (/\s*=>\s*/, $line);
	    if ((defined($key)) && (length($key))){
			print STDERR "    $key => $value\n" if ($localDebug);
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
	if (!open(INFILE, "<$filePath")) {
		$HeaderDoc::exitstatus = -1;
		warn "Can't open $filePath: $!\n";
		return ();
	}
	$fileString = <INFILE>;
	close INFILE;
	$/ = $oldRecSep;

	my $encDebug = 0;

	print STDERR "POINT 1\n" if ($encDebug);

	my $decoder = guess_encoding($fileString, qw/iso-8859-1 UTF-8/);

print STDERR "FILEPATH $filePath DECODER: $decoder\n" if ($encDebug);

	if ($decoder =~ /utf8/ && $decoder =~ /iso-8859-1/) {
		# Doesn't matter which we pick.  Guess UTF-8.
		print STDERR "Could be UTF-8 or ISO-8859-1.  Going with UTF-8.\n" if ($encDebug);
		$decoder = guess_encoding($fileString);
	}

	print STDERR "POINT 2\n" if ($encDebug);
	if (!ref($decoder)) {
		$decoder = guess_encoding($fileString, qw/Windows-1252/);
	}

	print STDERR "POINT 3\n" if ($encDebug);
	if (!ref($decoder)) {
		$decoder = guess_encoding($fileString, qw/euc-jp shiftjis 7bit-jis/);
	}
	print STDERR "POINT 4\n" if ($encDebug);
	if (!ref($decoder)) {
		$decoder = guess_encoding($fileString, qw/euc-jp shiftjis 7bit-jis/);
	}
	print STDERR "POINT 5\n" if ($encDebug);
	if (!ref($decoder)) {
		$decoder = guess_encoding($fileString, qw/iso-8859-2/);
	}
	print STDERR "POINT 6\n" if ($encDebug);
	if (!ref($decoder)) {
		$decoder = guess_encoding($fileString, qw/iso-8859-3/);
	}
	print STDERR "POINT 7\n" if ($encDebug);
	if (!ref($decoder)) {
		$decoder = guess_encoding($fileString, qw/iso-8859-4/);
	}
	print STDERR "POINT 8\n" if ($encDebug);
	if (!ref($decoder)) {
		$decoder = guess_encoding($fileString, qw/iso-8859-5/);
	}
	print STDERR "POINT 9\n" if ($encDebug);
	if (!ref($decoder)) {
		$decoder = guess_encoding($fileString, qw/iso-8859-6/);
	}
	print STDERR "POINT 10\n" if ($encDebug);
	if (!ref($decoder)) {
		$decoder = guess_encoding($fileString, qw/iso-8859-7/);
	}
	print STDERR "POINT 11\n" if ($encDebug);
	if (!ref($decoder)) {
		$decoder = guess_encoding($fileString, qw/iso-8859-8/);
	}
	print STDERR "POINT 12\n" if ($encDebug);
	if (!ref($decoder)) {
		$decoder = guess_encoding($fileString, qw/iso-8859-9/);
	}
	print STDERR "POINT 13\n" if ($encDebug);
	if (!ref($decoder)) {
		$decoder = guess_encoding($fileString, qw/iso-8859-10/);
	}
	print STDERR "POINT 14\n" if ($encDebug);
	if (!ref($decoder)) {
		$decoder = guess_encoding($fileString, qw/iso-8859-11/);
	}
	print STDERR "POINT 15\n" if ($encDebug);
	# if (!ref($decoder)) {
		# $decoder = guess_encoding($fileString, qw/iso-8859-12/);
	# }
	# print STDERR "POINT 16\n" if ($encDebug);
	# if (!ref($decoder)) {
		# $decoder = guess_encoding($fileString, qw/iso-8859-13/);
	# }
	# print STDERR "POINT 17\n" if ($encDebug);
	# if (!ref($decoder)) {
		# $decoder = guess_encoding($fileString, qw/iso-8859-14/);
	# }
	# print STDERR "POINT 18\n" if ($encDebug);
	# if (!ref($decoder)) {
		# $decoder = guess_encoding($fileString, qw/iso-8859-15/);
	# }
	print STDERR "POINT 19\n" if ($encDebug);

	# if (!ref($decoder)) {
		# $decoder = guess_encoding($fileString, qw/UTF-8 Windows-1252 euc-jp shiftjis 7bit-jis iso-8859-1 iso-8859-2 iso-8859-3 iso-8859-4 iso-8859-5 iso-8859-6 iso-8859-7 iso-8859-8 iso-8859-9 iso-8859-10 iso-8859-11 iso-8859-12 iso-8859-13 iso-8859-14 iso-8859-15/);
	# }
	ref($decoder) or die "Can't guess encoding: $decoder"; # trap error this way

	print STDERR "ENC is ".$decoder->name."\n" if ($encDebug);
	$HeaderDoc::lastFileEncoding = $decoder->name;

	# my $utf8 = $decoder->decode($fileString);
	# $fileString = $utf8;

	$fileString =~ s/\015\012/\n/go;
	$fileString =~ s/\r\n/\n/go;
	$fileString =~ s/\n\r/\n/go;
	$fileString =~ s/\r/\n/go;
	my @lineArray = split (/\n/, $fileString);
	
	# put the newline back on the end of each element of the array
	# we can't use split (/(\n)/, $fileString); because that adds the 
	# newlines as new elements in the array.
	return map($_."\n", @lineArray);
}

# This function supports the use of the @link tag to link to functions and
# types within a single file.  If you specify something like @link foo .... @/link,
# This code will get called.  If you specify an API ref instead of a bare symbol
# name, you should not even get here.
sub resolveLink
{
    my $symbol = shift;
    my $linkedword = "linked";
    if (@_) {
	$linkedword = shift;
    }
    my $ret = "";
    my $fullpath = $HeaderDoc::headerObject->fullpath();

    my $uid = $uid_list{$symbol};
	if ($uid && length($uid)) {
	    $ret = $uid;
	    if ($uid_conflict{$symbol}) {
		warn "$fullpath:0: warning: multiple matches found for symbol \"$symbol\"!!! Only the first matching symbol will be $linkedword. Replace the symbol with a specific api ref tag (e.g. apple_ref) in header file to fix this conflict.\n";
	    }
	}
    if ($ret eq "") {
        # If symbol is in an external API, resolution will be done
        # by resolveLinks, later; don't issue any warning yet.        
        if ($symbol !~ /^\/\//){
	       warn "$fullpath:0: warning: no symbol matching \"$symbol\" found.  If this symbol is not in this file or class, you need to specify it with an api ref tag (e.g. apple_ref).\n";
        }
	$ret = $symbol; # If $symbol is a uid, keep it as is
    # } else {
	# warn "RET IS \"$ret\"\n"
    }
    return $ret;
}

sub registerUID($$$)
{
    # This is now classless.
    # my $self = shift;
    my $uid = shift;
    my $name = shift;
    my $object = shift;
    my $localDebug = 0;

    if ($HeaderDoc::ignore_apiuid_errors == 2) { return; }
    if ($object->noRegisterUID()) { return; }

    my $objtype = ref($object) || $object;

    # Silently ignore objects that are going away anyway.
    if ($objtype =~ /HeaderDoc::HeaderElement/) { return; }
    if ($objtype =~ /HeaderDoc::APIOwner/) { return; }
    if ($uid =~ /^\/\/[^\/]+\/[^\/]+\/internal_temporary_object$/ || $uid =~ /^\/\/[^\/]+\/[^\/]+\/internal_temporary_object\/.*$/) { return; }

	# print STDERR "UID WAS $uid\n";

    print STDERR "OBJECT: $object\n" if ($localDebug);
    print STDERR "New UID registered: $object -> $uid.\n" if ($localDebug);
    cluck("New UID registered: $object -> $uid.  Backtrace follows\n") if ($localDebug);

    if ($uid_list_by_uid{$uid} != undef) {
    	if ($uid_list_by_uid{$uid} != $object) {
		# If we match, keep quiet.  This is normal.
		# Otherwise, resolve the duplicate apple_ref
		# below.
		# my $objid = "" . $object;
		# $objid =~ s/^.*\(//s;
		# $objid =~ s/\).*$//s;
		# my $objid = sanitize($object->apiOwner()->name(), 1)."_".$HeaderDoc::perHeaderObjectID++;
		my $objname = sanitize($uid, 1);
		my $objid = $objid_hash{$objname};
		if (!$objid) {
			$objid = 0;
		}
		$objid_hash{$objname} = $objid + 1;
		# print STDERR "NEXT for \"$objname\" WILL BE ".$objid_hash{$objname}."\n";
		my $newuid = $uid . "_DONTLINK_$objid";
		if ($uid_list_by_uid{$newuid} == undef) {
		    my $quiet = 0;
		    if ($HeaderDoc::test_mode) { $quiet = 1; }
		    # Avoid warning about methods before the return type
		    # has been set.
		    if ($object->can("returntype")) {
			if ($object->returntype() == undef) {
			    if ($objtype =~ /HeaderDoc::Method/) { $quiet = 1; }
			    if ($objtype =~ /HeaderDoc::Function/) {
				my $apio = $object->apiOwner();
				my $apioname = ref($apio) || $apio;
				if ($apioname !~ /HeaderDoc::Header/) { $quiet = 1; }
			    }
			}
		    }
		    if (!$quiet) {
			if ($newuid=~/^\/\/apple_ref\/doc\/title:(.*?)\//) {
				warn("Warning: same name used for more than one comment (base apple_ref type was $1)\n");
				warn("    UID changed from $uid to $newuid\n");
			} else {
				warn("Warning: UID $uid shared by multiple objects.  Disambiguating: new uid is $newuid\n");
			}
			if ($localDebug) { cluck("Backtrace follows\n"); }
		    }
		}
		$uid = $newuid;
		$uid_list_by_uid{$uid} = $object;
	}
    } else {
	$uid_list_by_uid{$uid} = $object;
    }


    print STDERR "registered UID $uid\n" if ($localDebug);
    # my $name = $uid;
    # $name =~ s/.*\///so;

    my $old_uid = $uid_list{$name};
    if ($old_uid && length($old_uid) && $old_uid ne $uid) {
	print STDERR "OU: $old_uid NU: $uid\n" if ($localDebug);
	$uid_conflict{$name} = 1;
    }
    $uid_list{$name} = $uid;
    # push(@uid_list, $uid);

    return $uid;
}

sub objectForUID
{
    my $uid = shift;
    return $uid_list_by_uid{$uid};
}

sub dereferenceUIDObject
{
    my $uid = shift;
    my $object = shift;

    if ( $uid_list_by_uid{$uid} == $object) {
	$uid_list_by_uid{$uid} = undef;
	$uid_list_by_uid{$uid} = 3;
	# print STDERR "Releasing object reference\n";
    # } else {
	# warn("Call to dereferenceUIDObject for non-matching object\n");
    }
}

sub unregisterUID
{
    my $uid = shift;
    my $name = shift;
    my $object = undef;
    if (@_) { $object = shift; }

    if ($HeaderDoc::ignore_apiuid_errors == 2) { return 0; }

    my $old_uid = $uid_list{$name};
    my $ret = 1;

    if ($uid_list{$name} eq $uid) {
	$uid_list{$name} = undef;
    } else {
	# warn("Attempt to unregister UID with wrong name: ".$uid_list{$name}." != $uid.\n");
	$ret = 0;
    }
    # if ($uid_list_by_uid{$uid} == $object) {
	# $uid_list_by_uid{$uid} = undef;
    # }

    return 0;
}

sub unregister_force_uid_clear
{
    my $uid = shift;
    $uid_list_by_uid{$uid} = undef;
}

sub quote
{
    my $input = shift;

    $input =~ s/(\W)/\\$1/go;

    return $input;
}

############### Debugging Routines ########################
sub printArray {
    my (@theArray) = @_;
    my ($i, $length);
    $i = 0;
    $length = @theArray;
    
    print STDERR "Printing contents of array:\n";
    while ($i < $length) {
	print STDERR  "Element $i ---> |$theArray[$i++]|\n";
    }
    print STDERR "\n\n";
}

sub printHash {
    my (%theHash) = @_;
    print STDERR "Printing contents of hash:\n";
    foreach my $keyword (keys(%theHash)) {
	print STDERR "$keyword => $theHash{$keyword}\n";
    }
    print STDERR "-----------------------------------\n\n";
}


sub parseTokens
{
    my $lang = shift;
    my $sublang = shift;

    my $localDebug = 0;
    my $sotemplate = "";
    my $eotemplate = "";
    my $soc = "";
    my $eoc = "";
    my $ilc = "";
    my $ilc_b = "";
    my $sofunction = "";
    my $soprocedure = "";
    my $operator = "";
    my $sopreproc = "";
    my $lbrace = "";
    my $rbrace = "";
    my $enumname = "enum";
    my $unionname = "union";
    my $structname = "struct";
    my $typedefname = "typedef";
    my $varname = "";
    my $constname = "";
    my $structisbrace = 0;
    my %macronames = ();
    my $classregexp = "";
    my $classbraceregexp = "";
    my $classclosebraceregexp = "";
    my $accessregexp = "";
    my $requiredregexp = "";
    my $propname = "";
    my $objcdynamicname = "";
    my $objcsynthesizename = "";
    my $moduleregexp = "";
    my $definename = "";	# Breaking this out so that we can abuse CPP
				# in IDL processing for cpp_quote without
				# actually allowing #define macros to be
				# parsed from the code.  This is only used for
				# code parsing, NOT for interpreting the
				# actual #define macros themselves!

    my $langDebug = 0;

    print STDERR "PARSETOKENS FOR lang: $lang sublang: $sublang\n" if ($localDebug);

    if ($lang eq "perl" || $lang eq "shell") {
	print STDERR "Language is Perl or Shell script.\n" if ($langDebug);
	$sotemplate = "";
	$eotemplate = "";
	$sopreproc = "";
	$soc = "";
	$eoc = "";
	$ilc = "#";
	if ($lang eq "perl") { $sofunction = "sub"; }
	else { $sofunction = "function"; }
	$lbrace = "{";
	$rbrace = "}";
	$enumname = "";
	$unionname = "";
	$structname = "";
	$typedefname = "";
	$varname = "";
	if ($lang eq "shell" && $sublang eq "csh") {
		# A variable that starts with "set" will "just work",
		# but a variable that starts with "setenv" has no
		# equals sign, so it needs help.
		$varname = "setenv";
	}
	$constname = "";
	$structisbrace = 0;
    } elsif ($lang eq "pascal") {
	print STDERR "Language is Pascal.\n" if ($langDebug);
	$sotemplate = "";
	$eotemplate = "";
	$sopreproc = "#"; # Some pascal implementations allow #include
	$soc = "{";
	$eoc = "}";
	$ilc = "";
	$sofunction = "function";
	$soprocedure = "procedure";
	$lbrace = "begin";
	$rbrace = "end";
	$enumname = "";
	$unionname = "";
	$structname = "record";
	$typedefname = "type";
	$varname = "var";
	$constname = "const";
	$structisbrace = 1;
    } else {
	# C and derivatives, plus PHP and Java(script)
	$classregexp = "^(class|namespace)\$";
	$moduleregexp = "^(namespace)\$";
	if ($lang eq "C") {
		$typedefname = "typedef";
	}
	if (($lang eq "C" && $sublang ne "php" && $sublang ne "IDL" && $sublang ne "MIG") || $lang =~ /Csource/) {
		print STDERR "Language is C or variant.\n" if ($langDebug);
		# if ($sublang eq "cpp" || $sublang eq "C") {
			$sotemplate = "<";
			$eotemplate = ">";
			$accessregexp = "^(public|private|protected)\$";
		# }
		$operator = "operator";
		$sopreproc = "#";
		if ($sublang eq "occ") {
			# @@@ Note: if C++ ever adopts package, add a question mark to this regexp.
			$accessregexp = "^(\@?public|\@?private|\@?protected|\@package)\$";
			$requiredregexp = "^(\@optional|\@required)\$";
			$propname = "\@property";
		}
	} elsif ($sublang eq "IDL") {
		print STDERR "Language is IDL.\n" if ($langDebug);
		$sopreproc = "#";
	} elsif ($sublang eq "MIG") {
		print STDERR "Language is MIG.\n" if ($langDebug);
		$sopreproc = "#";
		$typedefname = "type";
	} else {
		print STDERR "Language is Unknown.\n" if ($langDebug);
	}
	# warn("SL: $sublang\n");
	if ($lang eq "C" && $sublang ne "php" && $sublang ne "IDL") { # if ($sublang eq "occ" || $sublang eq "C")
		$classregexp = "^(class|\@class|\@interface|\@protocol|\@implementation|namespace)\$";
		$classbraceregexp = "^(\@interface|\@protocol|\@implementation)\$";
		$classclosebraceregexp = "^(\@end)\$";
	}
	if ($lang eq "C" && $sublang eq "IDL") {
		$classregexp = "^(module|interface)\$";
		$classbraceregexp = "";
		$classclosebraceregexp = "";
		$sotemplate = "["; # Okay, so not strictly speaking a template, but we don't
		$eotemplate = "]"; # care about what is in brackets.
		$moduleregexp = "^(module)\$";
	}
	if ($lang eq "java" && $sublang eq "java") {
		$classregexp = "^(class|interface|namespace)\$";
		$accessregexp = "^(public|private|protected|package)\$";
	} elsif ($sublang eq "php") {
		$accessregexp = "^(public|private|protected)\$";
		$ilc_b = "#";
	}
	$soc = "/*";
	$eoc = "*/";
	$ilc = "//";
	$lbrace = "{";
	$rbrace = "}";
	if ($lang eq "C" || $lang eq "java") {
		$enumname = "enum";
	}
	if ($lang eq "C") {
		$unionname = "union";
		$structname = "struct";
	}
	$varname = "";
	$constname = "const";
	$structisbrace = 0;
	# DO NOT DO THIS, no matter how tempting it may seem.
	# sofunction and soprocedure are only for functions/procedures
	# that do not follow the form '<type information> <name> ( <args> );'.
	# MIG does, so don't do this.
	# if ($sublang eq "MIG") {
		# $sofunction = "routine";
		# $soprocedure = "simpleroutine";
	# };
	if ($sublang ne "php" && $sublang ne "IDL") {
		# @macronames = ( "#if", "#ifdef", "#ifndef", "#endif", "#else", "#elif", "#error", "#warning", "#pragma", "#import", "#include", "#define" );
		%macronames = ( "#if" => 1, "#ifdef" => 1, "#ifndef" => 1, "#endif" => 1, "#else" => 1, "#undef" => 1, "#elif" =>1, "#error" => 1, "#warning" => 1, "#pragma" => 1, "#import" => 1, "#include" => 1, "#define"  => 1);
		$definename = "#define";
	} elsif ($sublang eq "IDL") {
		%macronames = ( "#if" => 1, "#ifdef" => 1, "#ifndef" => 1, "#endif" => 1, "#else" => 1, "#undef" => 1, "#elif" =>1, "#error" => 1, "#warning" => 1, "#pragma" => 1, "#import" => 1, "#include" => 1, "#define"  => 1, "import" => 1 );
		$definename = "#define";
	}
    }

    $HeaderDoc::soc = $soc;
    $HeaderDoc::ilc = $ilc;
    $HeaderDoc::eoc = $eoc;
    $HeaderDoc::socquot = $soc;
    $HeaderDoc::socquot =~ s/(\W)/\\$1/sg;
    $HeaderDoc::eocquot = $eoc;
    $HeaderDoc::eocquot =~ s/(\W)/\\$1/sg;
    $HeaderDoc::ilcquot = $ilc;
    $HeaderDoc::ilcquot =~ s/(\W)/\\$1/sg;
    $HeaderDoc::ilcbquot = $ilc_b;
    $HeaderDoc::ilcbquot =~ s/(\W)/\\$1/sg;

    return ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $ilc_b, $sofunction,
	$soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$enumname,
	$typedefname, $varname, $constname, $structisbrace, \%macronames,
	$classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp,
	$requiredregexp, $propname, $objcdynamicname, $objcsynthesizename, $moduleregexp, $definename);
}

sub isKeyword
{
    my $token = shift;
    my $keywordref = shift;
    my $case_sensitive = shift;
    my %keywords = %{$keywordref};
    my $localDebug = 0;

    # if ($token =~ /^\#/o) { $localDebug = 1; }

    print STDERR "isKeyWord: TOKEN: \"$token\"\n" if ($localDebug);
    print STDERR "#keywords: ".scalar(keys %keywords)."\n" if ($localDebug);
    if ($localDebug) {
	foreach my $keyword (keys %keywords) {
		print STDERR "isKeyWord: keyword_list: $keyword\n" if ($localDebug);
	}
    }

    if ($case_sensitive) {
	if ($keywords{$token}) {
	    print STDERR "MATCH\n" if ($localDebug);
	    return $keywords{$token};
	}
    } else {
      foreach my $keyword (keys %keywords) {
	print STDERR "isKeyWord: keyword: $keyword\n" if ($localDebug);
	my $quotkey = quote($keyword);
	if ($token =~ /^$quotkey$/i) {
		print STDERR "MATCH\n" if ($localDebug);
		return $keywords{$keyword};
	}
      }
    }
    return 0;
}


use FileHandle;
use IPC::Open2;
use Fcntl;


sub html2xhtml
{
    my $html = shift;
    my $debugname = shift;
    my $localDebug = 0;

    # print STDERR "FAST PATH: ".$HeaderDoc::ignore_apiuid_errors."\n";

    local $/;
    my $xmlout = "--xmlout";
    if ($xmllintversion eq "20607") {
	$xmlout = "";
    }

# print STDERR "xmllint version is $xmllintversion\n";
# print STDERR "xmllint is $xmllint\n";

    warn "PREOPEN\n" if ($localDebug);
    my $pid = open2( \*fromLint, \*toLint, "$xmllint --html $xmlout --recover --nowarning - 2> /dev/null");
    warn "ONE\n" if ($localDebug);

    toLint->autoflush();
    print toLint "<html><body>$html</body></html>\n";
    toLint->flush();

    warn "TWO\n" if ($localDebug);

    close toLint;

    my $xhtml = <fromLint>;
    warn "TWO-A\n" if ($localDebug);

    close fromLint;
    warn "THREE\n" if ($localDebug);

    my $old_xhtml = $xhtml;

    warn "FOUR\n" if ($localDebug);
    $xhtml =~ s/^<!DOCTYPE .*?>//so;
    $xhtml =~ s/^<\?xml.*?\?>\n<!.*>\n<html>//so;
    $xhtml =~ s/<\/html>$//so;
    if ($xhtml =~ /^\s*<body\/>\s*/o) {
	$xhtml = "";
    } else {
	$xhtml =~ s/^<body>//so;
	$xhtml =~ s/<\/body>$//so;
    }

    # Why, oh why does xmllint refuse to turn off translation for this
    # particular entity?  According to the man page, I should have to
    # specify --noent to get the behavior I'm getting....

    my $nbsprep = chr(0xc2).chr(0xa0);
    $xhtml =~ s/$nbsprep/&nbsp;/sg;

    # Do we want to translate &quot; back to a double-quote mark?  I don't
    # know why xmllint wants to turn this into an entity....
    # $xhtml =~ s/&quot;/"/sgo;

    # Attempt to get the length of the text itself (approximately)
    my $htmllengthcheck = $html;
    my $xhtmllengthcheck = $xhtml;
    $htmllengthcheck =~ s/\s//sgo;
    $xhtmllengthcheck =~ s/\s//sgo;
    $htmllengthcheck =~ s/<.*?>//sgo;
    $xhtmllengthcheck =~ s/<.*?>//sgo;

    if (length($xhtmllengthcheck) < length($htmllengthcheck)) {
	warn "DEBUGNAME: $debugname\n" if ($localDebug);
	warn "$debugname: XML to HTML translation failed.\n";
	warn "XHTML was truncated (".length($xhtmllengthcheck)." < ".length($htmllengthcheck).").\n";
	warn "BEGIN HTML:\n$html\nEND HTML\nBEGIN XHTML:\n$xhtml\nEND XHTML\n";
	# warn "BEGIN OLD XHTML:\n$old_xhtml\nEND OLD XHTML\n";
	# print STDERR "A:\n$htmllengthcheck\nB:\n$xhtmllengthcheck\n";
    }

    print STDERR "GOT XHTML (oldlen = ".length($html)."): $xhtml\n" if ($localDebug);

    my $retval = waitpid($pid, 0);
    my $exitstatus = $?;

    if ($exitstatus) {
	warn "DEBUGNAME: $debugname\n" if ($localDebug);
	warn "$debugname:XML to HTML translation failed.\n";
	warn "Error was $exitstatus\n";
    }


    return $xhtml;
}


sub resolveLinks($$$)
{
    my $path = shift;

    if (@_) {
        my $externalXRefFiles = shift;
	if (length($externalXRefFiles)) {
		my @files = split(/\s/s, $externalXRefFiles);
		foreach my $file (@files) {
			$path .= " -s \"$file\"";
		}
	}
    }
    if (@_) {
        my $externalAPIRefs = shift;
	if (length($externalAPIRefs)) {
		my @refs = split(/\s/s, $externalAPIRefs);
		foreach my $ref (@refs) {
			$path .= " -r \"$ref\"";
		}
	}
    }
    
    my $resolverpath = $HeaderDoc::modulesPath."bin/resolveLinks";

    $path =~ s/"/\\"/sg;
    print STDERR "EXECUTING $resolverpath \"$path\"\n";
    my $retval = system($resolverpath." \"$path\"");

    if ($retval == -1) {
	warn "error: resolveLinks not installed.  Please check your installation.\n";
    } elsif ($retval) {
	warn "error: resolveLinks failed ($retval).  Please check your installation.\n";
    }
}


# /*! validTag returns 1 if a tag is valid, -1 if a tag should be
#     replaced with another string, or 0 if a tag is not valid.
#  */
sub validTag
{
    my $field = shift;
    my $origfield = $field;
    my $include_first_tier = 1;
    my $include_second_tier = 1;
    if (@_) {
	my $level = shift;
	if ($level == 0) {
		$include_first_tier = 1;
		$include_second_tier = 1;
	} elsif ($level == 1) {
		$include_first_tier = 1;
		$include_second_tier = 0;
	} elsif ($level == 2) {
		$include_first_tier = 0;
		$include_second_tier = 1;
	}
	# print STDERR "DEBUG: field $field level: $level first: $include_first_tier second: $include_second_tier\n";
    # } else {
	# print STDERR "NO SECOND ARG\n";
    }


    SWITCH: {
            ($field =~ s/^\/\*\!//so) && do { return ($include_first_tier || $include_second_tier); };
            ($field =~ s/^\/\/\!//so) && do { return ($include_first_tier || $include_second_tier); };
            ($field =~ s/^abstract(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^alsoinclude(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^attribute(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^attributeblock(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^attributelist(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^author(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^availability(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^availabilitymacro(\s+|$)//sio) && do { return $include_first_tier; };
            ($field =~ s/^brief(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^callback(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^category(\s+|$)//sio) && do { return $include_first_tier; };
            ($field =~ s/^CFBundleIdentifier(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^charset(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^class(\s+|$)//sio) && do { return $include_first_tier; };
            ($field =~ s/^compilerflag(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^const(ant)?(\s+|$)//sio) && do { return ($include_first_tier || $include_second_tier); };
            ($field =~ s/^copyright(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^define(d)?(\s+|$)//sio) && do { return $include_first_tier; };
            ($field =~ s/^define(d)?block(\s+|$)//sio) && do { return $include_first_tier; };
            ($field =~ s/^\/define(d)?block(\s+|$)//sio) && do { return $include_first_tier; };
            ($field =~ s/^deprecated(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^description(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^details(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^discussion(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^encoding(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^enum(\s+|$)//sio) && do { return $include_first_tier; };
            ($field =~ s/^exception(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^field(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^file(\s+|$)//sio) && do { return $include_first_tier; };
            ($field =~ s/^flag(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^framework(\s+|$)//sio) && do { return $include_first_tier; };
            ($field =~ s/^frameworkuid(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^frameworkpath(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^frameworkcopyright(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^function(\s+|$)//sio) && do { return $include_first_tier; };
            ($field =~ s/^functiongroup(\s+|$)//sio) && do { return $include_first_tier; };
            ($field =~ s/^group(\s+|$)//sio) && do { return $include_first_tier; };
            ($field =~ s/^<\/hd_link>//sio) && do { return $include_second_tier; };	# note: opening tag not needed.
									# This is not a real tag.  It
									# is automatically inserted to
									# replace @/link, however,
									# and thus may appear at the
									# start of a parsed field in
									# some parts of the code.
            ($field =~ s/^header(\s+|$)//sio) && do { return $include_first_tier; }; 
            ($field =~ s/^hidesingletons(\s+|$)//sio) && do { return $include_second_tier; }; 
            ($field =~ s/^hidecontents(\s+|$)//sio) && do { return $include_second_tier; }; 
            ($field =~ s/^ignore(\s+|$)//sio) && do { return $include_second_tier; }; 
            ($field =~ s/^ignorefuncmacro(\s+|$)//sio) && do { return $include_second_tier; }; 
            ($field =~ s/^important(\s+|$)//sio) && do { return -$include_second_tier; }; 
            ($field =~ s/^indexgroup(\s+|$)//sio) && do { return $include_second_tier; }; 
            ($field =~ s/^interface(\s+|$)//sio) && do { return $include_first_tier; };
            ($field =~ s/^internal(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^language(\s+|$)//sio) && do { return $include_first_tier; };
            ($field =~ s/^link(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^\/link//sio) && do { return $include_second_tier; };
            ($field =~ s/^meta(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^method(\s+|$)//sio) && do { return $include_first_tier; };
            ($field =~ s/^methodgroup(\s+|$)//sio) && do { return $include_first_tier; };
            ($field =~ s/^name(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^namespace(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^noParse(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^param(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^parseOnly(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^preprocinfo(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^performance(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^property(\s+|$)//sio) && do { return $include_first_tier; };
            ($field =~ s/^protocol(\s+|$)//sio) && do { return $include_first_tier; };
            ($field =~ s/^related(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^result(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^return(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^see(also|)(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^serial(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^serialData(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^serialfield(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^since(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^struct(\s+|$)//sio) && do { return $include_first_tier; };
            ($field =~ s/^super(class|)(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^template(\s+|$)//sio) && do { return $include_first_tier; };
            ($field =~ s/^templatefield(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^throws(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^typedef(\s+|$)//sio) && do { return $include_first_tier; };
	    ($field =~ s/^union(\s+|$)//sio) && do { return $include_first_tier; };
            ($field =~ s/^unformatted(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^unsorted(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^updated(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^var(\s+|$)//sio) && do { return $include_first_tier; }; 
            ($field =~ s/^version(\s+|$)//sio) && do { return $include_second_tier; };
            ($field =~ s/^warning(\s+|$)//sio) && do { return -$include_second_tier; }; 
            ($field =~ s/^whyinclude(\s+|$)//sio) && do { return $include_second_tier; };
                {                       # print STDERR "NOTFOUND: \"$field\"\n";
					if (length($field)) {
						return 0;
					}
					return 1;
                };
         
        }
}


sub replaceTag($)
{
	my $tag = shift;

	if ($tag =~ s/^warning(\s|$)//si) {
		return "<p><b>WARNING:</b><br /></p><div class='warning_indent'>".$tag."</div>";
	}
	if ($tag =~ s/^important(\s|$)//si) {
		return "<p><b>Important:</b><br /></p><div class='important_indent'>".$tag."</div>";
	}
	warn "Could not replace unknown tag \"$tag\"\n";
}


sub stringToFields($$$)
{
	my $line = shift;
	my $fullpath = shift;
	my $linenum = shift;

	my $localDebug = 0;

	print STDERR "LINE WAS: \"$line\"\n" if ($localDebug);

	my @fields = split(/\@/s, $line);
	my @newfields = ();
	my $lastappend = "";
	my $in_textblock = 0;
	my $in_link = 0;
	my $lastlinkfield = "";

	my $keepfield = "";
	foreach my $field (@fields) {
		if ($field =~ /\\$/s) {
			$field =~ s/\\$//s;
			if ($keepfield ne "") {
				$keepfield .= "@".$field;
			} else {
				$keepfield = $field;
			}
		} elsif ($keepfield ne "") {
			$field = $keepfield."@".$field;
			$keepfield = "";
			push(@newfields, $field);
		} else {
			push(@newfields, $field);
		}
	}
	@fields = @newfields;
	@newfields = ();

	foreach my $field (@fields) {
	  my $dropfield = 0;
	  print STDERR "processing $field\n" if ($localDebug);
	  if ($in_textblock) {
	    if ($field =~ /^\/textblock/so) {
		print STDERR "out of textblock\n" if ($localDebug);
		if ($in_textblock == 1) {
		    my $cleanfield = $field;
		    $cleanfield =~ s/^\/textblock//sio;
		    $lastappend .= $cleanfield;
		    push(@newfields, $lastappend);
		    print STDERR "pushed \"$lastappend\"\n" if ($localDebug);
		    $lastappend = "";
		}
		$in_textblock = 0;
	    } else {
		# clean up text block
		$field =~ s/\</\&lt\;/sgo;
		$field =~ s/\>/\&gt\;/sgo;
		$lastappend .= "\@$field";
		print STDERR "new field is \"$lastappend\"\n" if ($localDebug);
	    }
	  } else {
	    # if ($field =~ /value/so) { warn "field was $field\n"; }
	    if ($field =~ s/^value/<hd_value\/>/sio) {
		$lastappend = pop(@newfields);
	    }
	    if ($field =~ s/^inheritDoc/<hd_ihd\/>/sio) {
		$lastappend = pop(@newfields);
	    }
	    # if ($field =~ /value/so) { warn "field now $field\n"; }
	    if ($field =~ s/^\/link/<\/hd_link>/sio) {
		# warn "FIELD WAS \"$field\"\n";
		if ($field =~ /^<\/hd_link>\s+[,.!?]/s) { $field =~ s/^<\/hd_link>\s+/<\/hd_link>/s; }
		# warn "FIELD NOW \"$field\"\n";
		if ($in_link) {
			$in_link = 0;
		} else {
			# drop this field on the floor.
			my $lastfield = pop(@newfields);
			$field =~ s/^<\/hd_link>//s;
			push(@newfields, $lastfield.$field);
			$field = "";
			$dropfield = 1;
		}
	    }
	    my $valid = validTag($field);
	    # Do field substitutions up front.
	    if ($valid == -1) {
		$field = replaceTag($field);
		# print STDERR "REPLACEMENT IS $field\n";
		if ($field !~ /^\@/) {
		    my $prev = pop(@newfields);
		    if (!$prev) { $prev = ""; }
		    push(@newfields, $prev.$field);
		    $dropfield = 2;
		}
	    }
	    if ($field =~ s/^link\s+//sio) {
		$lastlinkfield = $field;
		$in_link = 1;
		my $target = "";
		my $lastfield;

		if ($lastappend eq "") {
		    $lastfield = pop(@newfields);
		} else {
		    $lastfield = "";
		}
		# print STDERR "lastfield is $lastfield";
		$lastappend .= $lastfield; 
		if ($field =~ /^(\S*?)\s/so) {
		    $target = $1;
		} else {
		    # print STDERR "$fullpath:$linenum:MISSING TARGET FOR LINK!\n";
		    $target = $field;
		}
		my $localDebug = 0;
		print STDERR "target $target\n" if ($localDebug);
		my $qtarget = quote($target);
		$field =~ s/^$qtarget//sg;
		$field =~ s/\\$/\@/so;
		print STDERR "name $field\n" if ($localDebug);

		# Work around the infamous star-slash (eoc) problem.
		$target =~ s/\\\//\//g;

		if ($field !~ /\S/) { $field = $target; }

		$lastappend .= "<hd_link posstarget=\"$target\">";
		$lastappend .= "$field";
	    } elsif ($field =~ /^textblock\s/sio) {
		if ($lastappend eq "") {
		    $in_textblock = 1;
		    print STDERR "in textblock\n" if ($localDebug);
		    $lastappend = pop(@newfields);
		} else {
		    $in_textblock = 2;
		    print STDERR "in textblock (continuation)\n" if ($localDebug);
		}
		$field =~ s/^textblock(?:[ \t]+|([\n\r]))/$1/sio;
		# clean up text block
		$field =~ s/\</\&lt\;/sgo;
		$field =~ s/\>/\&gt\;/sgo;
		$lastappend .= "$field";
		print STDERR "in textblock.\nLASTAPPEND:\n$lastappend\nENDLASTAPPEND\n" if ($localDebug);
	    } elsif ($dropfield) {
		if ($dropfield == 1) {
			warn "$fullpath:$linenum:Unexpected \@/link tag found in HeaderDoc comment.\n";
		}
	    } elsif (!$valid) {
		my $fieldword = $field;
		my $lastfield = "";

		if ($lastappend == "") {
			$lastfield = pop(@newfields);
		} else {
			$lastfield = "";
		}
		$lastappend .= $lastfield; 

		# $fieldword =~ s/^\s*//sg; # Don't do this.  @ followed by space is an error.
		$fieldword =~ s/\s.*$//sg;
		warn "$fullpath:$linenum:Unknown field type \@".$fieldword." in HeaderDoc comment.\n";
		if ($localDebug) {
			cluck("Backtrace follows.\n");
		}
		$lastappend .= "\@".$field;

		if ($field !~ s/\\$/\@/so) {
			push(@newfields, $lastappend);
			$lastappend = "";
		}
	    } elsif ($field =~ s/\\$/\@/so) {
		$lastappend .= $field;
	    } elsif ($lastappend eq "") {
		push(@newfields, $field);
	    } else {
		$lastappend .= $field;
		push(@newfields, $lastappend);	
		$lastappend = "";
	    }
	  }
	}
	if (!($lastappend eq "")) {
	    push(@newfields, $lastappend);
	}
	if ($in_link) {
		warn "$fullpath:$linenum: warning: Unterminated \@link tag (starting field was: $lastlinkfield)\n";
	}
	if ($in_textblock) {
		warn "$fullpath:$linenum: warning: Unterminated \@textblock tag\n";
	}
	@fields = @newfields;

	if ($localDebug) {
		print STDERR "FIELDS:\n";
		for my $field (@fields) {
			print STDERR "FIELD:\n$field\n";
		}
	}

	return \@fields;
}


# /*! Sanitize a string for use in a URL */
sub sanitize
{
    my $string = shift;
    my $isname = 0;
    if (@_) {
	$isname = shift;
    }
    my $isoperator = 0;
    if ($isname) {
	if ($string =~ /operator/) { $isoperator = 1; }
    }

    my $newstring = "";
    my $prepart = "";
    my $postpart = "";

if ($string =~ /^\w*$/o) { return $string; }

    if ($string =~ s/^\///so) {
	$prepart = "/";
    }
    if ($string =~ s/\/$//so) {
	$postpart = "/";
    }

    my @parts = split(/(\W|\s)/, $string);

    foreach my $part (@parts) {
	if (!length($part)) {
		next;
	} elsif ($part =~ /\w/o) {
		$newstring .= $part;
	} elsif ($part =~ /\s/o) {
		# drop spaces.
		# $newstring .= $part;
	} elsif ($part =~ /[\~\:\,\.\-\_\+\!\*\(\)\/]/o) {
		# We used to exclude '$' as well, but this
		# confused libxml2's HTML parser.
		$newstring .= $part;
	} else {
		if (!$isname || ($isoperator && $part =~ /[\=\|\/\&\%\^\!\<\>]/)) {
			# $newstring .= "%".ord($part);
			my $val = ord($part);
			my $valstring = uc(sprintf("%02x", $val));
			$newstring .= "\%$valstring";
		}
	}
    }

    return $prepart.$newstring.$postpart;
}

# /*! @function nestignore
#     This function includes a list of headerdoc tags that are legal
#     within a headerdoc documentation block (e.g. a C struct)
#     such as parameters, etc.
#
#     The block parser support aspects of this function are
#     deprecated, as the calls to warnHDComment within the block
#     parser no longer exists.  Most calls to warnHDComment from
#     headerDoc2HTML.pl should always result in an error (since
#     they only occur outside the context of a declaration.
#
#     The exception is test point 12, which can cause false
#     positives for \@defineblock blocks.
#  */
sub nestignore
{
    my $tag = shift;
    my $dectype = shift;

# print STDERR "DT: $dectype TG: $tag\n";

    # defineblock can only be passed in for debug point 12, so
    # this can't break anything.

    if ($dectype =~ /defineblock/o && ($tag =~ /^\@define/o || $tag =~ /^\s*[^\s\@]/)) {
	# print STDERR "SETTING NODEC TO 1 (DECTYPE IS $dectype)\n";
	$HeaderDoc::nodec = 1;
	return 1;
    }


    return 0;

    # Old blockparser support logic.  Removed, since it broke other things.
    # if ($dectype =~ /(function|method|typedef)/o && $tag =~ /^\@param/o) {
	# return 1;
    # } elsif ($dectype =~ /\#define/o && $tag =~ /^\@define/o) {
	# return 1;
    # } elsif ($dectype !~ /(typedef|struct)/o && $tag =~ /^\@callback/o) {
	# return 1;
    # } elsif ($dectype !~ /(class|function|method|define)/o && $tag =~ /^\@field/o) {
	# return 1;
    # } elsif ($dectype !~ /(class|function|method|define)/o && $tag =~ /^\@constant/o) {
	# return 1;
    # }

    # return 0;
}

# /*! @function warnHDComment
#     @param teststring string to be checked for headerdoc markup
#     @param linenum line number
#     @param dectype declaration type
#     @param dp debug point string
#  */
sub warnHDComment
{
    my $linearrayref = shift;
    my $blocklinenum = shift;
    my $blockoffset = shift;
    my $dectype = shift;
    my $dp = shift;
    my $optional_lastComment = shift;

    my $fullpath = $HeaderDoc::headerObject->fullpath();
    my $localDebug = 2; # Set to 2 so I wouldn't keep turning this off.
    my $rawLocalDebug = 0;
    my $maybeblock = 0;

print STDERR "DT: $dectype\n" if ($rawLocalDebug);

    if ($dectype =~ /blockMode:\#define/) {
	# print STDERR "DEFBLOCK?\n";
	$maybeblock = 1;
    }
    # if ($dectype =~ /blockMode:#define/ && ($tag =~ /^\@define/i || $tag !~ /^\@/)) {
	# return 2;
    # }

    my $line = ${$linearrayref}[$blocklinenum];
    my $linenum = $blocklinenum + $blockoffset;

	print STDERR "LINE WAS $line\n" if ($rawLocalDebug);

    my $isshell = 0;

    my $soc = $HeaderDoc::soc;
    my $ilc = $HeaderDoc::ilc;
    # my $socquot = $HeaderDoc::socquot;
    # my $ilcquot = $HeaderDoc::ilcquot;
    my $indefineblock = 0;

    if ($optional_lastComment =~ /\s*\/\*\!\s*\@define(d)?block\s+/s) {
	print STDERR "INBLOCK\n" if ($rawLocalDebug);
	$indefineblock = 1;
	$dectype = "defineblock";
    } else {
	print STDERR "optional_lastComment: $optional_lastComment\n" if ($rawLocalDebug);
    }

    if (($HeaderDoc::lang eq "shell") || ($HeaderDoc::lang eq "perl")) {
	$isshell = 1;
    }

    my $debugString = "";
    if ($localDebug) { $debugString = " [debug point $dp]"; }

    if ((!$isshell && $line =~ /\Q$soc\E\!(.*)$/s) || ($isshell && $line =~ /\Q$ilc\E\s*\/\*\!(.*)$/s)) {
	my $rest = $1;

	$rest =~ s/^\s*//so;
	$rest =~ s/\s*$//so;

	while (!length($rest) && ($blocklinenum < scalar(@{$linearrayref}))) {
		$blocklinenum++;
		$rest = ${$linearrayref}[$blocklinenum];
		$rest =~ s/^\s*//so;
		$rest =~ s/\s*$//so;
	}

	print STDERR "REST: $rest\nDECTYPE: $dectype\n" if ($rawLocalDebug);

	if ($rest =~ /^\@/o) {
		 if (nestignore($rest, $dectype)) {
			print STDERR "NEST IGNORE[1]\n" if ($rawLocalDebug);
			return 0;
		}
	} else {
		print STDERR "Nested headerdoc markup with no tag.\n" if ($rawLocalDebug);
		 if (nestignore($rest, $dectype)) {
			print STDERR "NEST IGNORE[2]\n" if ($rawLocalDebug);
			return 0;
		}
	}

	if ($maybeblock) {
		print STDERR "CHECKING FOR END OF DEFINE BLOCK.  REST IS \"$rest\"\n" if ($rawLocalDebug);
		if ($rest =~ /^\s*\@define(d?)\s+/) {
			print STDERR "DEFINE\n" if ($rawLocalDebug);
			return 2;
		}
		if ($rest =~ /^\s*[^\@\s]/) {
			print STDERR "OTHER\n" if ($rawLocalDebug);
			return 2;
		}
	}
	if (!$HeaderDoc::ignore_apiuid_errors) {
		warn("$fullpath:$linenum: warning: Unexpected headerdoc markup found in $dectype declaration$debugString.  Output may be broken.\n");
	}
	print STDERR "RETURNING 1\n" if ($rawLocalDebug);
	return 1;
    }
#print STDERR "OK\n";
    print STDERR "RETURNING 0\n" if ($rawLocalDebug);
    return 0;
}

sub get_super {
    my $classType = shift;
    my $dec = shift;
    my $super = "";
    my $localDebug = 0;

    print STDERR "GS: $dec EGS\n" if ($localDebug);

    $dec =~ s/\n/ /smgo;

    if ($classType =~ /^occ/o) {
	if ($dec !~ s/^\s*\@interface\s*//so) {
	    if ($dec !~ s/^\s*\@protocol\s*//so) {
	    	$dec =~ s/^\s*\@class\s*//so;
	    }
	}
	if ($dec =~ /(\w+)\s*\(\s*(\w+)\s*\)/o) {
	    $super = $1; # delegate is $2
        } elsif (!($dec =~ s/.*?://so)) {
	    $super = "";
	} else {
	    $dec =~ s/\(.*//sgo;
	    $dec =~ s/\{.*//sgo;
	    $super = $dec;
	}
    } elsif ($classType =~ /^cpp$/o) {
	$dec =~ s/^\s*\class\s*//so;
        if (!($dec =~ s/.*?://so)) {
	    $super = "";
	} else {
	    $dec =~ s/\(.*//sgo;
	    $dec =~ s/\{.*//sgo;
	    $dec =~ s/^\s*//sgo;
	    $dec =~ s/^public//go;
	    $dec =~ s/^private//go;
	    $dec =~ s/^protected//go;
	    $dec =~ s/^virtual//go;
	    $super = $dec;
	}
    }

    $super =~ s/^\s*//o;
    $super =~ s/\s.*//o;

    print STDERR "$super is super\n" if ($localDebug);
    return $super;
}

# /*!
#      <code>classTypeFromFieldAndBPinfo</code> takes the type requested
#      in the headerdoc comment (or "auto" if none requested), the
#      type returned by the block parser, and the declaration (or the
#      first few bytes thereof) and determines what HeaderDoc object
#      should be created.
# 
# <pre>
#      Matching list:
#        HD                    CODE                    Use
#        @interface            ----                    same as @class (usually C COM Interface)
#        @class                @class                  ObjCCategory (gross)
#                                                      /|\ should be ObjCClass?
#        @class                class                   CPPClass
#        @class                typedef struct          CPPClass
#        @class                @interface              ObjCClass
#        @category             @interface              ObjCCategory
#        @protocol             @protocol               ObjCProtocol
#        auto                  @interface name : ...   ObjCClass
#        auto                  @interface name(...)    ObjCCategory
#        auto                  @protocol               ObjCProtocol
#        auto                  class                   CPPClass
# </pre>
#  */
sub classTypeFromFieldAndBPinfo
{
	my $classKeyword = shift;
	my $classBPtype = shift;
	my $classBPdeclaration = shift;
	my $fullpath = shift;
	my $linenum = shift;
	my $sublang = shift;

	my $deccopy = $classBPdeclaration;
	$deccopy =~ s/[\n\r]/ /s;
	$deccopy =~ s/\{.*$//sg;
	$deccopy =~ s/\).*$//sg;
	$deccopy =~ s/;.*$//sg;

	# print STDERR "DC: $deccopy\n";
	# print STDERR "CBPT: $classBPtype\n";

	SWITCH: {
		($classBPtype =~ /^\@protocol/) && do { return "intf"; };
		($classKeyword =~ /category/) && do { return "occCat"; };
		# ($classBPtype =~ /^\@class/) && do { return "occCat"; };
		($classBPtype =~ /^\@class/) && do { return "occ"; };
		($classBPtype =~ /^\@interface/) && do {
				if ($classKeyword =~ /class/) {
					return "occ";
				} elsif ($deccopy =~ /\:/s) {
					# print STDERR "CLASS: $deccopy\n";
					return "occ";
				} elsif ($deccopy =~ /\(/s) {
					# print STDERR "CATEGORY: $deccopy\n";
					return "occCat";
				} else {
					last SWITCH;
				}
			};
		($classKeyword =~ /class/) && do { return $sublang; };
		($classBPtype =~ /typedef/) && do { return "C"; };
		($classBPtype =~ /struct/) && do { return "C"; };
		($classBPtype =~ /class/) && do { return $sublang; };
		($classBPtype =~ /interface/) && do { return $sublang; };
		($classBPtype =~ /implementation/) && do { return $sublang; };
		($classBPtype =~ /module/) && do { return $sublang; };
		($classBPtype =~ /namespace/) && do { return $sublang; };
	}
	warn "$fullpath:$linenum: warning: Unable to determine class type.\n";
	warn "KW: $classKeyword\n";
	warn "BPT: $classBPtype\n";
	warn "DEC: $deccopy\n";
	return "cpp";
}

sub casecmp
{
    my $a = shift;
    my $b = shift;
    my $case = shift;

    if ($case) {
	if (($a eq $b) && ($a ne "") && ($b ne "")) { return 1; }
    } else {
	my $bquot = quote($b);
	if (($a =~ /^$bquot$/) && ($a ne "") && ($b ne "")) { return 1; }
    }

    return 0;
}

sub emptyHDok
{
    my $line = shift;
    my $okay = 0;

    SWITCH: {
	($line =~ /\@param(\s|$)/o) && do { $okay = 1; };
	($line =~ /\@name(\s|$)/o) && do { $okay = 1; };
	($line =~ /\@(function|method|)group(\s|$)/o) && do { $okay = 1; };
	($line =~ /\@language(\s|$)/o) && do { $okay = 1; };
	($line =~ /\@file(\s|$)/o) && do { $okay = 1; };
	($line =~ /\@header(\s|$)/o) && do { $okay = 1; };
	($line =~ /\@framework(\s|$)/o) && do { $okay = 1; };
	($line =~ /\@\/define(d)?block(\s|$)/o) && do { $okay = 1; };
	($line =~ /\@lineref(\s|$)/o) && do { $okay = 1; };
    }
    return $okay;
}

sub addAvailabilityMacro($$;$)
{
    my $token = shift;
    my $description = shift;
    my $has_args = 0;
    if (@_) {
	$has_args = shift || 0;
    }

    my $localDebug = 0;

    if (length($token) && length($description)) {
	print STDERR "AVTOKEN: \"$token\"\nDESC: $description\nHAS ARGS: $has_args\n" if ($localDebug);
	# push(@HeaderDoc::ignorePrefixes, $token);
	$HeaderDoc::availability_defs{$token} = $description;
	$HeaderDoc::availability_has_args{$token} = $has_args;
	HeaderDoc::BlockParse::cpp_remove($token);
    }
}

sub complexAvailabilityTokenToOSAndVersion($)
{
    my $token = shift;

    my $os = "";
    if ($token =~ s/^__IPHONE_//) {
	$os = "iPhone OS";
    } elsif ($token =~ s/^__MAC_//) {
	$os = "Mac OS X";
    } else {
	warn "Unknown OS in availability token \"$token\".  Giving up.\n";
	return "";
    }

    my $version = $token;
    $version =~ s/_/\./g;
    return ($os, $version);
}

sub complexAvailabilityToArray($$)
{
    my $token = shift;
    my $availstring = shift;
    my @returnarray = ();

    $availstring =~ s/\s*//sg;
    my @availparts = split(/,/, $availstring);

    if ($token eq "__OSX_AVAILABLE_STARTING") {
	my $macstarttoken = $availparts[0];
	my $iphonestarttoken = $availparts[1];
	my ($macstartos, $macstartversion) = complexAvailabilityTokenToOSAndVersion($macstarttoken);
	my ($iphonestartos, $iphonestartversion) = complexAvailabilityTokenToOSAndVersion($iphonestarttoken);

	if ($macstartversion eq "NA") {
		push(@returnarray, "Not available in $macstartos.");
	} else {
		push(@returnarray, "Available in $macstartos v$macstartversion.");
	}
	if ($iphonestartversion eq "NA") {
		push(@returnarray, "Not available in $iphonestartos.");
	} else {
		push(@returnarray, "Available in $iphonestartos v$iphonestartversion.");
	}
    } elsif ($token eq "__OSX_AVAILABLE_BUT_DEPRECATED") {
	my $macstarttoken = $availparts[0];
	my $macdeptoken = $availparts[1];
	my $iphonestarttoken = $availparts[2];
	my $iphonedeptoken = $availparts[3];

	my ($macstartos, $macstartversion) = complexAvailabilityTokenToOSAndVersion($macstarttoken);
	my ($iphonestartos, $iphonestartversion) = complexAvailabilityTokenToOSAndVersion($iphonestarttoken);
	my ($macdepos, $macdepversion) = complexAvailabilityTokenToOSAndVersion($macdeptoken);
	my ($iphonedepos, $iphonedepversion) = complexAvailabilityTokenToOSAndVersion($iphonedeptoken);

	if ($macstartversion eq "NA") {
		push(@returnarray, "Not available in $macstartos.");
	} elsif ($macdepversion eq "NA") {
		push(@returnarray, "Available in $macstartos v$macstartversion.");
	} else {
		if ($macstartversion eq $macdepversion) {
			push(@returnarray, "Introduced in $macstartos v$macstartversion, and deprecated in $macstartos v$macdepversion.");
		} else {
			push(@returnarray, "Introduced in $macstartos v$macstartversion, but later deprecated in $macstartos v$macdepversion.");
		}
	}
	if ($iphonestartversion eq "NA") {
		push(@returnarray, "Not available in $iphonestartos.");
	} elsif ($iphonedepversion eq "NA") {
		push(@returnarray, "Available in $iphonestartos v$iphonestartversion.");
	} else {
		if ($iphonestartversion eq $iphonedepversion) {
			push(@returnarray, "Introduced in $iphonestartos v$iphonestartversion, and deprecated in $iphonestartos v$iphonedepversion.");
		} else {
			push(@returnarray, "Introduced in $iphonestartos v$iphonestartversion, but later deprecated in $iphonestartos v$iphonedepversion.");
		}
	}
    } else {
	warn "Unknown complex availability token \"$token\".  Giving up.\n";
	return \@returnarray;
    }
    return \@returnarray;
}

# /*! Process the contents of a tag, e.g. @discussion.  The argument
#     should contain just the text to be processed, not the tag itself
#     or any end-of-comment marker. */
sub filterHeaderDocTagContents
{
    my $tagcontents = shift;

    my $opentags = "<p>|<h[1-6]>|<ul>|<ol>|<pre>|<dl>|<div>|<noscript>|<blockquote>|<form>|<hr>|<table>|<fieldset>|<address>";
    my $closetags = "<\/p>|<\/h[1-6]>|<\/ul>|<\/ol>|<\/pre>|<\/dl>|<\/div>|<\/noscript>|<\/blockquote>|<\/form>|<\/hr>|<\/table>|<\/fieldset>|<\/address>";

    my @parts = split(/($opentags|$closetags|\n)/o, $tagcontents);

    my $localDebug = 0;

    my $output = "";

    my $line_is_empty = 0;
    my $in_block_element = 0;
    foreach my $part (@parts) {
	my $lcpart = lc($part);
	if ($part ne "") {
		print STDERR "FHDTC PART: $part\n" if ($localDebug);
		if ($part =~ /\n/) {
			if ($line_is_empty) {
				print STDERR "NEWLINE: EMPTYLINE\n" if ($localDebug);
				# Emit paragraph break.  Two newlines in a row.
				if ($in_block_element == 2) {
					print STDERR "INSERT PARA\n" if ($localDebug);
					$output .= "</p>";
					$in_block_element = 0;
				}
				$line_is_empty = 0;
			} else {
				print STDERR "NEWLINE\n" if ($localDebug);
				$line_is_empty = 1;
			}
			$output .= $part;
		} elsif ($lcpart =~ /$opentags/) {
			print STDERR "OPENTAG\n" if ($localDebug);
			if ($in_block_element == 2) {
				print STDERR "CLOSING PARA\n" if ($localDebug);
				$output .= "</p>\n"; # close unclosed paragraphs.
			}
			$line_is_empty = 0;
			if ($lcpart eq "<p>") {
				print STDERR "BLOCK IS OPEN PARA\n" if ($localDebug);
				$in_block_element = 2;
			} else {
				print STDERR "BLOCK IS NOT OPEN PARA\n" if ($localDebug);
				$in_block_element = 1;
			}
			$output .= $part;
		} elsif ($part =~ /$closetags/) {
			print STDERR "CLOSETAG\n" if ($localDebug);
			$line_is_empty = 0;
			$in_block_element = 0;
			$output .= $part;
		} elsif (!$in_block_element && $part =~ /\S/) {
			print STDERR "OPENING IMPLICIT PARA\n" if ($localDebug);
			$output .= "<p>";
			$line_is_empty = 0;
			$in_block_element = 2;
			$output .= $part;
		} else {
			print STDERR "NORMAL TEXT\n" if ($localDebug);
			$line_is_empty = 0;
			$output .= $part;
		}
	}
    }

    return $output;
}

# /*! Process a comment, stripping off leading '*' and whitespace.
#  */
sub filterHeaderDocComment
{
    my $headerDocCommentLinesArrayRef = shift;
    my $lang = shift;
    my $sublang = shift;
    my $inputCounter = shift;

    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $ilc_b, $sofunction,
        $soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
        $enumname,
        $typedefname, $varname, $constname, $structisbrace, $macronamesref,
        $classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp,
        $requiredregexp, $propname, $objcdynamicname, $objcsynthesizename) = parseTokens($lang, $sublang);

    my $fullpath = $HeaderDoc::headerObject->fullpath();

    my @headerDocCommentLinesArray = @{$headerDocCommentLinesArrayRef};
    my $returnComment = "";

    my $localDebug = 0;
    my $liteDebug = 0;
    my $commentDumpDebug = 0;

    my $linenum = 0;
    my $curtextblockstarred = 1;
    my @textblock_starred_array = ();
    my $outerstarred = 1;
    my $textblock_number = 0;

    # Perl and shell HeaderDoc comments star with # /*! and end with # ... */
    # This is mainly to avoid conflicting with the shell magic #!.
    if ($lang eq "perl" || $lang eq "shell") {
	$soc = "/*";
	$eoc = "*/";
	$ilc = "";
    }

    my $paranoidstate = 0;
    foreach my $lineref (@headerDocCommentLinesArray) {
	my %lineentry = %{$lineref};
	my $in_textblock = $lineentry{INTEXTBLOCK};
	my $in_pre = $lineentry{INPRE};
	my $leaving_textblock = $lineentry{LEAVINGTEXTBLOCK};
	my $leaving_pre = $lineentry{LEAVINGPRE};
	my $line = $lineentry{LINE};

	if ($lang eq "perl" || $lang eq "shell") {
		$line =~ s/^\s*\#//o;
	}

	print STDERR "PREPASS LINE: \"$line\"\n" if ($localDebug || $liteDebug);
	# print STDERR "CMP $linenum CMP ".$#headerDocCommentLinesArray."\n";

	if (!$linenum) {
		print STDERR "PREPASS SKIP\n" if ($localDebug);
		$linenum++;
	} else {
		if ($in_textblock) {
			print STDERR "PREPASS IN TEXTBLOCK\n" if ($localDebug);
			if ($line !~ /^\s*\*/) {
				print STDERR "CURRENT TEXT BLOCK NOT STARRED\n" if ($localDebug);
				$curtextblockstarred = 0;
			}
		} elsif ($leaving_textblock) {
			print STDERR "PREPASS LEAVING TEXTBLOCK #".$textblock_number." (STARRED = $curtextblockstarred)\n" if ($localDebug);
			$textblock_starred_array[$textblock_number++] = $curtextblockstarred;
			$curtextblockstarred = 1;
		} else {
			print STDERR "PREPASS NORMAL LINE\n" if ($localDebug);
			if ($line !~ /^\s*\*/) {
				print STDERR "PREPASS OUTER NOT STARRED\n" if ($localDebug);
				$outerstarred = 0;
				# last; # bad idea.  Need to handle textblocks still.
			} else {
				if ($line !~ /^\s*\Q$eoc\E/) {
					print STDERR "PARANOID STATE -> 1\n" if ($localDebug);
					$paranoidstate = 1;
				} else {
					print STDERR "EOC\n" if ($localDebug);
				}
			}
		}
		$linenum++;
	}
    }
    # $HeaderDoc::enableParanoidWarnings = 1;
    print STDERR "OUTERSTARRED: $outerstarred\n" if ($localDebug);
    if ($paranoidstate && !$outerstarred && !$HeaderDoc::test_mode) {
	warn("$fullpath:$inputCounter: Partially starred comment.\n");
	warn("$fullpath:$inputCounter: Comment follows:\n");
	foreach my $lineref (@headerDocCommentLinesArray) {
		my %lineentry = %{$lineref};
		my $line = $lineentry{LINE};
		warn($line);
	}
	warn("$fullpath:$inputCounter: End of comment.\n");
    }

    # print STDERR "ENDCOUNT: ".$#headerDocCommentLinesArray."\n";


    my $starslash = 0;
    my $lastlineref = pop(@headerDocCommentLinesArray);
    my %lastline = %{$lastlineref};
    my $lastlinetext = $lastline{LINE};

    if ($lang eq "perl" || $lang eq "shell") {
	$lastlinetext =~ s/^\s*\#//o;
    }
    print STDERR "LLT: $lastlinetext\n" if ($localDebug);
    if ($lastlinetext =~ s/\Q$eoc\E\s*$//s) {
	if ($lang eq "perl" || $lang eq "shell") {
		$lastline{LINE} = "#".$lastlinetext;
	} else {
		$lastline{LINE} = $lastlinetext;
	}
	print STDERR "FOUND */\n" if ($localDebug);

	# If we just have */ on a line by itself, don't push it.  Otherwise, we would
	# get a bogus <BR><BR> at the end of the comment.
	if ($lastlinetext !~ /\S/) {
		print STDERR "LL dropped because it is empty: $lastlinetext\n" if ($localDebug);
		$starslash = 1;
	} else {
		print STDERR "LL retained (nonempty): $lastlinetext\n" if ($localDebug);
		push(@headerDocCommentLinesArray, $lastlineref);
	}
    } else {
	print STDERR "NO EOC ($eoc)\n";
	print STDERR "LL: $lastlinetext\n" if ($localDebug);
	push(@headerDocCommentLinesArray, $lastlineref);
    }
    $lastlineref = \%lastline;

    $textblock_number = 0;
    my $old_in_textblock = 0;
    foreach my $lineref (@headerDocCommentLinesArray) {
	my %lineentry = %{$lineref};
	my $in_textblock = $lineentry{INTEXTBLOCK};
	my $in_pre = $lineentry{INPRE};
	my $line = $lineentry{LINE};
	my $leaving_textblock = $lineentry{LEAVINGTEXTBLOCK};
	my $leaving_pre = $lineentry{LEAVINGPRE};

	if ($lang eq "perl" || $lang eq "shell") {
		$line =~ s/^\s*\#//o;
	}

	print STDERR "FILTER LINE: $line\n" if ($localDebug);
	print STDERR "IT: $in_textblock IP: $in_pre LT: $leaving_textblock LP: $leaving_pre\n" if ($localDebug);

	if ($in_textblock && $old_in_textblock) {
		# In textblock (not first line)
		if ($outerstarred) {
			my $tbstarred = $textblock_starred_array[$textblock_number];
			if ($tbstarred) {
				$line =~ s/^\s*[*]//so;
			}
		}
	} else {
		# Either not in a textblock or in the first line of a textblock

		if ($outerstarred) {
			my $tbstarred = $textblock_starred_array[$textblock_number];
 			if (!$leaving_textblock || $tbstarred) {
				$line =~ s/^\s*[*]//so;
			}
		}
		if (!$in_pre && !$leaving_pre && !$leaving_textblock) {
			$line =~ s/^[ \t]*//o; # remove leading whitespace

			# The following modification is done in
			# filterHeaderDocTagContents now.
			# if ($line !~ /\S/) {
				# $line = "</p><p>\n";
			# } 
		}
		$old_in_textblock = $in_textblock;
	}
	if ($leaving_textblock) {
		$textblock_number++;
	}
	if ($lang eq "perl" || $lang eq "shell") {
		$line = "#".$line;
	}

	$returnComment .= $line;
    }

    if ($starslash) {
	if ($lang eq "perl" || $lang eq "shell") {
		$returnComment .= "#";
	}
	$returnComment .= $eoc;
    }

if (0) { # Previous code worked like this:
    foreach my $lineref (@headerDocCommentLinesArray) {
	my %lineentry = %{$lineref};
	my $in_textblock = $lineentry{INTEXTBLOCK};
	my $in_pre = $lineentry{INPRE};
	my $line = $lineentry{LINE};
	# print STDERR "LINE: $line\n";
	$line =~ s/^\s*[*]\s+(\S.*)/$1/o; # remove asterisks that precede text
	if (!$in_textblock && !$in_pre) {
		$line =~ s/^[ \t]*//o; # remove leading whitespace
		if ($line !~ /\S/) {
			$line = "<br><br>\n";
		} 
		$line =~ s/^\s*[*]\s*$/<br><br>\n/o; # replace sole asterisk with paragraph divider
	      } else {
		$line =~ s/^\s*[*]\s*$/\n/o; # replace sole asterisk with empty line
	}
	$returnComment .= $line;
    }
}

    print STDERR "RESULTING COMMENT:\n$returnComment\nEOCOMMENT\n" if ($localDebug || $commentDumpDebug || $liteDebug);

    return $returnComment;
}

sub processTopLevel
{
	my ($inHeader, $inClass, $inInterface, $inCPPHeader, $inOCCHeader, $inPerlScript, $inShellScript, $inPHPScript, $inJavaSource, $inFunctionGroup, $inGroup, $inFunction, $inPDefine, $inTypedef, $inUnion, $inStruct, $inConstant, $inVar, $inEnum, $inMethod, $inAvailabilityMacro, $inUnknown, $classType, $line, $inputCounter, $blockOffset, $fullpath, $linenumdebug, $localDebug) = @_;

	$localDebug = $localDebug || 0;

	if ($localDebug) {
		my $token = $line;
		$token =~ s/\s*(\/\*|\/\/)\!//s;
		$token =~ s/^\s*//s;
		$token =~ s/\s.*$//s;
		print STDERR "TOKEN: $token\n";
	}

	$line =~ s/^\s*//s;

				SWITCH: { # determine which type of comment we're in
					($line =~ /^\/\*!\s+\@file\s*/io) && do {$inHeader = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@header\s*/io) && do {$inHeader = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@framework\s*/io) && do {$inHeader = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@template\s*/io) && do {$inClass = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@interface\s*/io) && do {$inClass = 1; $inInterface = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@class\s*/io) && do {$inClass = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@protocol\s*/io) && do {$inClass = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@category\s*/io) && do {$inClass = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@language\s+.*c\+\+\s*/io) && do {$inCPPHeader = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@language\s+.*objc\s*/io) && do {$inOCCHeader = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@language\s+.*perl\s*/io) && do {$inPerlScript = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@language\s+.*shell\s*/io) && do {$inShellScript = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@language\s+.*php\s*/io) && do {$inPHPScript = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@language\s+.*java\s*/io) && do {$inJavaSource = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@language\s+.*javascript\s*/io) && do {$inJavaSource = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@functiongroup\s*/io) && do {$inFunctionGroup = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@group\s*/io) && do {$inGroup = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@name\s*/io) && do {$inGroup = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@function\s*/io) && do {$inFunction = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@availabilitymacro(\s+)/io) && do { $inAvailabilityMacro = 1; $inPDefine = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@methodgroup\s*/io) && do {$inFunctionGroup = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@method\s*/io) && do {
						    if ($classType eq "occ" ||
							$classType eq "intf" ||
							$classType eq "occCat") {
							    $inMethod = 1;last SWITCH;
						    } else {
							    $inFunction = 1;last SWITCH;
						    }
					};
					($line =~ /^\/\*!\s+\@typedef\s*/io) && do {$inTypedef = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@union\s*/io) && do {$inUnion = 1;$inStruct = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@struct\s*/io) && do {$inStruct = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@const(ant)?\s*/io) && do {$inConstant = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@var\s*/io) && do {$inVar = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@property\s*/io) && do {$inUnknown = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@internal\s*/io) && do {
						# silently drop declaration.
						last SWITCH;
					};
					($line =~ /^\/\*!\s+\@define(d)?block\s*/io) && do {
							print STDERR "IN DEFINE BLOCK\n" if ($localDebug);
							$inPDefine = 2;
							last SWITCH;
						};
					($line =~ /^\/\*!\s+\@\/define(d)?block\s*/io) && do {
							print STDERR "OUT OF DEFINE BLOCK\n" if ($localDebug);
							$inPDefine = 0;
							last SWITCH;
						};
					($line =~ /^\/\*!\s+\@define(d)?\s*/io) && do {$inPDefine = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@lineref\s+(\d+)/io) && do {
						$blockOffset = $1 - $inputCounter;
						$inputCounter--;
						print STDERR "DECREMENTED INPUTCOUNTER [M4]\n" if ($HeaderDoc::inputCounterDebug);
						print STDERR "BLOCKOFFSET SET TO $blockOffset\n" if ($linenumdebug);
						last SWITCH;
					};
					($line =~ /^\/\*!\s+\@enum\s*/io) && do {$inEnum = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@serial(Data|Field|)\s+/io) && do {$inUnknown = 2;last SWITCH;};
					($line =~ /^\/\*!\s*[^\@\s]/io) && do {$inUnknown = 1;last SWITCH;};
					my $linenum = $inputCounter - 1 + $blockOffset;
					print STDERR "CHECKING VALIDFIELD FOR \"$line\".\n" if ($localDebug);;
					if (!validTag($line)) {
						warn "$fullpath:$linenum: warning: HeaderDoc comment is not of known type. Comment text is:\n";
						print STDERR "    $line\n";
					}
					$inUnknown = 1;
				} # end of SWITCH block

	return ($inHeader, $inClass, $inInterface, $inCPPHeader, $inOCCHeader, $inPerlScript, $inShellScript, $inPHPScript, $inJavaSource, $inFunctionGroup, $inGroup, $inFunction, $inPDefine, $inTypedef, $inUnion, $inStruct, $inConstant, $inVar, $inEnum, $inMethod, $inAvailabilityMacro, $inUnknown, $classType, $line, $inputCounter, $blockOffset, $fullpath, $linenumdebug, $localDebug);
}

# /*! @function processHeaderComment
#   */ 
sub processHeaderComment {
    my $apiOwner = shift;
    my $rootOutputDir = shift;
    my $fieldArrayRef = shift;
    my $lang = shift;
    my $debugging = shift;
    my $reprocess_input = shift;
    my @fields = @$fieldArrayRef;
    my $linenum = $apiOwner->linenum();
    my $fullpath = $apiOwner->fullpath();
    my $localDebug = 0;

	foreach my $field (@fields) {
	    print STDERR "header field: |$field|\n" if ($localDebug);
		SWITCH: {
			($field =~ /^\/\*\!/o)&& do {last SWITCH;}; # ignore opening /*!
			(($lang eq "java") && ($field =~ /^\s*\/\*\*/o)) && do {last SWITCH;}; # ignore opening /**
			($field =~ /^see(also)\s+/o) &&
				do {
					$apiOwner->see($field);
					last SWITCH;
				};
			 ($field =~ /^frameworkcopyright\s+/io) && 
			    do {
				my $copy = $field;
				$copy =~ s/frameworkcopyright\s+//s;
				$copy =~ s/^\s+//sg;
				$copy =~ s/\s+$//sg;
				$apiOwner->attribute("Requested Copyright", $copy, 0, 1);
				# warn "FRAMEWORK COPYRIGHT: $copy\n";
				last SWITCH;
			    };
			 ($field =~ /^frameworkuid\s+/io) && 
			    do {
				my $uid = $field;
				$uid =~ s/frameworkuid\s+//s;
				$uid =~ s/\s+//sg;
				$uid =~ s/\/$//sg;
				$apiOwner->attribute("Requested UID", $uid, 0, 1);
				# warn "FRAMEWORK PATH: $uid\n";
				last SWITCH;
			    };
			 ($field =~ /^frameworkpath\s+/io) && 
			    do {
				my $path = $field;
				$path =~ s/frameworkpath\s+//s;
				$path =~ s/\s+//sg;
				$path =~ s/\/$//sg;
				$apiOwner->attribute("Framework Path", $path, 0);
				# warn "FRAMEWORK PATH: $path\n";
				last SWITCH;
			    };
			(($field =~ /^header\s+/io) ||
			 ($field =~ /^file\s+/io) ||
			 ($field =~ /^framework\s+/io)) && 
			    do {
			 	if ($field =~ s/^framework//io) {
					$apiOwner->isFramework(1);
				} else {
					$field =~ s/^(header|file)//o;
				}
				
				my ($name, $disc, $is_nameline_disc);
				($name, $disc, $is_nameline_disc) = &getAPINameAndDisc($field); 
				# my $longname = $name; #." (".$apiOwner->name().")";
				# print STDERR "NAME: $name\n";
				# print STDERR "API-IF: ".$apiOwner->isFramework()."\n";
				if (length($name) && ((!$HeaderDoc::ignore_apiowner_names) || $apiOwner->isFramework())) {
					print STDERR "Setting header name to $name\n" if ($debugging);
					$apiOwner->name($name);
				}
				print STDERR "Discussion is:\n" if ($debugging);
				print STDERR "$disc\n" if ($debugging);
				if (length($disc)) {
					if ($is_nameline_disc) {
						$apiOwner->nameline_discussion($disc);
					} else {
						$apiOwner->discussion($disc);
					}
				}
				last SWITCH;
			};
            ($field =~ s/^availability\s+//io) && do {$apiOwner->availability($field); last SWITCH;};
	    ($field =~ s/^since\s+//io) && do {$apiOwner->availability($field); last SWITCH;};
            ($field =~ s/^author\s+//io) && do {$apiOwner->attribute("Author", $field, 0); last SWITCH;};
	    ($field =~ s/^version\s+//io) && do {$apiOwner->attribute("Version", $field, 0); last SWITCH;};
            ($field =~ s/^deprecated\s+//io) && do {$apiOwner->attribute("Deprecated", $field, 0); last SWITCH;};
            ($field =~ s/^version\s+//io) && do {$apiOwner->attribute("Version", $field, 0); last SWITCH;};
	    ($field =~ s/^attribute\s+//io) && do {
		    my ($attname, $attdisc, $is_nameline_disc) = &getAPINameAndDisc($field);
		    if (length($attname) && length($attdisc)) {
			$apiOwner->attribute($attname, $attdisc, 0);
		    } else {
			warn "$fullpath:$linenum: warning: Missing name/discussion for attribute\n";
		    }
		    last SWITCH;
		};
	    ($field =~ s/^indexgroup(\s+)/$1/io) && do {$apiOwner->indexgroup($field); last SWITCH;};
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
			    $apiOwner->attributelist($name, $line);
			}
		    } else {
			warn "$fullpath:$linenum: warning: Missing name/discussion for attributelist\n";
		    }
		    last SWITCH;
		};
	    ($field =~ s/^attributeblock\s+//io) && do {
		    my ($attname, $attdisc, $is_nameline_disc) = &getAPINameAndDisc($field);
		    if (length($attname) && length($attdisc)) {
			$apiOwner->attribute($attname, $attdisc, 1);
		    } else {
			warn "$fullpath:$linenum: warning: Missing name/discussion for attributeblock\n";
		    }
		    last SWITCH;
		};
            ($field =~ s/^updated\s+//io) && do {$apiOwner->updated($field); last SWITCH;};
            ($field =~ s/^unsorted\s+//io) && do {$HeaderDoc::sort_entries = 0; last SWITCH;};
            ($field =~ s/^abstract\s+//io) && do {$apiOwner->abstract($field); last SWITCH;};
            ($field =~ s/^brief\s+//io) && do {$apiOwner->abstract($field, 1); last SWITCH;};
            ($field =~ s/^description(\s+|$)//io) && do {$apiOwner->discussion($field); last SWITCH;};
            ($field =~ s/^details(\s+|$)//io) && do {$apiOwner->discussion($field); last SWITCH;};
            ($field =~ s/^discussion(\s+|$)//io) && do {$apiOwner->discussion($field); last SWITCH;};
            ($field =~ s/^copyright\s+//io) && do { $apiOwner->headerCopyrightOwner($field); last SWITCH;};
            ($field =~ s/^meta\s+//io) && do {$apiOwner->HTMLmeta($field); last SWITCH;};
	    ($field =~ s/^language\s+//io) && do {
		SWITCH {
		    ($field =~ /^\s*c\+\+\s*$/io) && do { $HeaderDoc::sublang = "cpp"; last SWITCH; };
		    ($field =~ /^\s*objc\s*$/io) && do { $HeaderDoc::sublang = "occ"; last SWITCH; };
		    ($field =~ /^\s*pascal\s*$/io) && do { $HeaderDoc::sublang = "pascal"; last SWITCH; };
		    ($field =~ /^\s*perl\s*$/io) && do { $HeaderDoc::sublang = "perl"; last SWITCH; };
		    ($field =~ /^\s*shell\s*$/io) && do { $HeaderDoc::sublang = "shell"; last SWITCH; };
		    ($field =~ /^\s*php\s*$/io) && do { $HeaderDoc::sublang = "php"; last SWITCH; };
		    ($field =~ /^\s*javascript\s*$/io) && do { $HeaderDoc::sublang = "javascript"; last SWITCH; };
		    ($field =~ /^\s*java\s*$/io) && do { $HeaderDoc::sublang = "java"; last SWITCH; };
		    ($field =~ /^\s*c\s*$/io) && do { $HeaderDoc::sublang = "C"; last SWITCH; };
			{
				warn("$fullpath:$linenum: warning: Unknown language $field in header comment\n");
			};
		};
	    };
            ($field =~ s/^CFBundleIdentifier\s+//io) && do {$apiOwner->attribute("CFBundleIdentifier", $field, 0); last SWITCH;};
            ($field =~ s/^related\s+//io) && do {$apiOwner->attributelist("Related Headers", $field); last SWITCH;};
            ($field =~ s/^(compiler|)flag\s+//io) && do {$apiOwner->attributelist("Compiler Flags", $field); last SWITCH;};
            ($field =~ s/^preprocinfo\s+//io) && do {$apiOwner->attribute("Preprocessor Behavior", $field, 1); last SWITCH;};
	    ($field =~ s/^whyinclude\s+//io) && do {$apiOwner->attribute("Reason to Include", $field, 1); last SWITCH;};
            ($field =~ s/^ignorefuncmacro\s+//io) && do { $field =~ s/\n//smgo; $field =~ s/<br\s*\/?>//sgo; $field =~ s/^\s*//sgo; $field =~ s/\s*$//sgo;
		$HeaderDoc::perHeaderIgnoreFuncMacros{$field} = $field;
		if (!($reprocess_input)) {$reprocess_input = 1;} print STDERR "ignoring $field" if ($localDebug); last SWITCH;};
	    ($field =~ s/^namespace(\s+)/$1/io) && do {$apiOwner->namespace($field); last SWITCH;};
	    ($field =~ s/^charset(\s+)/$1/io) && do {$apiOwner->encoding($field); last SWITCH;};
	    ($field =~ s/^encoding(\s+)/$1/io) && do {$apiOwner->encoding($field); last SWITCH;};
            ($field =~ s/^ignore\s+//io) && do { $field =~ s/\n//smgo; $field =~ s/<br\s*\/?>//sgo;$field =~ s/^\s*//sgo; $field =~ s/\s*$//sgo;
		# push(@HeaderDoc::perHeaderIgnorePrefixes, $field);
		$HeaderDoc::perHeaderIgnorePrefixes{$field} = $field;
		if (!($reprocess_input)) {$reprocess_input = 1;} print STDERR "ignoring $field" if ($localDebug); last SWITCH;};

            # warn("$fullpath:$linenum: warning: Unknown field in header comment: $field\n");
	    warn("$fullpath:$linenum: warning: Unknown field (\@$field) in header comment.\n");
		}
	}

}

# /*! @function getLineArrays
#     @abstract splits the input files into multiple text blocks
#   */
sub getLineArrays {
    # @@@ DAG: This function does heavy text manipuation and is
    # the prime suspect in cases of missing text in comments, odd
    # spacing problems, and so on.

    my $classDebug = 0;
    my $localDebug = 0;
    my $blockDebug = 0;
    my $dumpOnly = 0;
    my $rawLineArrayRef = shift;
    my @arrayOfLineArrays = ();
    my @generalHeaderLines = ();
    my @classStack = ();

    my $lang = shift;
    my $sublang = shift;

    my $inputCounter = 0;
    my $lastArrayIndex = @{$rawLineArrayRef};
    my $line = "";
    my $className = "";
    my $classType = "";
    my $isshell = 0;

    if ($lang eq "shell" || $lang eq "perl") {
	$isshell = 1;
    }
    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $ilc_b, $sofunction,
	$soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$enumname,
	$typedefname, $varname, $constname, $structisbrace, $macronamesref,
	$classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp,
	$requiredregexp, $propname, $objcdynamicname, $objcsynthesizename) = parseTokens($lang, $sublang);

    # my $socquot = $HeaderDoc::socquot;
    # my $eocquot = $HeaderDoc::eocquot;
    # my $ilcquot = $HeaderDoc::ilcquot;

    if ($isshell) {
	$eoc = "*/";
	# $eocquot = $eoc;
	# $eocquot =~ s/(\W)/\\$1/sg;
    }

    while ($inputCounter <= $lastArrayIndex) {
        $line = ${$rawLineArrayRef}[$inputCounter];

	# inputCounter should always point to the current line being processed

        # we're entering a headerdoc comment--look ahead for @class tag
	my $startline = $inputCounter;

	print STDERR "MYLINE: \"$line\"\n" if ($localDebug);

	print STDERR "SOC: $soc\n" if ($localDebug);
	print STDERR "EOC: $eoc\n" if ($localDebug);
	print STDERR "ISSHELL: $isshell\n" if ($localDebug);

		# No reason to support ilc_b here.  It is used for # comment starts in PHP, but is not
		# supported for HeaderDoc comments to avoid collisions with the #!/usr/bin/perl shell magic.
		if (($isshell && $line =~ /\Q$ilc\E\s*\/\*\!(.*)$/s) ||
		    (!$isshell && 
		      (($line =~ /^\s*\Q$soc\E\!/s) ||
		       (($lang eq "java" || $HeaderDoc::parse_javadoc) &&
			($line =~ /^\s*\Q$soc\E\*[^*]/s)))))  {  # entering headerDoc comment
			print STDERR "inHDComment\n" if ($localDebug);
			my $headerDocComment = "";
			my @headerDocCommentLinesArray = ();
			print STDERR "RESET headerDocCommentLinesArray\n" if ($localDebug);
			{
				local $^W = 0;  # turn off warnings since -w is overly sensitive here
				my $in_textblock = 0; my $in_pre = 0;
				my $leaving_textblock = 0; my $leaving_pre = 0;
				while (($line !~ /\Q$eoc\E/s) && ($inputCounter <= $lastArrayIndex)) {
				    if ($isshell) {
					$line =~ s/^\s*#//s;
				    }
				    # if ($lang eq "java" || $HeaderDoc::parse_javadoc) {
					$line =~ s/\@ref\s+(\w+)\s*(\(\))?/<code>\@link $1\@\/link<\/code>/sgio;
					$line =~ s/\{\s*\@linkdoc\s+(.*?)\}/<i>\@link $1\@\/link<\/i>/sgio;
					$line =~ s/\{\s*\@linkplain\s+(.*?)\}/\@link $1\@\/link/sgio;
					$line =~ s/\{\s*\@link\s+(.*?)\}/<code>\@link $1\@\/link<\/code>/sgio;
					$line =~ s/\{\s*\@docroot\s*\}/\\\@\\\@docroot/sgio;
					# if ($line =~ /value/o) { warn "line was: $line\n"; }
					$line =~ s/\{\@value\}/\@value/sgio;
					$line =~ s/\{\@inheritDoc\}/\@inheritDoc/sgio;
					# if ($line =~ /value/o) { warn "line now: $line\n"; }
				    # }
				    $line =~ s/([^\\])\@docroot/$1\\\@\\\@docroot/sgi;
				    my $templine = $line;
				    # print STDERR "HERE: $templine\n";

				    $leaving_textblock = 0; $leaving_pre = 0;
				    while ($templine =~ s/\@textblock//sio) { $in_textblock++; }
				    while ($templine =~ s/\@\/textblock//sio) { $in_textblock--; $leaving_textblock = 1;}
				    while ($templine =~ s/<pre>//sio) { $in_pre++; }
				    while ($templine =~ s/<\/pre>//sio) { $in_pre--; $leaving_pre = 1; }

				    # $headerDocComment .= $line;
				    my %lineentry = ();
				    $lineentry{INTEXTBLOCK} = $in_textblock;
				    $lineentry{INPRE} = $in_pre;
				    $lineentry{LEAVINGTEXTBLOCK} = $leaving_textblock;
				    $lineentry{LEAVINGPRE} = $leaving_pre;
				    $lineentry{LINE} = $line;

				    my $ref = \%lineentry;

				    print STDERR "PUSH[1] $line ($ref)\n" if ($localDebug);

				    push(@headerDocCommentLinesArray, $ref);

				    # warnHDComment($rawLineArrayRef, $inputCounter, 0, "HeaderDoc comment", "32");
			            $line = ${$rawLineArrayRef}[++$inputCounter];
				    warnHDComment($rawLineArrayRef, $inputCounter, 0, "HeaderDoc comment", "33");
				}
				if ($isshell) {
				    $line =~ s/^\s*#//s;
				}
				$line =~ s/\{\s*\@linkdoc\s+(.*?)\}/<i>\@link $1\@\/link<\/i>/sgio;
				$line =~ s/\{\s*\@linkplain\s+(.*?)\}/\@link $1\@\/link/sgio;
				$line =~ s/\{\s*\@link\s+(.*?)\}/<code>\@link $1\@\/link<\/code>/sgio;
				# $line =~ s/\{\s*\@docroot\s*\}/\\\@\\\@docroot/sgio;
				# if ($line =~ /value/so) { warn "line was: $line\n"; }
				$line =~ s/\{\@value\}/\@value/sgio;
				$line =~ s/\{\@inheritDoc\}/\@inheritDoc/sgio;
				$line =~ s/\{\s*\@docroot\s*\}/\\\@\\\@docroot/sgo;

				my %lineentry = ();
				$lineentry{INTEXTBLOCK} = $in_textblock;
				$lineentry{INPRE} = $in_pre;
				$lineentry{LINE} = $line;
				print STDERR "PUSH[2] $line\n" if ($localDebug);

				push(@headerDocCommentLinesArray, \%lineentry);
				# $headerDocComment .= $line ;

				# warnHDComment($rawLineArrayRef, $inputCounter, 0, "HeaderDoc comment", "34");
				$line = ${$rawLineArrayRef}[++$inputCounter];

				$headerDocComment = filterHeaderDocComment(\@headerDocCommentLinesArray, $lang, $sublang, $inputCounter);

				# A HeaderDoc comment block immediately
				# after another one can be legal after some
				# tag types (e.g. @language, @header).
				# We'll postpone this check until the
				# actual parsing.
				# 
				if (!emptyHDok($headerDocComment)) {
					my $emptyDebug = 0;
					warn "curline is $line" if ($emptyDebug);
					print STDERR "HEADERDOC COMMENT WAS $headerDocComment\n" if ($localDebug);
					warnHDComment($rawLineArrayRef, $inputCounter, 0, "HeaderDoc comment", "35", $headerDocComment);
				}
			}
			if ($localDebug) { print STDERR "first line after $headerDocComment is $line\n"; }

			push(@generalHeaderLines, $headerDocComment);
			$inputCounter--;
			print STDERR "DECREMENTED INPUTCOUNTER [M10]\n" if ($HeaderDoc::inputCounterDebug);
		} else {
			push (@generalHeaderLines, $line); print STDERR "PUSHED $line\n" if ($blockDebug);
		}
		$inputCounter++;
		print STDERR "INCREMENTED INPUTCOUNTER [M11]\n" if ($HeaderDoc::inputCounterDebug);
	     }

	if ($localDebug || $dumpOnly) {
		print STDERR "DUMPING LINES.\n";
		for my $line (@generalHeaderLines) {
			print STDERR "$line";
		}
		print STDERR "DONE DUMPING LINES.\n";
	}

	push (@arrayOfLineArrays, \@generalHeaderLines);
	return @arrayOfLineArrays;
}

my %uid_list_by_uid_0 = ();
my %uid_list_by_uid_1 = ();
my %uid_list_0 = ();
my %uid_list_1 = ();
my %uid_conflict_0 = ();
my %uid_conflict_1 = ();
my %objid_hash_0 = ();
my %objid_hash_1 = ();

sub savehashes
{
	my $alldecs = shift;

	if ($alldecs) {
		%uid_list_by_uid_0 = %uid_list_by_uid;
		%uid_list_0 = %uid_list;
		%uid_conflict_0 = %uid_conflict;
		%objid_hash_0 = %objid_hash;
	} else {
		%uid_list_by_uid_1 = %uid_list_by_uid;
		%uid_list_1 = %uid_list;
		%uid_conflict_1 = %uid_conflict;
		%objid_hash_1 = %objid_hash;
	}
}

sub loadhashes
{
	my $alldecs = shift;

	if ($alldecs) {
		%uid_list_by_uid = %uid_list_by_uid_0;
		%uid_list = %uid_list_0;
		%uid_conflict = %uid_conflict_0;
		%objid_hash = %objid_hash_0;
	} else {
		%uid_list_by_uid = %uid_list_by_uid_1;
		%uid_list = %uid_list_1;
		%uid_conflict = %uid_conflict_1;
		%objid_hash = %objid_hash_1;
	}
}

sub getAbsPath
{
	my $filename = shift;
	if ($filename =~ /^\Q$pathSeparator\E/) {
		return $filename;
	}
	return cwd().$pathSeparator.$filename;
}

sub allow_everything
{
	my $lang = shift;
	my $sublang = shift;

	if ($lang eq "C") {
		# sublang :
		# C, occ, cpp,
		# php, IDL, MIG

		return 1;
	} elsif ($lang =~ /Csource/) {

		return 1;
	} elsif ($lang eq "java") {
		if ($sublang ne "javascript") {
			return 1;
		}
	} elsif ($lang eq "pascal") {
		return 1; # Maybe
	} elsif ($lang eq "perl") {
		return 1; # Maybe
	}

	return 0;
}

# /*! Get availability macro information from a file. */
sub getAvailabilityMacros
{
    my $filename = shift;
    my $quiet = shift;

    print STDERR "Reading availability macros from \"$filename\".\n" if (!$quiet);

    my @availabilitylist = ();

    if (-f $filename) {
	@availabilitylist = &linesFromFile($filename);
    } else {
	# @availabilitylist = &linesFromFile($filename);
	warn "Can't open $filename for availability macros\n";
    }

    foreach my $line (@availabilitylist) {
	my ($token, $description, $has_args) = split(/\t/, $line, 3);
	# print STDERR "Adding avail for $line\n";
	addAvailabilityMacro($token, $description, $has_args);
    }
}


1;

__END__

