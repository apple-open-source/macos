#!/usr/bin/perl -w
use HTTP::Proxy;
use HTML::Parser;
use HTTP::Proxy::BodyFilter::htmlparser;

# define the filter (the most difficult part)
# filters not using HTML::Parser are much simpler :-)

my $parser = HTML::Parser->new( api_version => 3 );
$parser->handler(
    start => sub {
        my ( $self, $tag, $text ) = @_;
        $self->{output} .= $text;
        $self->{output} .= "YOUR JAVASCRIPT HERE" if $tag eq 'body';
    },
    "self,tagname,text"
);
$parser->handler(
    default => sub {
        my ($self, $text) = @_;
        $self->{output} .= $text;
    },
    "self,text"
);

# this is a read-write filter (rw => 1)
# that is the reason why we had to copy everything into $self->{output}
my $filter = HTTP::Proxy::BodyFilter::htmlparser->new( $parser, rw => 1 );

# create and launch the proxy
my $proxy = HTTP::Proxy->new(@ARGV);
$proxy->push_filter( response => $filter, mime => 'text/html' );
$proxy->start();

