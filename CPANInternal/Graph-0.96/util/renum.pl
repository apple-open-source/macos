my $MANIFEST;
my %MANIFEST;
my @MANIFEST;
if (open(MANIFEST, "MANIFEST")) {
    while (<MANIFEST>) {
	unless (m:^t/(\d+(\.\d+)?_+.+):) {
	    chomp;
	    push @MANIFEST, $_;
	}
    }
    close(MANIFEST);
} else {
    die "$0: opening 'MANIFEST' failed: $!\n";
}
die "$0: chdir 't' failed: $!\n" unless chdir("t");
my @t = sort { $a <=> $b } glob("[0-9]*.t");
my @r;
for my $i (0..$#t) {
    my $u = $t[$i];
    if ($u =~ s/^\d+(\.\d+)?_+//) {
	$u = sprintf "%02g_$u", $i;
	if ($u ne $t[$i]) {
	    my $r = [ $t[$i], $u ];
	    if (-e $r->[1]) {
		die "$0: renaming $t[$i] - $r->[1] already exists, whole rename aborted.\n";
	    } else {
		push @r, $r;
	    }
	}
	$MANIFEST{"t/$t[$i]"} = "t/$u\n";
    }
}
my $n = 0;
for my $r (@r) {
    my ($old, $new) = @$r;
    if (rename($old, $new)) {
	$n++;
    } else {
	warn "$0: rename $old $new failed: $!\n";
    }
}
printf "%s t/*.t file%s renamed.\n", $n ? $n : 'No', $n == 1 ? '' : 's';
push @MANIFEST, keys %MANIFEST;
if (open(MANINEW, ">MANIFEST.new")) {
    for my $mani (sort @MANIFEST) {
	print MANINEW "$mani\n"
	    or die "$0: MANIFEST.new print: $!\n";
    }
    close(MANINEW)
	or die "$0: MANIFEST.new print: $!\n";
    rename("MANIFEST.new", "../MANIFEST")
	or warn "$0: rename MANIFEST.new ../MANIFEST: $!\n";
} else {
    die "$0: MANIFEST.new create: $!\n";
}
printf "MANIFEST %supdated.\n", $n ? "" : "not ";
exit(0);
