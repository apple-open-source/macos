package Common;

#--------------------
#
# GLOBALS...
#
#--------------------

use vars qw(@DATA_SA
	    @DATA_LA
	    $DATA_S

	    @ADATA_SA
	    $ADATA_S

	    $FDATA_S
	    @FDATA_LA
	    );

#------------------------------

# Data...
#    ...as a scalar-array:
@DATA_SA = (
"A diner while ",
"dining at Crewe\n",
"Found a rather large ",
"mouse in his stew\n   Said the waiter, \"Don't shout,\n",
"   And ",
"wave it about..."
);
#    ...as a string:
$DATA_S = join '', @DATA_SA;
#    ...as a line-array:
@DATA_LA = lines($DATA_S);

# Additional data...
#    ...as a scalar-array:
@ADATA_SA = (
"\nor the rest",
" will be wanting one ", 
"too.\"\n",
);
#    ...as a string:
$ADATA_S = join '', @ADATA_SA;


# Full data...
#    ...as a string:
$FDATA_S = $DATA_S . $ADATA_S;    
#    ...as a line-array:
@FDATA_LA = lines($FDATA_S);




# Tester:
my $T;

# Scratch...
my $BUF = '';      # buffer
my $M;             # message


#------------------------------
# lines STR
#------------------------------
sub lines {
    my $s = shift;
    split /^/, $s;
}

#------------------------------
# test_init PARAMHASH
#------------------------------
# Init common tests.
#
sub test_init {
    my ($self, %p) = @_;
    $T = $p{TBone};
}

#------------------------------
# test_print HANDLE, TEST
#------------------------------
# Test printing to handle.
# 1
#
sub test_print {
    my ($self, $GH, $all) = @_;
    local($_);

    # Append with print:
    $M = "PRINT: able to print to $GH";
    $GH->print($ADATA_SA[0]);
    $GH->print(@ADATA_SA[1..2]);
    $T->ok(1, $M);
}

#------------------------------
# test_getc HANDLE
#------------------------------
# Test getc().
# 1
#
sub test_getc {
    my ($self, $GH) = @_;
    local($_);
    my @c;

    $M = "GETC: seek(0,0) and getc()";
    $GH->seek(0,0);
    for (0..2) { $c[$_] = $GH->getc };
    $T->ok((($c[0] eq 'A') &&
	    ($c[1] eq ' ') &&
	    ($c[2] eq 'd')), $M);
}

#------------------------------
# test_getline HANDLE
#------------------------------
# Test getline() and getlines().
# 4
#
sub test_getline {
    my ($self, $GH) = @_;
    local($_);

    $M = "GETLINE/SEEK3: seek(3,START) and getline() gets part of 1st line";
    $GH->seek(3,0);
    my $got  = $GH->getline;	
    my $want = "iner while dining at Crewe\n";
    $T->ok(($got eq $want), $M,
	   GH   => $GH,
	   Got  => $got,
	   Want => $want);

    $M = "GETLINE/NEXT: next getline() gets subsequent line";
    $_ = $GH->getline;	
    $T->ok(($_ eq "Found a rather large mouse in his stew\n"), $M,
	   Got => $_);

    $M = "GETLINE/EOF: repeated getline() finds end of stream";
    my $last;
    for (1..6) { $last = $GH->getline }
    $T->ok(!$last, $M,
	   Last => (defined($last) ? $last : 'undef'));

    $M = "GETLINE/GETLINES: seek(0,0) and getlines() slurps in string";
    $GH->seek(0,0);
    my @got  = $GH->getlines;
    my $gots = join '', @got;
    $T->ok(($gots eq $FDATA_S), $M,	   
	   GotAll  => $gots,
	   WantAll => $FDATA_S,
	   Got     => \@got);
}

#------------------------------
# test_read HANDLE
#------------------------------
# Test read().
# 4
#
sub test_read {
    my ($self, $GH) = @_;
    local($_);

    $M = "READ/FIRST10: reading first 10 bytes with seek(0,START) + read(10)";
    $GH->seek(0,0);
    $GH->read($BUF,10);
    $T->ok(($BUF eq "A diner wh"), $M);
    
    $M = "READ/NEXT10: reading next 10 bytes with read(10)";
    $GH->read($BUF,10);
    $T->ok(($BUF eq "ile dining"), $M);
	 
    $M = "READ/TELL20: tell() the current location as 20";
    $T->ok(($GH->tell == 20), $M);

    $M = "READ/SLURP: seek(0,START) + read(1000) reads in whole handle";
    $GH->seek(0,0);
    $GH->read($BUF,1000);
    $T->ok(($BUF eq $FDATA_S), $M);
}

#------------------------------
# test_seek HANDLE
#------------------------------
# Test seeks other than (0,0).
# 2
#
sub test_seek {
    my ($self, $GH) = @_;
    local($_);

    $M = "SEEK/SET: seek(2,SET) + read(5) returns 'diner'";
    $GH->seek(2,0);
    $GH->read($BUF,5);
    $T->ok_eq($BUF, 'diner', 
	      $M);

    $M = "SEEK/END: seek(-6,END) + read(3) returns 'too'";
    $GH->seek(-6,2);
    $GH->read($BUF,3);
    $T->ok_eq($BUF, 'too', 
	      $M);

    $M = "SEEK/CUR: seek(-7,CUR) + read(7) returns 'one too'"; 
    $GH->seek(-7,1);
    $GH->read($BUF,7);
    $T->ok_eq($BUF, 'one too',
	      $M);
}

#------------------------------
# test_tie PARAMHASH
#------------------------------
# Test tiehandle getline() interface.
# 4
#
sub test_tie {
    my ($self, %p) = @_;
    my ($tieclass, @tieargs) = @{$p{TieArgs}};
    local($_);
    my @lines;
    my $i;
    my $nmatched;
    
    $M = "TIE/TIE: able to tie";
    tie(*OUT, $tieclass, @tieargs);
    $T->ok(1, $M,
	   TieClass => $tieclass,
	   TieArgs => \@tieargs);

    $M = "TIE/PRINT: printing data";
    print OUT @DATA_SA;
    print OUT $ADATA_SA[0];
    print OUT @ADATA_SA[1..2];
    $T->ok(1, $M);

    $M = "TIE/GETLINE: seek(0,0) and scalar <> get expected lines";
    tied(*OUT)->seek(0,0);                       # rewind
    @lines = (); push @lines, $_ while <OUT>;    # get lines one at a time
    $nmatched = 0;                               # total up matches...
    for (0..$#lines) { ++$nmatched if ($lines[$_] eq $FDATA_LA[$_]) };
    $T->ok(($nmatched == int(@FDATA_LA)), $M,
	   Want => \@FDATA_LA,
	   Gotl => \@lines,
	   Lines=> "0..$#lines",
	   Match=> $nmatched,
	   FDatl=> int(@FDATA_LA),
	   FData=> \@FDATA_LA);	  

    $M = "TIE/GETLINES: seek(0,0) and array <> slurps in lines";
    tied(*OUT)->seek(0,0);                       # rewind
    @lines = <OUT>;                              # get lines all at once
    $nmatched = 0;                               # total up matches...
    for (0..$#lines) { ++$nmatched if ($lines[$_] eq $FDATA_LA[$_]) };
    $T->ok(($nmatched == int(@FDATA_LA)), $M,
	   Want => \@FDATA_LA,
	   Gotl => \@lines,
	   Lines=> "0..$#lines",
	   Match=> $nmatched);

#    $M = "TIE/TELL: telling data";
#    my $tell_oo  = tied(*OUT)->tell;
#    my $tell_tie = tell OUT;
#    $T->ok(($tell_oo == $tell_tie), $M,
#	   Want => $tell_oo,
#	   Gotl => $tell_tie);

}

#------------------------------
# test_recordsep
#------------------------------
# Try $/ tests.
#
#    3 x undef
#    3 x empty
#    2 x custom
#   11 x newline
#
sub test_recordsep_count {
    my ($self, $seps) = @_;
    my $count = 0;
    $count += 3 if ($seps =~ /undef/) ;
    $count += 3 if ($seps =~ /empty/) ;
    $count += 2 if ($seps =~ /custom/) ;
    $count += 11 if ($seps =~ /newline/); 
    $count;
}
sub test_recordsep {
    my ($self, $seps, $opener) = @_;
    my $GH;
    my @lines = ("par 1, line 1\n",
		 "par 1, line 2\n",
		 "\n",
		 "\n",
		 "\n",
		 "\n",
		 "par 2, line 1\n",
		 "\n",
		 "par 3, line 1\n",
		 "par 3, line 2\n",
		 "par 3, line 3");
    my $all = join('', @lines);

    ### Slurp everything:
    if ($seps =~ /undef/) {
	$GH = &$opener(\@lines);
        local $/ = undef;
        $T->ok_eq($GH->getline, $all,
                  "RECORDSEP undef: getline slurps everything");
    }

    ### Read a little, slurp the rest:
    if ($seps =~ /undef/) {
	$GH = &$opener(\@lines);
        $T->ok_eq($GH->getline, $lines[0],
		  "RECORDSEP undef: get first line");
        local $/ = undef;
        $T->ok_eq($GH->getline, join('', @lines[1..$#lines]),
		  "RECORDSEP undef: slurp the rest");
    }

    ### Read paragraph by paragraph:
    if ($seps =~ /empty/) {
	$GH = &$opener(\@lines);
        local $/ = "";
        $T->ok_eq($GH->getline, join('', @lines[0..2]),
                  "RECORDSEP empty: first par");
        $T->ok_eq($GH->getline, join('', @lines[6..7]),
                  "RECORDSEP empty: second par");
        $T->ok_eq($GH->getline, join('', @lines[8..10]),
                  "RECORDSEP empty: third par");
    }

    ### Read record by record:
    if ($seps =~ /custom/) {
	$GH = &$opener(\@lines);
        local $/ = "1,";
        $T->ok_eq($GH->getline, "par 1,",
                  "RECORDSEP custom: first rec");
        $T->ok_eq($GH->getline, " line 1\npar 1,",
                  "RECORDSEP custom: second rec");
    }

    ### Read line by line:
    if ($seps =~ /newline/) {
	$GH = &$opener(\@lines);
        local $/ = "\n";
	for my $i (0..10) {
	    $T->ok_eq($GH->getline, $lines[$i],
		      "RECORDSEP newline: rec $i");
	}
    }

}

#------------------------------
1;


