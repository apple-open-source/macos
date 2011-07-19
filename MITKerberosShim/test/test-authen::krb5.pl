# The following 2 functions rely upon the CPAN Module Authen::Krb5 (http://search.cpan.org/~jhorwitz/Krb5-1.8/Krb5.pm)
# for various pieces of Kerberos Information. I suspect these are at a high enough level that
# the changes will not affect their functionality

use Authen::krb5;

sub getPrincipalFromServiceRequestTicket {

    my $ticket = shift;
    my $keytab = shift;

    if ( !-e $keytab ) {
        die("The keytab file '$keytab' does not exist.");
    }

    Authen::Krb5::init_context() if ( !Authen::Krb5::context_is_inited() );

    my $ac = new Authen::Krb5::AuthContext;
    my $cc = Authen::Krb5::cc_default();
    my $kt = Authen::Krb5::kt_resolve($keytab);

    # when decoding the ticket, the 3rd parameter is the expected server's name for the ticket.
    # if this parameter is undef, then no verification is performed.
    # the 4th parameter is the keytab. If the keytab is not in '/etc/krb5.keytab' then
    # the keytab file must be specified.
    $ticket = Authen::Krb5::rd_req( $ac, $ticket, undef, $kt );

    if ( !defined($ticket) ) {
        die( "Unable to read kerberos ticket request: ", Authen::Krb5::error() );
    }

    my $serverPrincipal = $ticket->server();
    my $clientPrincipal = $ticket->enc_part2()->client();
    my $server          = $serverPrincipal->data . '@' . $serverPrincipal->realm;
    my $client          = $clientPrincipal->data . '@' . $clientPrincipal->realm;
    $logger->debug("getPrincipalFromServiceRequestTicket: server: $server");
    $logger->debug("getPrincipalFromServiceRequestTicket: client: $client");

    return $client;
}

sub getServiceRequestTicket {

    my $service = shift;
    my $host    = shift;
    my $realm   = shift;

    Authen::Krb5::init_context() if ( !Authen::Krb5::context_is_inited() );

    if ( defined($realm) && $realm ) {
        printf("KrbConnect-getServiceRequestTicket: Set the default realm to '$realm'\n");
        Authen::Krb5::set_default_realm($realm);
    }

    my $ac = new Authen::Krb5::AuthContext;
    my $cc = Authen::Krb5::cc_default();
    if (1) {
        printf("getServiceRequestTicket: KRB5_CONFIG: '$ENV{ KRB5_CONFIG }'\n");
        printf("getServiceRequestTicket: KRB5CCNAME: $ENV{ KRB5CCNAME }\n");
        printf("getServiceRequestTicket: Default realm: %s\n",     Authen::Krb5::get_default_realm() );
        printf("getServiceRequestTicket: Default cachename: %s\n", Authen::Krb5::cc_default_name() );
        printf("getServiceRequestTicket: Authen::Krb5::mk_req( $ac, 0, $service, $host, undef, $cc );\n");
        printf("getServiceRequestTicket: klist dump:\n %s\n", `/usr/bin/klist 2>&1` );
    }

    my $ticket = Authen::Krb5::mk_req( $ac, 0, $service, $host, "$service/$host", $cc );

    #    my $ticket = Authen::Krb5::mk_req( $ac, 0, $service, $host, undef, $cc );

    if ( !defined($ticket) ) {
        my $err = Authen::Krb5::error();
        if ( $err =~ /No credentials cache found/i ) {
            $err
                .= ".\n    Verify that you have valid Kerberos credentials (using /usr/bin/klist). "
                . "\n    If you do not, then try using the AppleConnect application or /usr/bin/kinit tool to establish them.\n"
                . " -- Error thrown ";
        }
        die("Unable to get ticket for '$service/$host': $err");
    }

    return $ticket;
}


my $ticket = getServiceRequestTicket("host", "nutcracker.apple.com");


my $client = getPrincipalFromServiceRequestTicket($ticket, "/etc/krb5.keytab");

