#!/usr/bin/perl -w
use HTTP::Proxy qw( :log );
use HTTP::Proxy::BodyFilter::htmlparser;
use HTML::Parser;
use strict;

# the proxy
my $proxy = HTTP::Proxy->new( @ARGV );

# all your base...
my @ayb = grep { !/^$/ } split/$/m, << 'AYB';
In A.D. 2101
War was beginning.
What happen ?
Somebody set up us the bomb.
We get signal.
What !
Main screen turn on.
It's You !!
How are you gentlemen !!
All your base are belong to us.
You are on the way to destruction.
What you say !!
You have no chance to survive make your time.
HA HA HA HA ....
Take off every 'zig' !!
You know what you doing.
Move 'zig'.
For great justice.
AYB

# the AYB parser
# replaces heading content with the AYB text
my $parser = HTML::Parser->new( api_version => 3 );
$parser->handler(
    start_document => sub {
        my $self = shift;
        $self->{ayb} = 0;
        $self->{i} = int rand @ayb;
    },
    "self"
);

$parser->handler(
    start => sub {
        my ( $self, $tag, $attr, $text ) = @_;
        $self->{ayb} = 1 if $tag =~ /^h\d/;
        if( $tag eq 'img' ) {
            $attr->{src} = 'http://home.uchicago.edu/~obmontoy/cats.jpg';
            $text = "<$tag "
                  . join(' ', map { qq($_="$attr->{$_}") } keys %$attr )
                  . ">";
        }
        $self->{output} .= $text;
    },
    "self,tagname,attr,text"
);

$parser->handler(
    end => sub {
        my ( $self, $tag, $text ) = @_;
        if( $tag =~ /^h\d/ ) {
            $self->{ayb} = 0;
        }
        $self->{output} .= $text;
    },
    "self,tagname,text"
);

$parser->handler(
    default => sub {
        my ( $self, $text ) = @_;
        $self->{output} .= $self->{ayb} ? $ayb[($self->{i} += 1 ) %= @ayb] : $text;
    },
    "self,text"
);

$proxy->push_filter(
    mime     => 'text/html',
    response => HTTP::Proxy::BodyFilter::htmlparser->new( $parser, rw => 1 ),
);

$proxy->start;
