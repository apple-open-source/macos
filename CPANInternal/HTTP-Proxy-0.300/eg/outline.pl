#!/usr/bin/perl -w
use HTTP::Proxy qw( :log );
use HTTP::Proxy::BodyFilter::htmlparser;
use HTML::Parser;
use strict;

my $parser = HTML::Parser->new( api_version => 3 );
$parser->handler(
    start_document => sub { my $self = shift; $self->{print} = 1 },
    "self"
);
$parser->handler(
    start => sub {
        my ( $self, $tag, $text ) = @_;
        $self->{print} = 1 if $tag =~ /^h\d/;
        $self->{output} .= $text if $self->{print};
        $self->{print} = 0 if $tag eq 'body';
    },
    "self,tagname,text"
);
$parser->handler(
    end => sub {
        my ( $self, $tag, $text ) = @_;
        $self->{print} = 1 if $tag eq 'body';
        $self->{output} .= $text if $self->{print};
        $self->{print} = 0 if $tag =~ /^h\d/;
    },
    "self,tagname,text"
);
$parser->handler(
    default => sub {
        my ( $self, $text ) = @_;
        $self->{output} .= $text if $self->{print};
    },
    "self,text"
);

my $filter = HTTP::Proxy::BodyFilter::htmlparser->new( $parser, rw => 1 );

my $proxy = HTTP::Proxy->new(@ARGV);
$proxy->push_filter( mime => 'text/html', response => $filter );
$proxy->start;

