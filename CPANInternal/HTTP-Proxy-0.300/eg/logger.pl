#!/usr/bin/perl -w
use strict;
use HTTP::Proxy;
use HTTP::Proxy::HeaderFilter::simple;
use HTTP::Proxy::BodyFilter::simple;
use CGI::Util qw( unescape );

# get the command-line parameters
my %args = (
   peek    => [],
   header  => [],
   mime    => 'text/*',
);
{
    my $args = '(' . join( '|', keys %args ) . ')';
    for ( my $i = 0 ; $i < @ARGV ; $i += 2 ) {
        if ( $ARGV[$i] =~ /$args/o ) {
            if ( ref $args{$1} ) {
                push @{ $args{$1} }, $ARGV[ $i + 1 ];
            }
            else {
                $args{$1} = $ARGV[ $i + 1 ];
            }
            splice( @ARGV, $i, 2 );
            redo if $i < @ARGV;
        }
    }
}

# the headers we want to see
my @srv_hdr = (
    qw( Content-Type Set-Cookie Set-Cookie2 WWW-Authenticate Location ),
    @{ $args{header} }
);
my @clt_hdr =
  ( qw( Cookie Cookie2 Referer Referrer Authorization ), @{ $args{header} } );

# NOTE: Body request filters always receive the request body in one pass
my $post_filter = HTTP::Proxy::BodyFilter::simple->new(
    begin  => sub { $_[0]->{binary} = 0; },
    filter => sub {
        my ( $self, $dataref, $message, $protocol, $buffer ) = @_;
        print STDOUT "\n", $message->method, " ", $message->uri, "\n";
        print_headers( $message, @clt_hdr );

        if ( $self->{binary} || $$dataref =~ /\0/ ) {
            $self->{binary} = 1;
            print STDOUT "    (not printing binary data)\n";
            return;
        }

        # this is from CGI.pm, method parse_params()
        my (@pairs) = split( /[&;]/, $$dataref );
        for (@pairs) {
            my ( $param, $value ) = split( '=', $_, 2 );
            $param = unescape($param);
            $value = unescape($value);
            printf STDOUT "    %-20s => %s\n", $param, $value;
        }
    }
);

my $get_filter = HTTP::Proxy::HeaderFilter::simple->new(
    sub {
        my ( $self, $headers, $message ) = @_;
        my $req = $message->request;
        if ( $req->method ne 'POST' ) {
            print STDOUT "\n", $req->method, " ", $req->uri, "\n";
            print_headers( $req, @clt_hdr );
        }
        print STDOUT $message->status_line, "\n";
        print_headers( $message, @srv_hdr );
    }
);

sub print_headers {
    my $message = shift;
    for my $h (@_) {
        if ( $message->header($h) ) {
            print STDOUT "    $h: $_\n" for ( $message->header($h) );
        }
    }
}

# create and start the proxy
my $proxy = HTTP::Proxy->new(@ARGV);

# if we want to look at SOME sites
if (@{$args{peek}}) {
    for (@{$args{peek}}) {
        $proxy->push_filter(
            host    => $_,
            method  => 'POST',
            request => $post_filter
        );
        $proxy->push_filter(
            host     => $_,
            response => $get_filter,
            mime     => $args{mime},
        );
    }
}
# otherwise, peek at all sites
else {
    $proxy->push_filter(
        method  => 'POST',
        request => $post_filter
    );
    $proxy->push_filter( response => $get_filter, mime => $args{mime} );
}

$proxy->start;

