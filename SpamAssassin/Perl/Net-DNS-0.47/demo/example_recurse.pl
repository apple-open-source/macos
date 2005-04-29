#!/usr/local/bin/perl -w

# Example usage for Net::DNS::Resolver::Recurse
# Performs recursion for a query.

use Net::DNS::Resolver::Recurse;
my $res = Net::DNS::Resolver::Recurse->new;
$res->debug(1);
$res->hints("198.41.0.4"); # A.ROOT-SERVER.NET.
my $packet = $res->query_dorecursion("www.rob.com.au.", "A");
$packet && $packet->print;
