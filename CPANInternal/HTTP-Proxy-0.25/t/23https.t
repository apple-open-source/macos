use Test::More skip_all => "Can't make this work with Crypt::SSLeay";
use strict;
use t::Utils;
use HTTP::Proxy;
use LWP::UserAgent;

# test CONNECT
my $test = Test::Builder->new;

# this is to work around tests in forked processes
$test->use_numbers(0);
$test->no_ending(1);

SKIP: {
    eval "require Crypt::SSLeay;"
    skip "Crypt::SSLeay not installed", 1 if $@;

my $proxy = HTTP::Proxy->new( port => 0, maxconn => 1, logmask => 63 );

# Excerpts from the Crypt::SSLeay documentation
# ---------------------------------------------
# LWP::UserAgent and Crypt::SSLeay have their own versions of proxy support.
#
# At the time of this writing, libwww v5.6 seems to proxy https requests
# fine with an Apache mod_proxy server. It sends a line like:
#
#     GET https://www.nodeworks.com HTTP/1.1
# 
# to the proxy server, which is not the CONNECT request that some
# proxies would expect, so this may not work with other proxy servers
# than mod_proxy.  The CONNECT method is used by Crypt::SSLeay's internal
# proxy support.
#
# For native Crypt::SSLeay proxy support of https requests, you need to
# set an environment variable HTTPS_PROXY to your proxy server & port, as in:
#
#     # PROXY SUPPORT
#     $ENV{HTTPS_PROXY} = 'http://proxy_hostname_or_ip:port';
#     $ENV{HTTPS_PROXY} = '127.0.0.1:8080';
#
# Use of the HTTPS_PROXY environment variable in this way is similar to
# LWP::UserAgent->env_proxy() usage, but calling that method will likely
# override or break the Crypt::SSLeay support, so do not mix the two.

#$proxy->agent( LWP::UserAgent->new ); # no env_proxy
$proxy->init;    # required to access the url later

# fork a HTTP proxy
my $pid = fork_proxy(
    $proxy,
    sub {
        ok( $proxy->conn == 1,
            "Served the correct number of requests" );
    }
);

# run a client
my $ua = LWP::UserAgent->new;    # no env_proxy
$ENV{HTTPS_PROXY} = $proxy->url; # to be used by Crypt::SSLeay
#$ENV{HTTPS_DEBUG} = 1;

my $req = HTTP::Request->new( GET => "https://www.gandi.net/");
my $res = $ua->request($req);
#print $res->status_line,$/,$res->headers->as_string; #500 ??

# make sure the kid is dead
wait;

}

# tests with lwp-request:
# HTTPS_PROXY=http://localhost:8080/ \
# HTTPS_DEBUG=1                      \
# lwp-request -P -des https://www.nodeworks.com/
#
# -P prevents lwp-request to call env_proxy() on its agent,
# so that Crypt::SSLeay can use the HTTPS_PROXY environment variable

