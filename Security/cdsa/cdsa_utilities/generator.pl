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

$SOURCEDIR=$ARGV[0];						# directory with inputs

(${D}) = $SOURCEDIR =~ m@([/:])@;			# guess directory delimiter
sub macintosh() { return ${D} eq ':'; }

if( macintosh() ){
$TARGETDIR=$ARGV[2];						# directory for outputs
}
 else{
$TARGETDIR=$ARGV[1];
}

$TABLES="$TARGETDIR${D}errorcodes.gen";		# error name tables

$tabs = "\t\t\t";	# argument indentation (noncritical)
$warning = "This file was automatically generated. Do not edit on penalty of futility!";


#
# Parse CSSM error header and build table of all named codes
#
open(ERR, "$SOURCEDIR${D}$ERR_H") or die "Cannot open $ERR_H: $^E";
open(APPLE_ERR, "$SOURCEDIR${D}$APPLE_ERR_H") or die "Cannot open $APPLE_ERR_H: $^E";
$/=undef;	# big gulp mode
$errors = <ERR> . <APPLE_ERR>;
$errors =~ tr/\012/\015/ if macintosh;
close(ERR); close(APPLE_ERR);

@fullErrors = $errors =~ /^\s+CSSMERR_([A-Z_]+)/gm;
@convertibles = $errors =~ /^\s+CSSM_ERRCODE_([A-Z_]+)\s*=\s*([0-9xXA-Fa-f]+)/gm;

while ($name = shift @convertibles) {
  $value = shift @convertibles or die;
  $convErrors[hex $value] = $name;
};

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
