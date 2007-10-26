#!/usr/bin/perl
#
# Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
# generateErrStrings.pl - create error strings files from the Security header files
#
# Usage:
#
#	perl generateErrStrings.pl <GENDEBUGSTRS?> <NAME_OF_STRINGS_FILE> <input files>
#
#	Currently supported files are SecBase.h, SecureTransport.h,cssmapple.h,
#	cssmerr.h and Authorization.h. These are used by:
#
#		void cssmPerror(const char *how, CSSM_RETURN error);
#	
#	which is in SecBase.cpp.
#
# Paths of input files:
#
#	./libsecurity_authorization/lib/Authorization.h
#	./libsecurity_cssm/lib/cssmapple.h
#	./libsecurity_cssm/lib/cssmerr.h
#	./libsecurity_keychain/lib/SecBase.h
#	./libsecurity_ssl/lib/SecureTransport.h
#
# Sample run:
#
#	perl generateErrStrings.pl "YES" "SecErrorMessages.strings" Authorization.h SecBase.h \
#		cssmapple.h cssmerr.h SecureTransport.h
#
# Input to script: header file(s) containing enum declarations
# Output: C++ program with one cout statement per decl
#
# The input headers are scanned for enums containing error numbers and
# optional comments. Only certain prefixes for the identifiers in the
# enums are considered, to avoid non-error message type defines. See
# the line in the file with CSSM_ERRCODE for acceptable prefixes.
#
# There are three styles of comments that this script parses:
#
#	Style A [see /System/Library/Frameworks/Security.framework/Headers/SecBase.h]:
#
#		errSSLProtocol				= -9800,	/* SSL protocol error */
#
#	Style B [see /System/Library/Frameworks/Security.framework/Headers/cssmapple.h]:
#
#		/* a code signature match failed */
#		CSSMERR_CSP_APPLE_SIGNATURE_MISMATCH = CSSM_CSP_PRIVATE_ERROR + 2,
#
#	Style C [see /System/Library/Frameworks/Security.framework/Headers/cssmerr.h]:
#
#		CSSM_CSSM_BASE_CSSM_ERROR =
#			CSSM_CSSM_BASE_ERROR + CSSM_ERRORCODE_COMMON_EXTENT + 0x10,
#		CSSMERR_CSSM_SCOPE_NOT_SUPPORTED =				CSSM_CSSM_BASE_CSSM_ERROR + 1,
#
# Style A has the comment after the comment. Style has the comment before the value,
# and Style C has no comment. In cases where both Style A and B apply, the
# comment at the end of the line is used.
#
# The final output after the generated Objective-C++ program is run looks like:
#
#		/* errSSLProtocol */
#		"-9800" = "SSL protocol error";
#
#		/* errSSLNegotiation */
#		"-9801" = "Cipher Suite negotiation failure";
#
# The appropriate byte order marker for UTF-16 is written to the start of the file.
# Note that the list of errors must be numerically unique across all input files, 
# or the strings file will be invalid. Comments in "Style B" may span multiple lines.
# C++ style comments are not supported. Any single or double quote in a comment is
# converted to a "-" in the output.
#
# The English versions of the error messages can be seen with:
#
#	cat /System/Library/Frameworks/Security.framework/Resources/English.lproj/SecErrorMessages.strings
#
# find -H -X -x . -name "*.h" -print0 2>/dev/null | xargs -0 grep -ri err
# -----------------------------------------------------------------------------------

# Style questions:
#	- what should I make PROGNAME?
#	- should I use a special call to make the temp file in the .mm file?
#

#use strict;
#use warnings;

die "Usage:  $0 <gendebug> <tmpdir> <.strings file> <list of headers>\n" if ($#ARGV < 3);

$GENDEBUGSTRINGS=$ARGV[0];			# If "YES", include all strings & don't localize 
$TMPDIR=$ARGV[1];					# temporary directory for program compile, link, run
$TARGETSTR=$ARGV[2];				# path of .strings file, e.g. 
									#	${DERIVED_SRC}/English.lproj/SecErrorMessages.strings
@INPUTFILES=@ARGV[3 .. 9999];		# list of input files

$#INPUTFILES = $#ARGV - 3;			# truncate to actual number of files

print "gend: $GENDEBUGSTRINGS, tmpdir: $TMPDIR, targetstr: $TARGETSTR\n";
$PROGNAME="${TMPDIR}/generateErrStrings.mm";
open PROGRAM,"> $PROGNAME"  or die "can't open $PROGNAME: $!";
select PROGRAM;

printAdditionalIncludes();
printInputIncludes();
printMainProgram();

# -----------------------------------------------------------------------------------
# Parse error headers and build array of all relevant lines
open(ERR, "cat " . join(" ", @INPUTFILES) . "|") or die "Cannot open error header files";
$/="\};";	#We set the section termination string - very important
processInput();
close(ERR);
# -----------------------------------------------------------------------------------

printTrailer();
select STDOUT;
close PROGRAM;

compileLinkAndRun();

# 4: Done!
exit;

# -----------------------------------------------------------------------------------
#			Subroutines
# -----------------------------------------------------------------------------------

sub processInput
{
	# 3: Read input, process each line, output it.
	while ( $line = <ERR>)
	{
		($enum) = ($line =~ /\n\s*enum\s*{\s*([^}]*)};/);
		while ($enum ne '')	#basic filter for badly formed enums
		{
			#Drop leading whitespace
			$enum =~ s/^\s+//;
	#	print "A:", $enum,"\n";
			($leadingcomment) = ($enum =~ m%^(/\*([^*]|[\r\n]|(\*+([^*/]|[\r\n])))*\*+/)|(//.*)%);
			if ($leadingcomment ne '')
			{	
				$enum = substr($enum, length($leadingcomment));
				$leadingcomment = substr($leadingcomment, 2);		# drop leading "/*"
				$leadingcomment = substr($leadingcomment, 0, -2);	# drop trailing "*/"
				$leadingcomment = cleanupComment($leadingcomment);
			}
			next if ($enum eq '');	#basic filter for badly formed enums
			
			# Check for C++ style comments at start of line
			if ($enum =~ /\s*(\/\/)/)
			{
				#Drop everything before the end of line
				$enum =~ s/[^\n]*[\n]*//;
				next;
			}
			($identifier) = ($enum =~ /\s*([_A-Za-z][_A-Za-z0-9]*)/);
			
#			print "identifier: ", $identifier,"\n" if ($identifier ne '');
			
			#Drop everything before the comma
			$enum =~ s/[^,]*,//;
	
			# Now look for trailing comment. We only consider them
			# trailing if they come before the end of the line
			($trailingcomment) = ($enum =~ /^[ \t]*\/\*((.)*)?\*\//);
		#	($trailingcomment) = ($enum =~ m%^(/\*([^*]|[\r\n]|(\*+([^*/]|[\r\n])))*\*+/)|(//.*)%);
			$trailingcomment = cleanupComment($trailingcomment);
			
			#Drop everything before the end of line
			$enum =~ s/[^\n]*[\n]*//;
	#	print "B:", $enum,"\n";
	#	print "lc:$leadingcomment, id:$identifier, tc:$trailingcomment\n";
	#	print "===========================================\n";
		
			writecomment($leadingcomment, $identifier, $trailingcomment);
		}	
	}
}

sub writecomment
{
	# Leading comment, id, trailing comment
	# To aid localizers, we will not output a line with no comment
	#
	# Output is e.g.
	#	tmp << "/* errAuthorizationSuccess */\n\"" << errAuthorizationSuccess 
	#		<< "\" = \"The operation completed successfully.\"\n" << endl;
	
	my($mylc,$myid,$mytc) = @_;
	if ($myid =~ /(CSSM_ERRCODE|CSSMERR_|errSec|errCS|errAuth|errSSL)[_A-Za-z][_A-Za-z0-9]*/)
	{
		$errormessage = '';
		if ($mytc ne '')
		{	$errormessage = $mytc; }
		elsif ($mylc ne '')
		{	$errormessage = $mylc; }
		elsif ($GENDEBUGSTRINGS eq "YES")
		{	$errormessage = $myid; }
		
		if ($errormessage ne '')
		{
			print "\ttmp << \"/* ", $myid, " */\\n\\\"\" << ";
			print $myid, " << \"\\\" = \\\"";
			print $errormessage, "\\\";\\n\" << endl;\n";
		}
	}
};

 
sub printAdditionalIncludes
{
	#This uses the "here" construct to dump out lines verbatim
	print <<"AdditionalIncludes";

#include <iostream>
#include <fstream>
#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>

using namespace std;
AdditionalIncludes
}

sub printInputIncludes
{
	#Now "#include" each of the input files
	print "\n#include \"$_\"" foreach @INPUTFILES;
	print "\n";
}

sub printMainProgram
{
	#Output the main part of the program using the "here" construct
	print <<"MAINPROGRAM";

void writeStrings(const char *stringsFileName);
void createStringsTemp();

int main (int argc, char * const argv[])
{
	const char *stringsFileName = NULL;
   
	if (argc == 2)
		stringsFileName = argv[1];
	else
	if (argc == 1)
		stringsFileName = "SecErrorMessages.strings";
	else
		return -1;

	cout << "Strings file to create: " << stringsFileName << endl;
	createStringsTemp();
	writeStrings(stringsFileName);
}

void writeStrings(const char *stringsFileName)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSFileHandle *fh = [NSFileHandle fileHandleForReadingAtPath:@"generateErrStrings.tmp"];
	NSData *rawstrings = [fh readDataToEndOfFile];
	UInt32 encoding = CFStringConvertEncodingToNSStringEncoding (kCFStringEncodingUTF8);
	NSString *instring = [[NSString alloc] initWithData:rawstrings encoding:(NSStringEncoding)encoding];

	if (instring)
	{
		NSString *path = [NSString stringWithUTF8String:stringsFileName];
		NSFileManager *fm = [NSFileManager defaultManager];
		if ([fm fileExistsAtPath:path])
			[fm removeFileAtPath:path handler:nil];
		BOOL bx = [fm createFileAtPath:path contents:nil attributes:nil];
		NSFileHandle *fs = [NSFileHandle fileHandleForWritingAtPath:path];
		[fs writeData:[instring dataUsingEncoding:NSUnicodeStringEncoding]];
	}

	[pool release];
}

void createStringsTemp()
{
	ofstream tmp("generateErrStrings.tmp") ; 

MAINPROGRAM
}

sub cleanupComment
{
	my $comment = shift @_;
#	print "A:",$comment,"\n";
	if ($comment ne '')
	{
		$comment =~ s/\s\s+/ /g; 	# Squeeze multiple spaces to one
		$comment =~ s/^\s+//;		# Drop leading whitespace
		$comment =~ s/\s+$//;		# Drop trailing whitespace
		$comment =~ s/[\'\"]/-/g; 	# Replace quotes with -
	}
#	print "B:",$comment,"\n";
	$comment;
}    

sub printTrailer
{
	print "	tmp.close();\n";
	print "}\n";
}

sub compileLinkAndRun
{
	$status = system( <<"MAINPROGRAM");
(cd ${TMPDIR} ; /usr/bin/cc -x objective-c++  -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -Wreturn-type -fmessage-length=0 -F$ENV{'BUILT_PRODUCTS_DIR'} -I$ENV{'BUILT_PRODUCTS_DIR'}/SecurityPieces/Headers -I$ENV{'BUILT_PRODUCTS_DIR'}/SecurityPieces/PrivateHeaders -c generateErrStrings.mm -o generateErrStrings.o)
MAINPROGRAM
	die "$compile exited funny: $?" unless $status == 0;

	$status = system( <<"LINKERSTEP");
(cd ${TMPDIR} ; /usr/bin/g++ -o generateErrStrings generateErrStrings.o -framework Foundation )
LINKERSTEP
	die "$linker exited funny: $?" unless $status == 0;

	$status = system( <<"RUNSTEP");
(cd ${TMPDIR} ; ./generateErrStrings $TARGETSTR )
RUNSTEP
	die "$built program exited funny: $?" unless $status == 0;
}

