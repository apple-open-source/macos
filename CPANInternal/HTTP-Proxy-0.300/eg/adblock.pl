#!/usr/bin/perl -w
use strict;
use HTTP::Proxy qw( :log );
use HTTP::Proxy::HeaderFilter::simple;
use vars qw( $re );

# this is a very simple ad blocker
# a full-fledged ad blocker should be a module

# this dot is *not* a web bug ;-)
my $no = HTTP::Response->new( 200 );
$no->content_type('text/plain');
$no->content('.');

my $filter = HTTP::Proxy::HeaderFilter::simple->new( sub {
   my ( $self, $headers, $message ) = @_;
   $self->proxy->response( $no ) if $message->uri->host =~ /$re/o;
} );

my $proxy = HTTP::Proxy->new( @ARGV );
$proxy->push_filter( request => $filter );
$proxy->start;

# a short and basic list
BEGIN {
    $re = join '|', map { quotemeta } qw(
        ads.wanadooregie.com
        cybermonitor.com
        doubleclick.com
        adfu.blockstackers.com
        bannerswap.com
        click2net.com
        clickxchange.com
        dimeclicks.com
        fastclick.net
        mediacharger.com
        mediaplex.com
        myaffiliateprogram.com
        netads.hotwired.com
        valueclick.com
    );
}

