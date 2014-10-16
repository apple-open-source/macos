#!/usr/bin/perl -w
use HTTP::Proxy;
use HTTP::Proxy::HeaderFilter::simple;

# call this proxy as
# eg/switch.pl proxy http://proxy1:port/,http://proxy2:port/
my %args = @ARGV;
my @proxy = split/,/, $args{proxy};
my $proxy = HTTP::Proxy->new(@ARGV);

$proxy->push_filter(
    request => HTTP::Proxy::HeaderFilter::simple->new(
        sub {
            shift->proxy->agent->proxy( http => $proxy[ rand @proxy ] );
        }
    )
);

$proxy->start;
