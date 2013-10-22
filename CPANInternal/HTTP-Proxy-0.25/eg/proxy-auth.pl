#!/usr/bin/perl -w
use HTTP::Proxy qw( :log );
use HTTP::Proxy::HeaderFilter::simple;
use MIME::Base64 qw( encode_base64 );
use strict;

# the encoded user:password pair
# login:  http
# passwd: proxy
my $token = "Basic " . encode_base64( "http:proxy", '' );

# a very simple proxy that requires authentication
my $proxy = HTTP::Proxy->new(@ARGV);

# the authentication filter
$proxy->push_filter(
    request => HTTP::Proxy::HeaderFilter::simple->new(
        sub {
            my ( $self, $headers, $request ) = @_;

            # check the token against all credentials
            my $ok = 0;
            $_ eq $token && $ok++
                for $self->proxy->hop_headers->header('Proxy-Authorization');

            # no valid credential
            if ( !$ok ) {
                my $response = HTTP::Response->new(407);
                $response->header(
                    Proxy_Authenticate => 'Basic realm="HTTP::Proxy"' );
                $self->proxy->response($response);
            }
        }
    )
);

$proxy->start;

