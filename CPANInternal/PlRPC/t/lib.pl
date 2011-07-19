require IO::Socket;
require Config;
require Net::Daemon::Test;
require RPC::PlClient;


sub Test($) {
    my $result = shift;
    printf("%sok %d\n", ($result ? "" : "not "), ++$numTest);
    $result;
}

sub RunTests (@) {
    my $client;
    my $key;

    if ($_[0]  &&  $_[0] eq 'usercipher') {
	shift;
	$key = shift;
    }

    # Making a new connection
    Test($client = eval { RPC::PlClient->new(@_) })
	or print "Failed to make second connection: $@\n";
    if ($key) { $client->{'cipher'} = $key }

    # Creating a calculator object
    my $calculator = eval { $client->ClientObject('Calculator', 'new') };
    Test($calculator) or print "Failed to create calculator: $@\n";
    print "Calculator is $calculator.\n";
    print "Handle is $calculator->{'object'}.\n";
    print "Client is $calculator->{'client'}.\n";

    # Let him do calculations ...
    my $result = eval { $calculator->add(4, 6, 7) };
    Test($result and $result eq 17)
	or printf("Expected 17, got %s, errstr $@\n",
		  (defined($result) ? $result : "undef"));

    $result = eval { $calculator->multiply(2, 3, 4) };
    Test($result and $result eq 24);

    $result = eval { $calculator->subtract(27, 12) };
    Test($result and $result eq 15);

    $result = eval { $calculator->subtract(27, 12, 7) };
    Test($@ and $@ =~ /Usage/);

    $result = eval { $calculator->divide(15, 3) };
    Test($result and $result eq 5);

    $result = eval { $calculator->divide(27, 12, 7) };
    Test($@ and $@ =~ /Usage/);

    $result = eval { $calculator->divide(27, 0) };
    Test($@ and $@ =~ /zero/);

    ($client, $calculator);
}


1;
