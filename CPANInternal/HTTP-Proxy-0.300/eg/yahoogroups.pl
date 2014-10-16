#!/usr/bin/perl -w
use strict;
use HTTP::Proxy qw( :log );
use HTTP::Proxy::HeaderFilter::simple;
use CGI::Util qw( unescape );

my $proxy = HTTP::Proxy->new(@ARGV);

$proxy->push_filter(
    host     => 'groups.yahoo.com',
    response => HTTP::Proxy::HeaderFilter::simple->new(
        sub {
            my ( $self, $headers, $message ) = @_;
            my $location;

            # ads start by redirecting to 'interrupt'
            return
              unless ( $location = $headers->header('Location') )
              && $location =~ m!/interrupt\?!;

            # fetch the ad page (we need the cookie)
            # use a new request to avoid modifying the original one
            $self->proxy->log( FILTERS, "YAHOOGROUPS",
                "Ad interrupt detected: fetching $location" );
            my $r = $self->proxy->agent->simple_request(
                HTTP::Request->new(
                    GET => $location,
                    $message->request->headers    # headers are cloned
                )
            );

            # redirect to our original destination
            # which was stored in the 'done' parameter
            # and pass the cookie along
            $location = unescape($location);
            $location =~ s|^(http://[^/]*).*done=([^&]*).*$|$1$2|;
            $headers->header( Location   => $location );
            $headers->header( Set_Cookie => $r->header('Set_Cookie') );
            $self->proxy->log( FILTERS, "YAHOOGROUPS",
                "Set-Cookie: " . $r->header('Set_Cookie') );
        }
    )
);

$proxy->start;

