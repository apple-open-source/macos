use strict;

use Test::More tests => 4;

BEGIN {
    # already tested, but no harm done
    use_ok( 'Net::SSL' );
}

my $url = 'https://rt.cpan.org/';

my @prereq;

eval q{ use LWP::UserAgent };
push @prereq, "LWP::UserAgent" if $@;

eval q{ use HTTP::Request };
push @prereq, "HTTP::Request" if $@;

my $network_tests;
if (open IN, '<test.config') {
    diag("config on $^O");
    while (<IN>) {
        chomp;
        if (my ($key, $value) = ($_ =~ /\A(\S+)\s+(.*)/)) {
            if ($key eq 'network_tests') {
                $network_tests = $value;
            }
            elsif (grep {$key eq $_} qw(cc inc lib ssl)) {
                diag("$key $value");
            }
        }
    }
    close IN;
}

my $PROXY_ADDR_PORT = 'localhost:3128';

sub live_connect {
    my $hr = shift;
    local $ENV{HTTPS_PROXY} = $PROXY_ADDR_PORT;

    # always true if we've been instructed to skip the attempt
    return 1 unless $network_tests;

    my $sock = Net::SSL->new(
        PeerAddr => 'rt.cpan.org',
        PeerPort => 443,
        Timeout  => 10,
    );

    return defined($sock) ? 1 : 0;
    # $sock will be garbage collected and the connection torn down
}

my $test_name = 'connect through proxy';
Net::SSL::send_useragent_to_proxy(0);
eval { live_connect( {chobb => 'schoenmaker'} ) };
my $err = $@;
if (length $err == 0) {
    pass( $test_name );
    $err = 0;
}
else {
    if ($err =~ /^proxy connect failed: proxy connect to $PROXY_ADDR_PORT failed: / ) {
        pass( "$test_name - no proxy available" );
    }
    else {
        fail( "$test_name - untrapped error" );
        diag($@);
    }
    $err = 1;
}

SKIP: {
    skip( "no proxy found at $PROXY_ADDR_PORT", 1 )
        if $err;

    Net::SSL::send_useragent_to_proxy(1);
    my $test_name = 'connect through proxy, forward user agent';
    eval { live_connect( {chobb => 'schoenmaker'} ) };
    $err = $@;

    TODO: {
        if ($network_tests) {
            local $TODO = "caller stack walk broken (CPAN bug #4759)";
            is( $err, '', "can forward useragent string to proxy" );
        }
        else {
            pass("can forward useragent string to proxy (network tests disabled)" );
        }
    }
}

SKIP: {
    my $nr_live_tests = 1;
    skip( "Cannot load prerequisite modules @prereq", $nr_live_tests ) if @prereq;
    skip( "Network tests disabled", $nr_live_tests ) unless $network_tests;

    my $ua  = LWP::UserAgent->new;
    $ua->agent('Crypt-SSLeay tester ');
    my $req = HTTP::Request->new;
    my $url = 'https://rt.cpan.org/';

    $req->method('HEAD');
    $req->uri($url);

    my $test_name = 'HEAD https://rt.cpan.org/';
    my $res;
    eval { $res = $ua->request($req) };
    if ($@) {
        my $err = $@;
        fail($test_name);
        diag("eval error = [$err]");
    }
    elsif ($res->is_success) {
        pass($test_name);
    }
    else {
        fail($test_name);
        diag("HTTP status = ", $res->status_line);
        diag("This may not be the fault of the module, $url may be down");
    }
}
