# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

$number_of_tests = 83;

BEGIN { $| = 1; print "1..$number_of_tests\n"; }
END {print "not ok 1\n" unless $loaded;}

use Unix::Syslog qw(:macros :subs);

$loaded = 1;
print "ok 1\n";

######################### End of black magic.

# Insert your test code below (better if it prints "ok 13"
# (correspondingly "not ok 13") depending on the success of chunk 13
# of the test code):


# print result from last eval expression
sub print_result($) {
  if (shift) { print " not ok"; return 1 }
  else { print " ok"; return 0 }
}

my $n = 2;
my $failures = 0;

print "Testing priorities:\n";
printf "%-20s", 'LOG_EMERG';   eval { LOG_EMERG() };   $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_ALERT';   eval { LOG_ALERT() };   $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_CRIT';    eval { LOG_CRIT() };    $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_ERR';     eval { LOG_ERR() };     $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_WARNING'; eval { LOG_WARNING() }; $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_NOTICE';  eval { LOG_NOTICE() };  $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_INFO';    eval { LOG_INFO() };    $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_DEBUG';   eval { LOG_DEBUG() };   $failures += print_result($@); print " $n\n"; $n++;

print "\nTesting facilities\n";
$fac = 0;
printf "%-20s", 'LOG_KERN';   eval { LOG_KERN() };     $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_USER';   eval { LOG_USER() };     $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_MAIL';   eval { LOG_MAIL() };     $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_DAEMON'; eval { LOG_DAEMON() };   $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_AUTH';   eval { LOG_AUTH() };     $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_SYSLOG'; eval { LOG_SYSLOG() };   $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_LPR';    eval { LOG_LPR() };      $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_NEWS';   eval { LOG_NEWS() };     $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_UUCP';   eval { LOG_UUCP() };     $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_CRON';   eval { LOG_CRON() };     $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_LOCAL0'; eval { LOG_LOCAL0() };   $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_LOCAL1'; eval { LOG_LOCAL1() };   $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_LOCAL2'; eval { LOG_LOCAL2() };   $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_LOCAL3'; eval { LOG_LOCAL3() };   $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_LOCAL4'; eval { LOG_LOCAL4() };   $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_LOCAL5'; eval { LOG_LOCAL5() };   $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_LOCAL6'; eval { LOG_LOCAL6() };   $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_LOCAL7'; eval { LOG_LOCAL7() };   $failures += print_result($@); print " $n\n"; $n++;

print "\nThese facilities are not defined on all systems:\n";
printf "%-20s", 'LOG_AUTHPRIV'; eval { LOG_AUTHPRIV() }; $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_FTP';      eval { LOG_FTP() };      $failures += print_result($@); print " $n\n"; $n++;

print "\nThe number of available facilities is:\n";
printf "%-20s", 'LOG_NFACILITIES '; eval { LOG_NFACILITIES() }; $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_FACMASK';      eval { LOG_FACMASK };       $failures += print_result($@); print " $n\n"; $n++;

print "\nTesting options\n";
printf "%-20s", 'LOG_PID';    eval { LOG_PID() };    $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_CONS';   eval { LOG_CONS() };   $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_ODELAY'; eval { LOG_ODELAY() }; $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_NDELAY'; eval { LOG_NDELAY() }; $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_NOWAIT'; eval { LOG_NOWAIT() }; $failures += print_result($@); print " $n\n"; $n++;

print "\nThese options are not defined on all systems:\n";
printf "%-20s", 'LOG_PERROR'; eval{ LOG_PERROR() };  $failures += print_result($@); print " $n\n"; $n++;

print "\nTesting macros for setlogmask()\n";
printf "%-20s", 'LOG_MASK'; eval { LOG_MASK(1) };    $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_UPTO'; eval { LOG_UPTO(1) };    $failures += print_result($@); print " $n\n"; $n++;

print "\nThese macros are not defined on all systems:\n";
printf "%-20s", 'LOG_PRI';     eval { LOG_PRI(1) };       $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_MAKEPRI'; eval { LOG_MAKEPRI(1,1) }; $failures += print_result($@); print " $n\n"; $n++;
printf "%-20s", 'LOG_FAC';     eval { LOG_FAC(1) };       $failures += print_result($@); print " $n\n"; $n++;

print "\nOn some systems these functions return just empty strings:\n";
foreach $p qw(LOG_EMERG LOG_ALERT LOG_CRIT LOG_ERR LOG_WARNING LOG_NOTICE LOG_INFO
	      LOG_DEBUG) {
  my $pnum = &$p;
  printf "%-30s", "priorityname($p):";
  unless (defined(priorityname($pnum))) {
    print "not defined. skipped $n\n"; $n++;
    next;
  }
  eval qq{ printf "%-8s", priorityname($pnum) }; $failures += print_result($@); print " $n\n"; $n++;
}

print "\n";

foreach $f qw(LOG_KERN LOG_USER LOG_MAIL LOG_DAEMON LOG_AUTH LOG_SYSLOG LOG_LPR
	      LOG_NEWS LOG_UUCP LOG_CRON LOG_AUTHPRIV LOG_FTP LOG_LOCAL0 LOG_LOCAL1
	      LOG_LOCAL2 LOG_LOCAL3 LOG_LOCAL4 LOG_LOCAL5 LOG_LOCAL6 LOG_LOCAL7) {
  my $fnum = &$f;
  printf "%-30s", "facilityname($f):";
  unless (defined(facilityname($fnum))) {
    print "not defined. skipped $n\n"; $n++;
    next;
  }
  eval qq{ printf "%-8s", facilityname($fnum) }; $failures += print_result($@); print " $n\n"; $n++;
}

print "\nTesting setlogmask:\n";
print "Setting mask to ", LOG_MASK(LOG_INFO), "\n";
$oldmask = setlogmask(LOG_MASK(LOG_INFO));
$newmask = setlogmask($oldmask);
print "New mask is     $newmask  ";
  $failures += print_result($newmask!=LOG_MASK(LOG_INFO)); print " $n\n"; $n++;

if ($failures == 0) {
  print "\n*** Congratulations! All tests passed.\n\n";
}
else {
  print "\n*** Test results: ", $n-1-$failures, " tests of ", $n-1, " passed!\n\n";
}

print <<EOM;
Testing functions

As the functions `openlog', `closelog' and `syslog' do not return any
value that indicates success or failure, please have a look at the log
file generated by syslogd(8).  The following tests print a message of
the facility `local7' and the priority `info'.

EOM

sub basename {
  my $name = shift;
  $name =~ s@.*/@@;
  return $name;
}

print "openlog\n";
openlog(basename($0), LOG_PID, LOG_LOCAL7);

print "syslog\n";
syslog(LOG_INFO, "Unix::Syslog testsuite: The ident string should be \`%s\' (Test %d)", basename($0), $n++);
syslog(LOG_INFO, "Unix::Syslog testsuite: Testing quote character \`%%\' (Test %d)", $n++);
syslog(LOG_INFO, "Unix::Syslog testsuite: This message prints an error message: %m (Test %d)", $n++);
syslog(LOG_INFO, "Unix::Syslog testsuite: This message prints a percent sign followed by the character \`m\': %%m (Test %d)", $n++);
syslog(LOG_INFO, "Unix::Syslog testsuite: This message prints a percent sign followed by an error message: %%%m (Test %d)", $n++);

syslog(LOG_INFO, "Unix::Syslog testsuite: This message prints a percent sign followed by the character \`m\': %s (Test %d)", '%m', $n++);
syslog(LOG_INFO, "Unix::Syslog testsuite: This message prints two percent signs followed by the character \`m\': %s (Test %d)", '%%m', $n++);
syslog(LOG_INFO, "Unix::Syslog testsuite: This message prints three percent signs followed by the character \`m\': %s (Test %d)", '%%%m', $n++);

print "setlogmask\n";
setlogmask(LOG_MASK(LOG_INFO));

print "syslog\n";
syslog(LOG_INFO,  "Unix::Syslog testsuite: (LOG_MASK) This message should be visible (Test %d)\n", $n++);
syslog(LOG_EMERG, "Unix::Syslog testsuite: (LOG_MASK) This message should NOT be visible (Test %d)\n", $n++);

setlogmask(LOG_UPTO(LOG_INFO));
syslog(LOG_INFO,    "Unix::Syslog testsuite: (LOG_UPTO) This message should be visible (Test %d)\n", $n++);
syslog(LOG_EMERG,   "Unix::Syslog testsuite: (LOG_UPTO) This message should be visible (Test %d)\n", $n++);
syslog((LOG_INFO()+1), "Unix::Syslog testsuite: (LOG_UPTO) This message should NOT be visible (Test %d)\n", $n++);

print "closelog\n\n";
closelog;

print "Testing pointer handling in closelog().\n";
print "Calling closelog() a second time.\n";
closelog;
print "This message should not be preceeded by an error message\n";
print "   about dereferenced pointers.\n\n";
