#! /usr/bin/perl -w
# Utilities.pm
# 
# Common subroutines
# Last Updated: $Date: 2005/01/15 01:41:55 $
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
use Carp;
use Exporter;
foreach (qw(Mac::Files Mac::MoreFiles)) {
    eval "use $_";
}

$VERSION = '$Revision: 1.11.2.4.2.35 $';
@ISA = qw(Exporter);
@EXPORT = qw(findRelativePath safeName safeNameNoCollide linesFromFile makeAbsolutePath
             printHash printArray fileNameFromPath folderPathForFile convertCharsForFileMaker 
             updateHashFromConfigFiles getHashFromConfigFile getVarNameAndDisc
             getAPINameAndDisc openLogs
             logMsg logMsgAndWarning logWarning logToAllFiles closeLogs
             registerUID resolveLink quote parseTokens isKeyword html2xhtml
             resolveLinks stringToFields sanitize warnHDComment
             classTypeFromFieldAndBPinfo get_super casecmp unregisterUID);

my %uid_list = ();
my %uid_conflict = ();
my $xmllintversion = "";
my $xmllint = "/usr/bin/xmllint";

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
# print "STRING \"$xmllintversion\".\n";
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
    my $localDebug = 0;

    # If we start with a newline (e.g.
    #     @function
    #       discussion...
    # treat it like JavaDoc and let the block parser
    # pick up a name.
    print "LINE: $line\n" if ($localDebug);
    if ($line =~ /^\s*\n\s*/o) {
	print "returning discussion only.\n" if ($localDebug);
	$line =~ s/^\s+//o;
	return ("", "$line");
    }
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
	($name, $disc) = split (/\n/, $line, 2);
    } else {
	($name, $disc) = split (/\s/, $line, 2);
    }
    # ensure that if the discussion is empty, we return an empty
    # string....
    $disc =~ s/\s*$//o;
    
    if ($name =~ /operator/o) {  # this is for operator overloading in C++
        ($operator, $name, $disc) = split (/\s/, $line, 3);
        $name = $operator." ".$name;
    }
    # print "name is $name, disc is $disc";
    return ($name, $disc);
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
    	print "reading $configFile\n" if ($localDebug);
		open(INFILE, "<$configFile") || die "Can't open $configFile.\n";
		@lines = <INFILE>;
		close INFILE;
    } else {
        print "No configuration file found at $configFile\n" if ($localDebug);
        return;
    }
    
	foreach my $line (@lines) {
	    if ($line =~/^#/o) {next;};
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

sub resolveLink
{
    my $symbol = shift;
    my $ret = "";
    my $filename = $HeaderDoc::headerObject->filename();

    my $uid = $uid_list{$symbol};
	if ($uid && length($uid)) {
	    $ret = $uid;
	    if ($uid_conflict{$symbol}) {
		warn "$filename:0:WARNING: multiple matches found for symbol \"$symbol\"!!!\n";
		warn "$filename:0:Only the first matching symbol will be linked.\n";
		warn "$filename:0:Replace the symbol with a specific api ref tag\n";
		warn "$filename:0:(e.g. apple_ref) in header file to fix this conflict.\n";
	    }
	}
    if ($ret eq "") {
	warn "$filename:0:WARNING: no symbol matching \"$symbol\" found.  If this\n";
	warn "$filename:0:symbol is not in this file or class, you need to specify it\n";
	warn "$filename:0:with an api ref tag (e.g. apple_ref).\n";
    # } else {
	# warn "RET IS \"$ret\"\n"
    }
    return $ret;
}

sub registerUID($$)
{
    # This is now classless.
    # my $self = shift;
    my $uid = shift;
    my $name = shift;
    my $localDebug = 0;

    print "registered UID $uid\n" if ($localDebug);
    # my $name = $uid;
    # $name =~ s/.*\///so;

    my $old_uid = $uid_list{$name};
    if ($old_uid && length($old_uid) && $old_uid ne $uid) {
	print "OU: $old_uid NU: $uid\n" if ($localDebug);
	$uid_conflict{$name} = 1;
    }
    $uid_list{$name} = $uid;
    # push(@uid_list, $uid);
}

sub unregisterUID
{
    my $uid = shift;
    my $name = shift;
    my $old_uid = $uid_list{$name};

    if ($uid_list{$name} eq $uid) {
	$uid_list{$name} = undef;
	return 1;
    }

    return 0;
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
    my $sofunction = "";
    my $soprocedure = "";
    my $operator = "";
    my $sopreproc = "";
    my $lbrace = "";
    my $rbrace = "";
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

    print "PARSETOKENS FOR lang: $lang sublang: $sublang\n" if ($localDebug);

    if ($lang eq "perl" || $lang eq "shell") {
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
	$unionname = "";
	$structname = "";
	$typedefname = "";
	$varname = "";
	$constname = "";
	$structisbrace = 0;
    } elsif ($lang eq "pascal") {
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
	$unionname = "";
	$structname = "record";
	$typedefname = "type";
	$varname = "var";
	$constname = "const";
	$structisbrace = 1;
    } else {
	# C and derivatives, plus PHP and Java(script)
	$classregexp = "^(class)\$";
	if ($lang eq "C" && $sublang ne "php") {
		# if ($sublang eq "cpp" || $sublang eq "C") {
			$sotemplate = "<";
			$eotemplate = ">";
			$accessregexp = "^(public|private|protected)\$";
		# }
		$operator = "operator";
		$sopreproc = "#";
	} elsif ($lang eq "MIG") {
		$sopreproc = "#";
	}
	if ($lang eq "C" && $sublang ne "php") { # if ($sublang eq "occ" || $sublang eq "C") {
		$classregexp = "^(class|\@class|\@interface|\@protocol)\$";
		$classbraceregexp = "^(\@interface|\@protocol)\$";
		$classclosebraceregexp = "^(\@end)\$";
	}
	if ($lang eq "java" && $sublang eq "java") {
		$accessregexp = "^(public|private|protected|package)\$";
	} elsif ($sublang eq "php") {
		$accessregexp = "^(public|private|protected)\$";
	}
	$soc = "/*";
	$eoc = "*/";
	$ilc = "//";
	$lbrace = "{";
	$rbrace = "}";
	$unionname = "union";
	$structname = "struct";
	$typedefname = "typedef";
	$varname = "";
	$constname = "";
	$structisbrace = 0;
	# DO NOT DO THIS, no matter how tempting it may seem.
	# sofunction and soprocedure are only for functions/procedures
	# that do not follow the form '<type information> <name> ( <args> );'.
	# MIG does, so don't do this.
	# if ($sublang eq "MIG") {
		# $sofunction = "routine";
		# $soprocedure = "simpleroutine";
	# };
	if ($sublang ne "php") {
		# @macronames = ( "#if", "#ifdef", "#ifndef", "#endif", "#else", "#pragma", "#import", "#include", "#define" );
		%macronames = ( "#if" => 1, "#ifdef" => 1, "#ifndef" => 1, "#endif" => 1, "#else" => 1, "#pragma" => 1, "#import" => 1, "#include" => 1, "#define"  => 1);
	}
    }

    $HeaderDoc::socquot = $soc;
    $HeaderDoc::socquot =~ s/(\W)/\\$1/sg;
    $HeaderDoc::eocquot = $eoc;
    $HeaderDoc::eocquot =~ s/(\W)/\\$1/sg;
    $HeaderDoc::ilcquot = $ilc;
    $HeaderDoc::ilcquot =~ s/(\W)/\\$1/sg;

    return ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $sofunction,
	$soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$typedefname, $varname, $constname, $structisbrace, \%macronames,
	$classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp);
}

sub isKeyword
{
    my $token = shift;
    my $keywordref = shift;
    my $case_sensitive = shift;
    my %keywords = %{$keywordref};
    my $localDebug = 0;

    # if ($token =~ /^\#/o) { $localDebug = 1; }

    print "isKeyWord: TOKEN: $token\n" if ($localDebug);

    if ($case_sensitive) {
	if ($keywords{$token}) {
	    print "MATCH\n" if ($localDebug);
	    return 1;
	}
    } else {
      foreach my $keyword (keys %keywords) {
	print "isKeyWord: keyword: $keyword\n" if ($localDebug);
	my $quotkey = quote($keyword);
	if ($token =~ /^$quotkey$/i) {
		print "MATCH\n" if ($localDebug);
		return 1;
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

    # print "FAST PATH: ".$HeaderDoc::ignore_apiuid_errors."\n";

    local $/;
    my $xmlout = "--xmlout";
    if ($xmllintversion eq "20607") {
	$xmlout = "";
    }

# print "xmllint version is $xmllintversion\n";
# print "xmllint is $xmllint\n";

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
	# print "A:\n$htmllengthcheck\nB:\n$xhtmllengthcheck\n";
    }

    print "GOT XHTML (oldlen = ".length($html)."): $xhtml\n" if ($localDebug);

    my $retval = waitpid($pid, 0);
    my $exitstatus = $?;

    if ($exitstatus) {
	warn "DEBUGNAME: $debugname\n" if ($localDebug);
	warn "$debugname:XML to HTML translation failed.\n";
	warn "Error was $exitstatus\n";
    }


    return $xhtml;
}


sub resolveLinks($)
{
    my $path = shift;

    my $resolverpath = $HeaderDoc::modulesPath."bin/resolveLinks";

    # print "EXECUTING $resolverpath $path\n";
    my $retval = system($resolverpath." $path");

    if ($retval == -1) {
	warn "WARNING: resolveLinks not installed.  Please check your installation.\n";
    } elsif ($retval) {
	warn "WARNING: resolveLinks failed ($retval).  Please check your installation.\n";
    }
}


sub stringToFields($$$)
{
	my $line = shift;
	my $filename = shift;
	my $linenum = shift;

	my $localDebug = 0;

	print "LINE WAS: \"$line\"\n" if ($localDebug);

	my @fields = split(/\@/s, $line);
	my @newfields = ();
	my $lastappend = "";
	my $in_textblock = 0;
	my $in_link = 0;
	my $lastlinkfield = "";

	foreach my $field (@fields) {
	  print "processing $field\n" if ($localDebug);
	  if ($in_textblock) {
	    if ($field =~ /^\/textblock/so) {
		print "out of textblock\n" if ($localDebug);
		if ($in_textblock == 1) {
		    my $cleanfield = $field;
		    $cleanfield =~ s/^\/textblock//sio;
		    $lastappend .= $cleanfield;
		    push(@newfields, $lastappend);
		    print "pushed \"$lastappend\"\n" if ($localDebug);
		    $lastappend = "";
		}
		$in_textblock = 0;
	    } else {
		# clean up text block
		$field =~ s/\</\&lt\;/sgo;
		$field =~ s/\>/\&gt\;/sgo;
		$lastappend .= "\@$field";
		print "new field is \"$lastappend\"\n" if ($localDebug);
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
		$in_link = 0;
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
		# print "lastfield is $lastfield";
		$lastappend .= $lastfield; 
		if ($field =~ /^(\S*?)\s/so) {
		    $target = $1;
		} else {
		    # print "$filename:$linenum:MISSING TARGET FOR LINK!\n";
		    $target = $field;
		}
		my $localDebug = 0;
		print "target $target\n" if ($localDebug);
		my $qtarget = quote($target);
		$field =~ s/^$qtarget//sg;
		$field =~ s/\\$/\@/so;
		print "name $field\n" if ($localDebug);
		$lastappend .= "<hd_link posstarget=\"$target\">";
		$lastappend .= "$field";
	    } elsif ($field =~ /^textblock\s/sio) {
		if ($lastappend eq "") {
		    $in_textblock = 1;
		    print "in textblock\n" if ($localDebug);
		    $lastappend = pop(@newfields);
		} else {
		    $in_textblock = 2;
		    print "in textblock (continuation)\n" if ($localDebug);
		}
		$field =~ s/^textblock\s+//sio;
		# clean up text block
		$field =~ s/\</\&lt\;/sgo;
		$field =~ s/\>/\&gt\;/sgo;
		$lastappend .= "$field";
		print "in textblock.\n" if ($localDebug);
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
		warn "$filename:$linenum:Unterminated \@link tag (starting field was: $lastlinkfield)\n";
	}
	if ($in_textblock) {
		warn "$filename:$linenum:Unterminated \@textblock tag\n";
	}
	@fields = @newfields;

	if ($localDebug) {
		print "FIELDS:\n";
		for my $field (@fields) {
			print "$field\n";
		}
	}

	return \@fields;
}


# /*! Sanitize a string for use in a URL */
sub sanitize($)
{
    my $string = shift;
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
	# } elsif ($part =~ /\s/o) {
		# $newstring .= $part;
	} elsif ($part =~ /[\~\:\,\.\-\_\+\!\*\(\)\/]/o) {
		# We used to exclude '$' as well, but this
		# confused libxml2's HTML parser.
		$newstring .= $part;
	} else {
		# $newstring .= "%".ord($part);
		my $val = ord($part);
		my $valstring = sprintf("%02d", $val);
		$newstring .= "\%$valstring";
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

# print "DT: $dectype TG: $tag\n";

    # defineblock can only be passed in for debug point 12, so
    # this can't break anything.

    if ($dectype =~ /defineblock/o && $tag =~ /^\@define/o) {
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

    my $filename = $HeaderDoc::headerObject->filename();
    my $localDebug = 2; # Set to 2 so I wouldn't keep turning this off.
    my $rawLocalDebug = 0;
    my $maybeblock = 0;

print "DT: $dectype\n" if ($rawLocalDebug);

    if ($dectype =~ /blockMode:\#define/) {
	# print "DEFBLOCK?\n";
	$maybeblock = 1;
    }
    # if ($dectype =~ /blockMode:#define/ && ($tag =~ /^\@define/i || $tag !~ /^\@/)) {
	# return 2;
    # }

    my $line = ${$linearrayref}[$blocklinenum];
    my $linenum = $blocklinenum + $blockoffset;

	print "LINE WAS $line\n" if ($rawLocalDebug);

    my $isshell = 0;

    my $socquot = $HeaderDoc::socquot;
    my $ilcquot = $HeaderDoc::ilcquot;
    my $indefineblock = 0;

    if ($optional_lastComment =~ /\s*\/\*\!\s*\@define(d)?block\s+/) {
	print "INBLOCK\n" if ($rawLocalDebug);
	$indefineblock = 1;
	$dectype = "defineblock";
    } else {
	print "optional_lastComment: $optional_lastComment\n" if ($rawLocalDebug);
    }

    if (($HeaderDoc::lang eq "shell") || ($HeaderDoc::lang eq "perl")) {
	$isshell = 1;
    }

    my $debugString = "";
    if ($localDebug) { $debugString = " [debug point $dp]"; }

    if ((!$isshell && $line =~ /$socquot\!(.*)$/o) || ($isshell && $line =~ /$ilcquot\s*\/\*\!(.*)$/o)) {
	my $rest = $1;

	$rest =~ s/^\s*//so;
	$rest =~ s/\s*$//so;

	while (!length($rest) && ($blocklinenum < scalar(@{$linearrayref}))) {
		$blocklinenum++;
		$rest = ${$linearrayref}[$blocklinenum];
		$rest =~ s/^\s*//so;
		$rest =~ s/\s*$//so;
	}

	print "REST: $rest\nDECTYPE: $dectype\n" if ($rawLocalDebug);

	if ($rest =~ /^\@/o) {
		 if (nestignore($rest, $dectype)) {
#print "IGNORE\n" if ($rawLocalDebug);
			return 0;
		}
	} else {
		printf("Nested headerdoc markup with no tag.\n") if ($rawLocalDebug);
	}

	if (!$HeaderDoc::ignore_apiuid_errors) {
		warn("$filename:$linenum: WARNING: Unexpected headerdoc markup found in $dectype declaration$debugString.  Output may be broken.\n");
	}
	if ($maybeblock) {
		if ($rest =~ /^\s*\@define(d?)\s+/) {
			return 2;
		}
		if ($rest =~ /^\s*[^\@\s]/) {
			return 2;
		}
	}
	return 1;
    }
#print "OK\n";
    return 0;
}

sub get_super {
    my $classType = shift;
    my $dec = shift;
    my $super = "";
    my $localDebug = 0;

    print "GS: $dec EGS\n" if ($localDebug);

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

    print "$super is super\n" if ($localDebug);
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
	my $filename = shift;
	my $linenum = shift;
	my $sublang = shift;

	my $deccopy = $classBPdeclaration;
	$deccopy =~ s/[\n\r]/ /s;
	$deccopy =~ s/\{.*$//sg;
	$deccopy =~ s/\).*$//sg;
	$deccopy =~ s/;.*$//sg;

	# print "DC: $deccopy\n";
	# print "CBPT: $classBPtype\n";

	SWITCH: {
		($classBPtype =~ /^\@protocol/) && do { return "intf"; };
		($classKeyword =~ /category/) && do { return "occCat"; };
		# ($classBPtype =~ /^\@class/) && do { return "occCat"; };
		($classBPtype =~ /^\@class/) && do { return "occ"; };
		($classBPtype =~ /^\@interface/) && do {
				if ($classKeyword =~ /class/) {
					return "occ";
				} elsif ($deccopy =~ /\:/s) {
					# print "CLASS: $deccopy\n";
					return "occ";
				} elsif ($deccopy =~ /\(/s) {
					# print "CATEGORY: $deccopy\n";
					return "occCat";
				} else {
					last SWITCH;
				}
			};
		($classKeyword =~ /class/) && do { return $sublang; };
		($classBPtype =~ /typedef/) && do { return "C"; };
		($classBPtype =~ /struct/) && do { return "C"; };
		($classBPtype =~ /class/) && do { return $sublang; };
	}
	warn "$filename:$linenum:Unable to determine class type.\n";
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


1;

__END__

