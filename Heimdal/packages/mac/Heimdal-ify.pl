#!/usr/bin/perl

use strict;
use File::Basename;

my ($replace, $in, $outdir) = ($ARGV[0], $ARGV[1], $ARGV[2]);

my $f = $outdir . "/" . basename($in);

my ($IN, $OUT);

open IN, "$in" or die "failed to open $in";
open OUT, ">$f" or die "failed to open $f";

while(<IN>) {
    if ($replace eq "GSS" and m/krb5-types.h/) {
	s/<krb5-types.h>/<inttypes.h>/;
    }
    if (m/_err.h/ or m/_asn1.h/ or m/krb5.*\.h/ or m/com_.*\.h/ or m/gssapi.*\.h/ or m/.*-protos\.h/ or /heimbase.h/) {
	s/#include +\<(.*).h>/#include <${replace}\/$1.h>/;
    }
    print OUT;
}

close IN;
close OUT;

exit 0;
