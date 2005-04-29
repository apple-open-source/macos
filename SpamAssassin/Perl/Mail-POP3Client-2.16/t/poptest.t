# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl t/poptest.t'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; $tests=26; print "1..$tests\n"; }
END {print "not ok 1\n" unless $loaded;}
use Mail::POP3Client;
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

# Insert your test code below (better if it prints "ok 13"
# (correspondingly "not ok 13") depending on the success of chunk 13
# of the test code):

my $skip = 0;

# additional tests require a pop account to test against
# set POPTESTACCOUNT in environment.  Format is user:password:host
#  % POPTESTACCOUNT="userid:password:pop3" make test
$ENV{POPTESTACCOUNT} || do {
  print STDERR "\nTests 2-$tests skipped, try setting POPTESTACCOUNT=user:pass:host in environment.\n";
  for ( $test = 2; $test <= $tests; $test++ ) {print "ok $test\n";}
  $skip = 1;
};

($user, $pass, $host) = split( /:/, $ENV{POPTESTACCOUNT} );
$user ||= "*";
$pass ||= "*";
$host ||= "localhost";
($user && $pass && $host ) || do {
  if ( ! $skip ) {
    print STDERR "\nTests 2-$tests skipped, POPTESTACCOUNT=user:pass:host must use valid combination.\n";
    for ( $test = 2; $test <= $tests; $test++ ) {print "ok $test\n";}
    $skip = 1;
  }
};

($user && $pass && $host ) && do {
  if ( ! $skip ) {
    $user =~ /\*/ || $pass =~ /\*/ && print STDERR "Using default user or password, expect failures!";
    
    my $test = 2;
    
    # recommended style - autoconnects
    my $pop = new Mail::POP3Client( PASSWORD => $pass,
				    HOST => $host,
				    USER => $user,
				  );
    
    $pop->Alive() || print "not ";
    print "ok ", $test++, "\n";
    
    # test some of the methods (we won't test a Delete)
    $pop->POPStat() >= 0 || print "not ";
    print "ok ", $test++, "\n";
    
    (my $count = $pop->Count()) >= 0 || print "not ";
    print "ok ", $test++, "\n";
    
    $pop->Size() >= 0 || print "not ";
    print "ok ", $test++, "\n";
    
    if ( $count > 0 )
      {
	my @array = $pop->List() || print "not ";
      }
    print "ok ", $test++, "\n";
    
    if ( $count > 0 )
      {
	$pop->Head( 1 ) || print "not ";
      }
    print "ok ", $test++, "\n";
    
    if ( $count > 0 )
      {
	$pop->HeadAndBody( 1 ) || print "not ";
      }
    print "ok ", $test++, "\n";
    
    if ( $count > 0 )
      {
	$pop->Body( 1 ) || print "not ";
      }
    print "ok ", $test++, "\n";
    
    
    $pop->Close() || print "not ";
    print "ok ", $test++, "\n";
    
    
    # do each step by hand
    my $pop2 = new Mail::POP3Client( HOST => $host ) || print "not ";
    print "ok ", $test++, "\n";
    $pop2->User( $user );
    $pop2->Pass( $pass );
    $pop2->Connect() and $pop2->POPStat() || print "not ";
    print "ok ", $test++, "\n";
    
    $pop2->Close() || print "not ";
    print "ok ", $test++, "\n";
    
    
    # test old positional-style constructors
    my $pop3 = new Mail::POP3Client( $user, $pass, $host ) || print "not ";
    print "ok ", $test++, "\n";
    
    $pop3->Close() || print "not ";
    print "ok ", $test++, "\n";
    
    my $pop4 = new Mail::POP3Client( $user, $pass, $host, 110 ) || print "not ";
    print "ok ", $test++, "\n";
    $pop4->Close() || print "not ";
    print "ok ", $test++, "\n";
    
    my $pop5 = new Mail::POP3Client( $user, $pass, $host, 110, 0 ) || print "not ";
    print "ok ", $test++, "\n";
    $pop5->Close() || print "not ";
    print "ok ", $test++, "\n";
    
    my $pop6 = new Mail::POP3Client( $user, $pass, $host, 110, 0, 'APOP' ) || print "not ";
    print "ok ", $test++, "\n";
    $pop6->Close() || print "not ";
    print "ok ", $test++, "\n";
    
    my $pop7 = new Mail::POP3Client( $user, $pass, $host, 110, 0, 'PASS' ) || print "not ";
    print "ok ", $test++, "\n";
    $pop7->Close() || print "not ";
    print "ok ", $test++, "\n";
    
    
    # 2 concurrent connections - server may barf on this
    my $pop8 = new Mail::POP3Client( PASSWORD => $pass,
				     HOST => $host,
				     USER => $user,
				   );
    my $pop9 = new Mail::POP3Client( PASSWORD => $pass,
				     HOST => $host,
				     USER => $user,
				   );
    
    $pop8->Alive() && $pop9->Alive() || print "not ";
    print "ok ", $test++, "\n";
    
    $pop8->Close() || print "not ";
    print "ok ", $test++, "\n";
    $pop9->Close() || print "not ";
    print "ok ", $test++, "\n";
  }
};

