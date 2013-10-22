#!/usr/bin/env perl
use strict;
use warnings;
use HTTP::Proxy;
use HTTP::Proxy::BodyFilter::save;
use Digest::MD5 qw( md5_hex);
use POSIX qw( strftime );

my $proxy = HTTP::Proxy->new(@ARGV);

# a filter to save FLV files somewhere
my $flv_filter = HTTP::Proxy::BodyFilter::save->new(
    filename => sub {
        my ($message) = @_;
        my $uri = $message->request->uri;

        # get the id, or fallback to some md5 hash
        my ($id) = ( $uri->query || '' ) =~ /id=([^&;]+)/i;
        $id = md5_hex($uri) unless $id;

        # compute the filename (including the base site name)
        my ($host) = $uri->host =~ /([^.]+\.[^.]+)$/;
        my $file = strftime "flv/%Y-%m-%d/${host}_$id.flv", localtime;

        # ignore it if we already have it
        return if -e $file && -s $file == $message->content_length;

        # otherwise, save
        return $file;
    },
);

# push the filter for all MIME types we want to catch
for my $mime (qw( video/flv video/x-flv )) {
    $proxy->push_filter(
        mime     => $mime,
        response => $flv_filter,
    );
}

$proxy->start;

