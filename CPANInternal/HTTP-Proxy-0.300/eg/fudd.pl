#!/usr/bin/perl -w

# based on Google's Elmer Fudd preference setting

use HTTP::Proxy;
use HTTP::Proxy::BodyFilter::tags;
use HTTP::Proxy::BodyFilter::htmltext;
use strict;

my $proxy = HTTP::Proxy->new(@ARGV);

$proxy->push_filter(
    mime     => 'text/html',
    response => HTTP::Proxy::BodyFilter::tags->new,
    response => HTTP::Proxy::BodyFilter::htmltext->new(
        sub { y/r/w/; s/l(?=\w)/w/g }
    )
);

$proxy->start;

