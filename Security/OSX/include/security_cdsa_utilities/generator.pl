#!/usr/bin/perl
#
# generator.pl - derive various and sundry C++ code from the CDSA header files
#
# Usage:
#	perl generator.pl input-directory output-directory
#
# Perry The Cynic, Fall 1999.
#
$ERR_H="cssmerr.h";
$APPLE_ERR_H="cssmapple.h";

$SOURCEDIR=$ARGV[0];						# directory with cdsa headers
$TARGETDIR=$ARGV[1];						# where to put the output file
@INPUTFILES=@ARGV[2 .. 9999];				# list of input files

$TABLES="$TARGETDIR/errorcodes.gen";		# error name tables

$tabs = "\t\t\t";	# argument indentation (noncritical)
$warning = "This file was automatically generated. Do not edit on penalty of futility!";


#
# Parse CSSM error header and build table of all named codes
#
open(ERR, "$SOURCEDIR/$ERR_H") or die "Cannot open $ERR_H: $^E";
open(APPLE_ERR, "$SOURCEDIR/$APPLE_ERR_H") or die "Cannot open $APPLE_ERR_H: $^E";
$/=undef;	# big gulp mode
$errors = <ERR> . <APPLE_ERR>;
close(ERR); close(APPLE_ERR);

@fullErrors = $errors =~ /^\s+CSSMERR_([A-Z_]+)/gm;
@convertibles = $errors =~ /^\s+CSSM_ERRCODE_([A-Z_]+)\s*=\s*([0-9xXA-Fa-f]+)/gm;

while ($name = shift @convertibles) {
  $value = shift @convertibles or die;
  $convErrors[hex $value] = $name;
};


#
# Read Keychain-level headers for more error codes (errSecBlahBlah)
#
open(ERR, "cat " . join(" ", @INPUTFILES) . "|") or die "Cannot open error header files";
$/=undef;	# still gulping
$_ = <ERR>;
@kcerrors = /err((?:Sec|Authorization)\w+)\s*=\s*-?\d+/gm;
close(ERR);


#
# Now we will generate the error name tables.
#
open(OUT, ">$TABLES") or die "Cannot write $TABLES: $^E";
select OUT;

print <<HDR;
//
// CSSM error code tables.
// $warning
//
static const Mapping errorList[] = {
HDR
foreach $name (@fullErrors) {
  print "  { CSSMERR_$name, \"$name\" },\n";
};
foreach $err (@kcerrors) {
  print "  { err$err, \"$err\" },\n";
};
print <<MID;
};

static const char * const convErrorList [] = {
MID
for ($n = 0; $n <= $#convErrors; $n++) {
  if ($name = $convErrors[$n]) {
    print "  \"$name\",\n";
  } else {
    print "  NULL,\n";
  };
};
print "};\n";

close(OUT);
select(STDOUT);

$nFull = $#fullErrors + 1;
$nConv = $#convertibles + 1;
print "$nFull errors available to error translation functions.\n";
