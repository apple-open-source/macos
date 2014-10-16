#!/usr/bin/perl -w
use HTTP::Proxy qw( :log );
use HTTP::Proxy::HeaderFilter::simple;
use strict;

# a very simple proxy
my $proxy = HTTP::Proxy->new(@ARGV);

# this filter redirects all requests to perlmonks.org
my $filter = HTTP::Proxy::HeaderFilter::simple->new(
    sub {
        my ( $self, $headers, $message ) = @_;

        # modify the host part of the request
        $self->proxy()->log( ERROR, "FOO", $message->uri() );
        $message->uri()->host('perlmonks.org');

        # create a new redirect response
        my $res = HTTP::Response->new(
            301,
            'Moved to perlmonks.org',
            [ Location => $message->uri() ]
        );

        # and make the proxy send it back to the client
        $self->proxy()->response($res);
    }
);

# put this filter on perlmonks.com and www.perlmonks.org
$proxy->push_filter( host => 'perlmonks.com',     request => $filter );
$proxy->push_filter( host => 'www.perlmonks.org', request => $filter );

$proxy->start();
