#!/usr/bin/perl
#
# generatorX.pl - create error strings files from the Security header files
#
# John Hurley, Summer 2003. Based on generator.pl, Perry The Cynic, Fall 1999.
#
# Usage:
#	perl generatorX.pl input-directory output-directory <files>
#
#	Currently supported files are SecBase.h, SecureTransport.h and Authorization.h
#
#	perl generatorX.pl `pwd` `pwd` SecBase2.h SecureTransport2.h Authorization.h
#
#	Input will be like:
#
#		errSSLProtocol				= -9800,	/* SSL protocol error */
#		errSSLNegotiation			= -9801,	/* Cipher Suite negotiation failure */
#
#	Output should be like (in Unicode):
#
#		/* errSSLProtocol */
#		"-9800" = "SSL protocol error";
#
#		/* errSSLNegotiation */
#		"-9801" = "Cipher Suite negotiation failure";
#
# Note that the list of errors must be numerically unique across all input files, or the strings file
# will be invalid.Comments that span multiple lines will be ignored, as will lines with no comment. C++
# style comments are not supported.
#

use Encode;

$SOURCEDIR=$ARGV[0];				# directory with error headers
$TARGETDIR=$ARGV[1];				# where to put the output file
@INPUTFILES=@ARGV[2 .. 9999];			# list of input files

$TABLES="$TARGETDIR/SecErrorMessages.strings";	# error strings 

$tabs = "\t\t\t";	# argument indentation (noncritical)
$warning = "This file was automatically generated. Do not edit on penalty of futility!";

#
# Parse error headers and build array of all relevant lines
#

open(ERR, "cat " . join(" ", @INPUTFILES) . "|") or die "Cannot open error header files";
$/=undef;	# still gulping
$_ = <ERR>;
@errorlines = m{(?:^\s*)(err[Sec|Authorization|SSL]\w+)(?:\s*=\s*)(-?\d+)(?:\s*,?\s*)(?:/\*\s*)(.*)(?:\*/)(?:$\s*)}gm;
close(ERR);

$nFull = $#errorlines / 3;

#
# Now we will generate the error name tables.
#

open(OUT, ">$TABLES") or die "Cannot write $TABLES: $^E";
select OUT;

# Print warning comment
$msg = "//\n// Security error code tables.\n// $warning\n//\n";

# Print the error messages
while ($errx = shift @errorlines)
{
    $value = shift @errorlines;	# or die;
    $str = shift @errorlines;	# or die;
    $str =~ s/\s*$//;		# drop trailing white space
    if ( $value != 0)  		# can't output duplicate error codes
    {
        $msg = $msg . "\n/* $errx */\n\"$value\" = \"$str\";\n";
    }
};
$msg = $msg . "\n";
$output = encode("UTF-16", $msg,  Encode::FB_PERLQQ);
print "$output";

close(OUT);
select(STDOUT);

#print "$nFull errors available to error translation functions.\n";
