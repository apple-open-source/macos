#!/usr/bin/perl -w
use HTTP::Proxy;
use HTTP::Proxy::HeaderFilter::simple;
use HTTP::Proxy::BodyFilter::htmlparser;
use HTTP::Proxy::BodyFilter::htmltext;
use HTML::Parser;
use strict;

# where to find URI in tag attributes
# (it actually a little more complicated, since some tags can have
# several attributes that require an URI)
my %links = (
    a      => 'href',
    area   => 'href',
    base   => 'href',
    link   => 'href',
    frame  => 'src',
    iframe => 'src',
    img    => 'src',
    input  => 'src',
    script => 'src',
    form   => 'action',
    body   => 'background',
);
my $re_tags = join '|', sort keys %links;

my $hrefparser = HTML::Parser->new( api_version => 3 );

# turn all https:// links to http://this_is_ssl links
$hrefparser->handler(
    start => sub {
        my ( $self, $tag, $attr, $attrseq, $text ) = @_;
        if ( $tag =~ /^($re_tags)$/o
            && exists $attr->{$links{$1}}
            && substr( $attr->{$links{$1}}, 0, 8 ) eq "https://" )
        {
            $attr->{$links{$1}} =~ s!^https://!http://this_is_ssl.!;
            $text = "<$tag "
              . join( ' ', map { qq($_="$attr->{$_}") } @$attrseq ) . ">";
        }
        $self->{output} .= $text;
    },
    "self,tagname,attr,attrseq,text"
);

# by default copy everything
$hrefparser->handler(
    default => sub {
        my ( $self, $text ) = @_;
        $self->{output} .= $text;
    },
    "self,text"
);

# the proxy itself
my $proxy = HTTP::Proxy->new(@ARGV);

$proxy->push_filter(
    mime     => 'text/html',
    response =>
      HTTP::Proxy::BodyFilter::htmlparser->new( $hrefparser, rw => 1 ),
);

# detect https requests
$proxy->push_filter(
    request => HTTP::Proxy::HeaderFilter::simple->new(
        sub {
            my ( $self, $headers, $message ) = @_;

            # find out the actual https site
            my $uri = $message->uri;
            if ( $uri =~ m!^http://this_is_ssl\.! ) {
                $uri->scheme("https");
                my $host = $uri->host;
                $host =~ s!^this_is_ssl\.!!;
                $uri->host($host);
            }
        }
    ),
    response => HTTP::Proxy::HeaderFilter::simple->new(
        sub {
            my ( $self, $headers, $message ) = @_;

            # modify Location: headers in the response
            my $location = $headers->header( 'Location' );
            if( $location =~ m!^https://! ) {
                $location =~ s!^https://!http://this_is_ssl.!;
                $headers->header( Location => $location );
            }
        }
    ),
);

$proxy->start;
