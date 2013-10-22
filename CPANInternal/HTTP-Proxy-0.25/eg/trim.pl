#!/usr/bin/perl -w
use HTTP::Proxy qw( :log );
use HTTP::Proxy::BodyFilter::lines;
use HTTP::Proxy::BodyFilter::simple;
use strict;

my $proxy = HTTP::Proxy->new(@ARGV);

# a simple proxy that trims whitespace in HTML
$proxy->push_filter(
    mime => 'text/html',
    response => HTTP::Proxy::BodyFilter::lines->new(),
    response => HTTP::Proxy::BodyFilter::simple->new(
        sub {
            my ($self, $dataref ) = @_;
            $$dataref =~ s/^\s+//m; # multi-line data
            $$dataref =~ s/\s+$//m;
        }
    )
);

$proxy->start;
