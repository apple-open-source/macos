
use strict;
$^W = 1;			# warnings too

my ($testnr, $maxnr, $oknr);

BEGIN { $testnr = 1; $maxnr = 42; print "$testnr..$maxnr\n"; }
sub ok ($) {
  if ($_[0]) {
    print "ok ", $testnr++, "\n";
    $oknr++;
    return 1;
  } else {
    print "not ok ", $testnr++, "\n";
    my ($package, $filename, $line) = caller;
    print "# Test failed at $filename line $line.\n";
    return undef;
  }
}

sub fatal($) {
  ok(shift) or die;
}

my $Perl = $^X;

use Expect;
#$Expect::Exp_Internal = 1;
#$Expect::Debug = 1;

print "\nBasic tests...\n\n";

{
  my $exp = Expect->spawn("$Perl -v");
  fatal(defined $exp);
  $exp->log_user(0);
  fatal($exp->expect(10, "krzlbrtz", "Copyright") == 2);
  fatal($exp->expect(10, "Larry Wall", "krzlbrtz") == 1);
  fatal(not $exp->expect(3, "Copyright"));
}

print "\nTesting exec failure...\n\n";

{
  my $exp = new Expect;
  ok(defined $exp);
  $exp->log_stdout(0);
  $! = 0;
  fatal(not defined $exp->spawn("Ignore_This_Error_Its_A_Test__efluna3w6868tn8"));
  ok($!);
  my $res = $exp->expect(20,
			 [ "Cannot exec" => sub{ ok(1); }],
			 [ eof => sub{ print "EOF\n"; ok(1) }],
			 [ timeout => sub{ print "TIMEOUT\n"; ok(0) }],
			);
#  ok(defined $res and $res == 1);
}

print "\nTesting exp_continue...\n\n";

{
  my $exp = new Expect($Perl . q{ -e 'foreach (qw(A B C D End)) { print "$_\n"; }' });
  my $state = "A";
  $exp->expect(2,
	       [ "[ABCD]" => sub { my $self = shift;
				   ok($self->match eq $state);
				   $state++;
				   exp_continue;
				 } ],
	       [ "End" => sub { ok($state eq "E"); } ],
               [ eof => sub { print "EOF\n"; ok(0); } ],
               [ timeout => sub { print "TIMEOUT\n"; ok(0);} ],
              );
  $exp->hard_close();
}

{
  my $exp = new Expect($Perl . q{ -e 'print "Begin\n"; sleep (5); print "End\n";' });
  my $cnt = 0;
  $exp->expect(1,
	       [ "Begin" => sub { ok(1); exp_continue; } ],
	       [ "End" => sub { ok(1); } ],
               [ eof => sub { print "EOF\n"; ok(0); } ],
               [ timeout => sub { $cnt++; ($cnt < 7)? exp_continue : 0;} ],
              );
  ok($cnt > 2 and $cnt < 7);
  $exp->hard_close();
}

{
  # timeout shouldn't destroy accum contents
  my $exp = new Expect($Perl . q{ -e 'print "some string\n"; sleep (5);' });
  ok(not defined $exp->expect(1, "NoMaTcH"));
  my $i = $exp->expect(1, '-re', 'some\s');
  ok (defined $i and $i == 1);
  $exp->hard_close();
}

print "\nTesting -notransfer...\n\n";

{
  my $exp = new Expect($Perl . q{ -e 'print "X some other\n"; sleep 5;'});
  $exp->notransfer(1);
  $exp->expect(3,
	       [ "some" => sub { ok(1); } ],
               [ eof => sub { print "EOF\n"; ok(0); } ],
               [ timeout => sub { print "TIMEOUT\n"; ok(0);} ],
              );
  $exp->expect(3,
	       [ "some" => sub { ok(1); } ],
               [ eof => sub { print "EOF\n"; ok(0); } ],
               [ timeout => sub { print "TIMEOUT\n"; ok(0);} ],
              );
  $exp->expect(3,
	       [ "other" => sub { ok(1); } ],
               [ eof => sub { print "EOF\n"; ok(0); } ],
               [ timeout => sub { print "TIMEOUT\n"; ok(0);} ],
              );
  sleep(6);
  $exp->expect(3,
	       [ "some" => sub { my $self = shift; ok(1); $self->set_accum($self->after()); } ],
               [ eof => sub { print "EOF\n"; ok(0); } ],
               [ timeout => sub { print "TIMEOUT\n"; ok(0);} ],
              );
  $exp->expect(3,
	       [ "some" => sub { ok(0); } ],
	       [ "other" => sub { my $self = shift; ok(1); $self->set_accum($self->after()); } ],
               [ eof => sub { print "EOF\n"; ok(0); } ],
               [ timeout => sub { print "TIMEOUT\n"; ok(0);} ],
              );
  $exp->expect(3,
	       [ "some" => sub { ok(0); } ],
	       [ "other" => sub { ok(0); } ],
               [ eof => sub { print "EOF\n"; ok(1); } ],
               [ timeout => sub { print "TIMEOUT\n"; ok(0);} ],
              );
}

print "\nTesting raw reversing...\n\n";

{
  my @Strings =
    (
     "The quick brown fox jumped over the lazy dog.",
     "Ein Neger mit Gazelle zagt im Regen nie",
     "Was ich brauche ist ein Lagertonnennotregal",
    );

  my $exp = new Expect;
#  my $exp = new Expect ("$Perl -MIO::File -ne 'BEGIN {\$|=1; \$in = new IO::File \">reverse.in\" or die; \$in->autoflush(1); \$out = new IO::File \">reverse.out\" or die; \$out->autoflush(1); } chomp; print \$in \"\$_\\n\"; \$_ = scalar reverse; print \"\$_\\n\"; print \$out \"\$_\\n\"; '");


  print "isatty(\$exp): ";
  if (POSIX::isatty($exp)) {
    print "YES\n";
  } else {
    print "NO\n";
  }

  $exp->raw_pty(1);

  $exp->spawn("$Perl -ne 'chomp; sleep 0; print scalar reverse, \"\\n\"'")
    or die "Cannot spawn $Perl: $!\n";
  my $called = 0;
  $exp->log_file(sub { $called++; });
  foreach my $s (@Strings) {
    my $rev = scalar reverse $s;
    $exp->send("$s\n");
    $exp->expect(10,
		 [ quotemeta($rev) => sub { ok(1); }],
		 [ timeout => sub { ok(0); die "Timeout"; } ],
		 [ eof => sub { ok(0); die "EOF"; } ],
		);
  }
  ok($called >= @Strings);
  $exp->log_file(undef);

  # now with send_slow
  $called = 0;
  $exp->log_file(sub { $called++; });
  my $delay = 0.1;
  foreach my $s (@Strings) {
    my $rev = scalar reverse $s;
    my $now = time;
    $exp->send_slow($delay, "$s\n");
    $exp->expect(10,
		 [ quotemeta($rev) => sub { ok(1); }],
		 [ timeout => sub { ok(0); die "Timeout"; } ],
		 [ eof => sub { ok(0); die "EOF"; } ],
		);
    my $dur = time - $now;
    ok($dur > length($s) * $delay);
  }
  ok($called >= @Strings);
  $exp->log_file(undef);


  print <<_EOT_;

------------------------------------------------------------------------------
>  The following tests check system-dependend behaviour, so even if some fail,
>  Expect might still be perfectly usable for you!
------------------------------------------------------------------------------
_EOT_

# we check if the raw pty can handle large chunks of text at once

  my $randstring = 'fakjdf ijj845jtirg8e 4jy8 gfuoyhjgt8h gues9845th guoaeh gt98hae 45t8u ha8rhg ue4ht 8eh tgo8he4 t8 gfj aoingf9a8hgf uain dgkjadshftuehgfusand987vgh afugh 8h 98H 978H 7HG zG 86G (&g (O/g &(GF(/EG F78G F87SG F(/G F(/a sldjkf hajksdhf jkahsd fjkh asdHJKGDSGFKLZSTRJKSGOSJDFKGHSHGDFJGDSFJKHGSDFHJGSDKFJGSDGFSHJDGFljkhf lakjsdh fkjahs djfk hasjkdh fjklahs dfkjhasdjkf hajksdh fkjah sdjfk hasjkdh fkjashd fjkha sdjkfhehurthuerhtuwe htui eruth ZI AHD BIZA Di7GH )/g98 9 97 86tr(& TA&(t 6t &T 75r 5$R%/4r76 5&/% R79 5 )/&';
  my $maxlen;
  $exp->log_stdout(0);
  $exp->log_file("test.log");
  my $exitloop;
  $SIG{ALRM} = sub { die "TIMEOUT on send" };

  foreach my $len (1 .. length($randstring)) {
    print "$len\r";
    my $s = substr($randstring, 0, $len);
    my $rev = scalar reverse $s;
    eval {
      alarm(10);
      $exp->send("$s\n");
      alarm(0);
    };
    if ($@) {
      ok($maxlen > 80);
      print "Warning: your raw pty blocks when sending more than $maxlen bytes!\n";
      $exitloop = 1;
      last;
    }
    $exp->expect(10,
		 [ quotemeta($rev) => sub {$maxlen = $len; }],
		 [ timeout => sub { ok($maxlen > 160);
				    print "Warning: your raw pty can only handle $maxlen bytes at a time!\n" ;
				    $exitloop = 1; } ],
		 [ eof => sub { ok(0); die "EOF"; } ],
		);
    last if $exitloop;
  }
  $exp->log_file(undef);
  print "Good, your raw pty can handle at least ".length($randstring)." bytes at a time.\n" if not $exitloop;
  ok($maxlen > 160);
}

# Now test for the max. line length. Some systems are limited to ~255
# chars per line, after which they start loosing characters.  As Cygwin 
# then hangs and cannot be freed via alarm, we only test up to 160 characters
# to avoid that.

{
  my $exp = new Expect ("$Perl -ne 'chomp; sleep 0; print scalar reverse, \"\\n\"'")
    or die "Cannot spawn $Perl: $!\n";

  $exp->log_stdout(0);
  my $randstring = 'Fakjdf ijj845jtirg8 gfuoyhjgt8h gues9845th guoaeh gt9vgh afugh 8h 98H 97BH 7HG zG 86G (&g (O/g &(GF(/EG F78G F87SG F(/G F(/a slkf ksdheq@f jkahsd fjkh%&/"§ä#üßw';
  my $maxlen;
  my $exitloop;
  foreach my $len (1 .. length($randstring)) {
    print "$len\r";
    my $s = substr($randstring, 0, $len);
    my $rev = scalar reverse $s;
    eval {
      alarm(10);
      $exp->send("$s\n");
      alarm(0);
    };
    if ($@) {
      ok($maxlen > 80);
      print "Warning: your default pty blocks when sending more than $maxlen bytes per line!\n";
      $exitloop = 1;
      last;
    }
    $exp->expect(10,
		 [ quotemeta($rev) => sub {$maxlen = $len; }],
		 [ timeout => sub { print "Warning: your default pty can only handle $maxlen bytes at a time!\n" ;
				    $exitloop = 1; } ],
		 [ eof => sub { ok(0); die "EOF"; } ],
		);
  }
  print "Good, your default pty can handle lines of at least ".length($randstring)." bytes at a time.\n" if not $exitloop;
  ok($maxlen > 100);
}

{
  print "\nTesting controlling terminal...\n\n";
  my $exp = new Expect($Perl . q{ -MIO::Handle -e 'open(TTY, "+>/dev/tty") or die "no controlling terminal"; autoflush TTY 1; print TTY "Expect_test_prompt: "; $s = <TTY>; chomp $s; print "uc: \U$s\n"; close TTY; exit 0;'});

  my $pwd = "pAsswOrd";
  $exp->log_file("test_dev_tty.log");
  $exp->expect(10,
	       [ qr/Expect_test_prompt:/, sub {
		   my $self = shift;
		   $self->send("$pwd\n");
		   $exp->log_file(undef);
		   exp_continue;
		 } ],
	       [ qr/(?m:^uc:\s*(\w+))/, sub {
		   my $self = shift;
		   my ($s) = $self->matchlist;
		   chomp $s;
		   print "match: $s\n";
		   ok($s eq uc($pwd));
		 } ],
	       [ eof => sub {
		   ok(0); die "EOF";
		 } ],
	       [ timeout => sub {
		   ok(0); die "Timeout";
		 } ],
	      );
}

print "\nChecking if exit status is returned correctly...\n\n";

{
  my $exp = new Expect($Perl . q{ -e 'print "Expect_test_pid: $$\n"; sleep 2; exit(42);'});
  $exp->expect(10,
               [ qr/Expect_test_pid:/, sub { my $self = shift; } ],
               [ eof => sub { print "eof\n"; } ],
               [ timeout => sub { print "timeout\n";} ],
              );
  my $status = $exp->soft_close();
  printf "soft_close: 0x%04X\n", $status;
  ok($exp->exitstatus() == $status);
  ok((($status >> 8) & 0x7F) == 42);
}

print "\nChecking if signal exit status is returned correctly...\n\n";

{
  my $exp = new Expect($Perl . q{ -e 'print "Expect_test_pid: $$\n"; sleep 2; kill 15, $$;'});
  $exp->expect(10,
               [ qr/Expect_test_pid:/, sub { my $self = shift; } ],
               [ eof => sub { print "eof\n"; } ],
               [ timeout => sub { print "timeout\n";} ],
              );
  my $status = $exp->soft_close();
  printf "soft_close: 0x%04X\n", $status;
  ok($exp->exitstatus() == $status);
  my ($hi, $lo) = (($status >> 8) & 0x7F, $status & 0x7F);

  ok($hi == 15 or $lo == 15);
}

print <<__EOT__;

Checking if EOF on pty slave is correctly reported to master...
(this fails on about 50% of the supported systems, so don't panic!
 Expect will work anyway!)

__EOT__

{
  my $exp = new Expect($Perl . q{ -e 'close STDIN; close STDOUT; close STDERR; sleep 3;'});
  $exp->expect(2,
               [ eof => sub { print "EOF\n"; } ],
               [ timeout => sub { print "TIMEOUT\nSorry, you may not notice if the spawned process closes the pty.\n"; } ],
              );
  $exp->hard_close();
}

print "Passed $oknr of $maxnr tests.\n";
print <<__EOT__ if ($oknr != $maxnr);
Please scroll back and check which test(s) failed and what comments
were given.  Expect probably is still completely usable!!
__EOT__

exit(0);
