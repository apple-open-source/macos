use strict;
use Test::More tests => 8;

use Net::SSL;

# ensure no proxification takes place
$ENV{NO_PROXY} = '127.0.0.1';

my $sock;
eval {
    $sock = Net::SSL->new(
        PeerAddr => '127.0.0.1',
        PeerPort => 443,
        Timeout  => 3,
    );
};

my $test_name = 'Net::SSL->new';
if ($@) {
    my $fail = $@;
    if ($fail =~ /\AConnect failed: connect: \b/i) {
        pass( "$test_name - expected failure" );
    }
    else {
        fail( "$test_name" );
        diag( $fail );
    }
}
else {
    ok( defined $sock, $test_name );
}

SKIP: {
    skip( "nothing listening on localhost:443", 7 )
        unless defined $sock;

    is( ref($sock), 'Net::SSL', 'blessed socket' );

    eval { $sock->accept };
    like ($@, qr(\Aaccept not implemented for Net::SSL sockets),
        'accept() not implemented'
    );

    eval { $sock->getc };
    like ($@, qr(\Agetc not implemented for Net::SSL sockets),
        'getc() not implemented'
    );

    eval { $sock->ungetc };
    like ($@, qr(\Aungetc not implemented for Net::SSL sockets),
        'ungetc() not implemented'
    );

    eval { $sock->getlines };
    like ($@, qr(\Agetlines not implemented for Net::SSL sockets),
        'getlines() not implemented'
    );

    is( $sock->blocking, 1, 'socket is blocking' );
    $sock->blocking(0);
    is( $sock->blocking, 0, 'socket is now non-blocking' );
}
