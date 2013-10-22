#!perl

use Term::ReadPassword;

if ($ENV{AUTOMATED_TESTING}) {
    print "1..0 # Skip: Automated testing detected (AUTOMATED_TESTING) \n";
    exit;
}

$Term::ReadPassword::USE_STARS = $ENV{USE_STARS};

print "1..1\n";

# Let's open the TTY (rather than STDOUT) if we can
# local(*TTY, *TTYOUT);
my($in, $out) = Term::ReadLine->findConsole;
die "No console available" unless $out;

if (open TTYOUT, ">>$out") {
    # Cool
} else {
    # Well, let's allow STDOUT instead
    open TTYOUT, ">>&STDOUT"
	or die "Can't re-open STDOUT: $!";
}

# Don't buffer it!
select( (select(TTYOUT), $|=1)[0] );

# Well, this would be hard to test unless I set up a ptty and sockets and
# my head hurts....
INTERACTIVE: {
  my $secret = '';
  { 
    # Naked block for scoping and redo
    print TTYOUT "\n\n# (Don't worry - you're not changing any real password!)\n";
    my $new_pw = read_password("Enter your (fake) new password: ", 20);
    if (not defined $new_pw) {
      print TTYOUT "# Time's up!\n";
      print TTYOUT "# Were you scared, or are you merely an automated test?\n";
      print "ok 1\n";
      last INTERACTIVE;
    } elsif ($new_pw eq '') {
      print TTYOUT "# No empty passwords allowed.\n";
      print TTYOUT "# (Use the password ' ' (a space character) to skip this test.)\n";
      redo;
    } elsif ($new_pw =~ /^ +$/) {
      print TTYOUT "# Skipping the test!\n";
      print "ok 1\n";
      last INTERACTIVE;
    } elsif ($new_pw =~ /([^\x20-\x7E])/) {
      my $bad = unpack "H*", $1;
      print TTYOUT "# Your (fake) password may not contain the ";
      print TTYOUT "evil character with hex code $bad.\n";
      redo;
    } elsif (length($new_pw) < 3) {
      print TTYOUT "# Your (fake) password must be longer than that!\n";
      redo;
    } elsif ($new_pw ne read_password("Enter it again: ")) {
      print TTYOUT "# Passwords don't match.\n";
      redo;
    } else {
      $secret = $new_pw;
      print TTYOUT "# Your (fake) password is now changed.\n";
    }
  }

  print TTYOUT "# \n# Time passes... you come back the next day... and you see...\n";
  while (1) {
    my $password = read_password('password: ');
    redo unless defined $password;
    if ($password eq $secret) {
      print TTYOUT "# Access granted.\n";
      print "ok 1\n";
      last;
    } else {
      print TTYOUT "# Access denied.\n";
      print TTYOUT "# (But I'll tell you: The password is '$secret'.)\n";
      redo;
    }
  }
}
