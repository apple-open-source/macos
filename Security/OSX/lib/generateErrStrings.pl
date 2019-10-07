#!/usr/bin/perl
#
# Copyright (c) 2003-2004,2006,2008,2012,2014 Apple Inc. All Rights Reserved.
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
# Style A has the comment after the value. Style has the comment before the value,
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
# C++ style comments are not supported. Double quotes in a comment are hardened with
# "\" in the output.
#
# The English versions of the error messages can be seen with:
#
#	cat /System/Library/Frameworks/Security.framework/Resources/en.lproj/SecErrorMessages.strings
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
									#	${DERIVED_SRC}/en.lproj/SecErrorMessages.strings
@INPUTFILES=@ARGV[3 .. 9999];		# list of input files

$#INPUTFILES = $#ARGV - 3;			# truncate to actual number of files

print "gend: $GENDEBUGSTRINGS, tmpdir: $TMPDIR, targetstr: $TARGETSTR\n";
open STRINGFILE, "> $TARGETSTR"  or die "can't open $TARGETSTR: $!";
select STRINGFILE;
binmode STRINGFILE, ":encoding(UTF-16)";

# -----------------------------------------------------------------------------------
# Parse error headers and build array of all relevant lines
open(ERR, "cat " . join(" ", @INPUTFILES) . "|") or die "Cannot open error header files";
$/="\};";	#We set the section termination string - very important
processInput();
close(ERR);
# -----------------------------------------------------------------------------------

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
        ($enum) = ($line =~ /\n\s*(?:enum|CF_ENUM\(OSStatus\))\s*{\s*([^}]*)};/);
        while ($enum ne '')	#basic filter for badly formed enums
        {
            #Drop leading whitespace
            $enum =~ s/^\s+//;

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
            #print "identifier: ", $identifier,"\n" if ($identifier ne '');

            ($value) = ($enum =~ /\s*[_A-Za-z][_A-Za-z0-9]*\s*=\s*(-?[0-9]*),/);
            #print "value: ", $value,"\n" if ($value ne '');

            #Drop everything before the comma, end of line or trailing comment
            $enum =~ s/[^,]*(,|\n|(\/\*))//;

            # Now look for trailing comment. We only consider them
            # trailing if they come before the end of the line
            ($trailingcomment) = ($enum =~ /^[ \t]*\/\*((.)*)?\*\//);
            $trailingcomment = cleanupComment($trailingcomment);

            #Drop everything before the end of line
            $enum =~ s/[^\n]*[\n]*//;

            #print "lc:$leadingcomment, id:$identifier, v:$value, tc:$trailingcomment\n";
            #print "===========================================\n";

            writecomment($leadingcomment, $identifier, $trailingcomment, $value);
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
	
	my($mylc,$myid,$mytc,$myvalue) = @_;
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
			print "/* ", $myid, " */\n\"";
            print $myvalue, "\" = \"";
			print $errormessage, "\";\n\n";
		}
	}
};

sub cleanupComment
{
	my $comment = shift @_;
#	print "A:",$comment,"\n";
	if ($comment ne '')
	{
		$comment =~ s/\s\s+/ /g; 	# Squeeze multiple spaces to one
		$comment =~ s/^\s+//;		# Drop leading whitespace
		$comment =~ s/\s+$//;		# Drop trailing whitespace
		$comment =~ s/[\"]/\\\"/g; 	# Replace double quotes with \" (backslash is sextupled to make it through regex and printf)
	}
#	print "B:",$comment,"\n";
	$comment;
}
