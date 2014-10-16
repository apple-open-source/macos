#!/usr/bin/perl -w
use strict;
use HTTP::Proxy qw( :log );
use HTTP::Proxy::BodyFilter::simple;
use CGI::Util qw( unescape );

# NOTE: Body request filters always receive the request body in one pass
my $filter = HTTP::Proxy::BodyFilter::simple->new(
    sub {
        my ( $self, $dataref, $message, $protocol, $buffer ) = @_;
        print STDOUT $message->method, " ", $message->uri, "\n";

        # this is from CGI.pm, method parse_params
        my (@pairs) = split ( /[&;]/, $$dataref );
        for (@pairs) {
            my ( $param, $value ) = split ( '=', $_, 2 );
            $param = unescape($param);
            $value = unescape($value);
            printf STDOUT "    %-30s => %s\n", $param, $value;
        }
    }
);

my $proxy = HTTP::Proxy->new(@ARGV);
$proxy->push_filter( method => 'POST', request => $filter );
$proxy->start;

