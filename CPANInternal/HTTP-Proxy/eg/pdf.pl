#!/usr/bin/perl
#
# Saves all PDF files, and just confirm saving to the client
# (the PDF file never arrives to the client, but is replaced by
# a simple HTML file)
#
# Based on a request by Emmanuel Di Prétoro
# 
use strict;
use warnings;
use HTTP::Proxy qw ( :log );
use HTTP::Proxy::BodyFilter::save;
use HTTP::Proxy::BodyFilter::simple;
use HTTP::Proxy::HeaderFilter::simple;

my $proxy = HTTP::Proxy->new( @ARGV );

my $saved;
$proxy->push_filter(
    # you should probably restrict this to certain hosts as well
    path => qr/\.pdf$/,
    mime => 'application/pdf',
    # save the PDF
    response => HTTP::Proxy::BodyFilter::save->new(
        template => "%f",
        prefix   => 'pdf'
    ),
    # send a HTML message instead
    response => HTTP::Proxy::BodyFilter::simple->new(
        begin => sub {
            my ( $self, $message ) = @_;    # for information, saorge
            $saved = 0;
        },
        filter => sub {
            my ( $self, $dataref, $message, $protocol, $buffer ) = @_;
            $$dataref = $saved++ ? "" 
              : sprintf '<p>Saving PDF file. Go <a href="%s">back</a></p>',
                        $message->request->header('referer');
        }
    ),
    # change the response Content-Type
    response => HTTP::Proxy::HeaderFilter::simple->new(
        sub {
            my ( $self, $headers, $response ) = @_;
            $headers->content_type('text/html');
        }
    ),
);

$proxy->start;

