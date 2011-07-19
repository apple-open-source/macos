
use strict;
$^W = 1; # enable warnings
my ($testnr, $maxnr, $oknr);

BEGIN { $testnr = 1; $maxnr = 4; print "$testnr..$maxnr\n"; }
sub ok ($) {
  if ($_[0]) {
    print "ok ", $testnr++, "\n";
    $oknr++;
  } else {
    print "not ok ", $testnr++, "\n";
    my ($package, $filename, $line) = caller;
    print "# Test failed at $filename line $line.\n";
  }
}

use blib;
use IO::Pty;
use IO::Tty qw(TIOCSCTTY TIOCNOTTY TCSETCTTY);
$IO::Tty::DEBUG = 1;
use POSIX ();

my $Perl = $^X;

print "Configuration: $IO::Tty::CONFIG\n";
print "Checking for appropriate ioctls: ";
print "TIOCNOTTY " if defined TIOCNOTTY;
print "TIOCSCTTY " if defined TIOCSCTTY;
print "TCSETCTTY " if defined TCSETCTTY;
print "\n";

print "Checking that returned fd's don't clash with stdin/out/err...\n";
{
  my $pid = fork();
  die "Cannot fork" if not defined $pid;
  unless ($pid) {
    # child closes stdin/out and reports test result via exit status
    sleep 0;
    close STDIN;
    close STDOUT;
    my $master = new IO::Pty;
    my $slave = $master->slave();
    if ($master->fileno < 3 or $slave->fileno < 3) {
      die 'ERROR: masterfd='.$master->fileno.', slavefd='.$slave->fileno."\n";
    }
    exit(0);
  }

  die "Wrong child" if wait != $pid;
  print "child exit=$?\n" if $?;
  ok($? == 0);
}


print "Checking if child gets pty as controlling terminal...\n";
{
  my $master = new IO::Pty;

  pipe(FROM_CHILD, TO_PARENT)
    or die "Cannot create pipe: $!";
  my $pid = fork();
  die "Cannot fork" if not defined $pid;
  unless ($pid) {
    # child
    sleep(1);
    $master->make_slave_controlling_terminal();
    my $slave = $master->slave();
    close $master;
    close FROM_CHILD;
    print TO_PARENT "\n";
    close TO_PARENT;
    open(TTY, "+>/dev/tty") or die "no controlling terminal";
    autoflush TTY 1;
    print TTY "gimme on /dev/tty: ";
    my $s = <TTY>;
    chomp $s;
    print $slave "back on STDOUT: \U$s\n";
    close TTY; close $slave;
    sleep(1);
    exit 0;
  }

  close TO_PARENT;
  $master->close_slave();
  my $dummy;
  my $stat = sysread(FROM_CHILD, $dummy, 1);
  die "Cannot sync with child: $!" if not $stat;
  close FROM_CHILD;

  my ($s, $chunk);
  $SIG{ALRM} = sub { ok(0); die "Timeout ($s)"; };
  alarm(10);

  sysread($master, $s, 100) or die "sysread() failed: $!";
  ok($s =~ m/gimme.*:/);

  print $master "seems OK!\n";

  # collect all responses
  my $ret;
  while ($ret = sysread($master, $chunk, 100)) {
    $s .= $chunk;
  }
  print $s;
  ok($s =~ m/back on STDOUT: SEEMS OK!/);
  warn <<"_EOT_" unless defined $ret;

WARNING: when the client closes the slave pty, the master gets an error
(undef return value and \$! eq "$!")
instead of EOF (0 return value).  Please be sure to handle this 
in your application (Expect already does).

_EOT_

  alarm(0);
  kill TERM => $pid;
}

# now for the echoback tests
print <<_EOT_;
Checking basic functionality and how your ptys handle large strings...
  This test may hang on certain systems, even though it is protected
  by alarm().  If the counter stops, try Ctrl-C, the test should continue.

_EOT_

{
  my $randstring = "fakjdf ijj845jtirg\r8e 4jy8 gfuoyhj\agt8h\0x00 gues98\0xFF 45th guoa\beh gt98hae 45t8u ha8rhg ue4ht 8eh tgo8he4 t8 gfj aoingf9a8hgf uain dgkjadshft+uehgf =usüand9ß87vgh afugh 8*h 98H 978H 7HG zG 86G (&g (O/g &(GF(/EG F78G F87SG F(/G F(/a sldjkf ha\@j<\rksdhf jk>~|ahsd fjkh asdHJKGDSG TRJKSGO  JGDSFJDFHJGSDK1%&FJGSDGFSH\0xADJäDGFljkhf lakjs(dh fkjahs djfk hasjkdh fjklahs dfkjhdjkf haöjksdh fkjah sdjf)\$/§&k hasÄÜÖjkdh fkjhuerhtuwe htui eruth ZI AHD BIZA Di7GH )/g98 9 97 86tr(& TA&(t 6t &T 75r 5\$R%/4r76 5&/% R79 5 )/&";
  my $master = new IO::Pty;
  print "isatty(\$master): ", POSIX::isatty($master)? "YES\n": "NO\n";
  if (POSIX::isatty($master)) {
    $master->set_raw() or
      warn "warning: \$master->set_raw(): $!";
  }

  pipe(FROM_CHILD, TO_PARENT)
    or die "Cannot create pipe: $!";
  my $pid = fork();
  die "Cannot fork" if not defined $pid;
  unless ($pid) {
    # child sends back everything inverted
    my $c;
    my $slave = $master->slave();
    close $master;
    print "isatty(\$slave): ", POSIX::isatty($slave)? "YES\n": "NO\n";
    $slave->set_raw() or
      warn "warning: \$slave->set_raw(): $!";
    close FROM_CHILD;
    print TO_PARENT "\n";
    close TO_PARENT;
    my $cnt = 0;
    my $linecnt = 0;
    while(1) { 
      my $ret = sysread($slave, $c, 1);
      warn "sysread(): $!" unless defined $ret;
      die "Slave got EOF at line $linecnt, byte $cnt.\n" unless $ret;
      $cnt++;
      if ($c eq "\n") {
	$linecnt++;
	$cnt = 0;
      } else {
	$c = ~$c;
      }
      $ret = syswrite($slave, $c, 1);
      warn "syswrite(): $!" unless defined $ret;
    }
  }
  close TO_PARENT;
  $master->close_slave();
  my $dummy;
  my $stat = sysread(FROM_CHILD, $dummy, 1);
  die "Cannot sync with child: $!" if not $stat;
  close FROM_CHILD;

  print "Child PID = $pid\n";

  # parent sends down some strings and expects to get them back inverted
  my $maxlen = 0;
  foreach my $len (1 .. length($randstring)) {
    print STDERR "$len\r";
    my $s = substr($randstring, 0, $len);
    my $buf;
    my $ret = "";
    my $inv = ~$s."\n";
    $s .= "\n";
    my $sendbuf = $s;
    $SIG{ALRM} = $SIG{TERM} = $SIG{INT} = sub {die "TIMEOUT(SIG".shift().")";};
    eval {
      alarm(15);
      while ($sendbuf or length($ret) < length($s)) {
	if ($sendbuf) {
	  my $sent = syswrite($master, $sendbuf, length($sendbuf));
	  die "syswrite() failed: $!" unless defined $sent;
	  $sendbuf = substr($sendbuf, $sent);
	}
	$buf = "";
	my $read = sysread($master, $buf, length($s));
	die "Couldn't read from child: $!" if not $read;
	$ret .= $buf;
      }
      alarm(0);
    };
    if ($@) {
      warn $@;
      last;
    }

    if ($ret eq $inv) {
      $maxlen = $len;
    } else {
      if (length($s) == length($ret)) {
	warn "Got back a wrong string with the right length ".length($ret)."\n";
      } else {
	warn "Got back a wrong string with the wrong length ".length($ret).
	  " (instead of ".length($s).")\n";
      }
      ok(0);
      last;
    }
  }
  $SIG{ALRM} = $SIG{TERM} = $SIG{INT} = 'DEFAULT';
  if ($maxlen < length($randstring)) {
    warn <<"_EOT_";

WARNING: your raw ptys block when sending more than $maxlen bytes!
This may cause problems under special scenarios, but you probably
will never encounter that problem.

_EOT_
  } else {
    warn "Good, your raw ptys can handle at least $maxlen bytes at once.\n";
  }
  ok($maxlen >= 200);
  close($master);
  sleep(1);
  kill TERM => $pid;
}

print "Passed $oknr of $maxnr tests.\n";


