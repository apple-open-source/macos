#!/usr/bin/perl -w
# yeah, I know, I write UK English ;-)
use HTTP::Proxy qw( :log );
use HTTP::Proxy::HeaderFilter::simple;
use strict;

# a very simple proxy
my $proxy = HTTP::Proxy->new( @ARGV );

# the anonymising filter
$proxy->push_filter(
    mime    => undef,
    request => HTTP::Proxy::HeaderFilter::simple->new(
        sub { $_[1]->remove_header(qw( User-Agent From Referer Cookie Cookie2 )) }
    ),
    response => HTTP::Proxy::HeaderFilter::simple->new(
        sub { $_[1]->remove_header(qw( Set-Cookie Set-Cookie2 )) }
    )
);

$proxy->start;
