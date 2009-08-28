#!/usr/local/bin/perl

use strict;
use warnings;

use Net::DNS;
use Net::DNS::Resolver::Recurse;

my $res = Net::DNS::Resolver::Recurse->new;


$res->recursion_callback(sub {
	my $packet = shift;
	
	$_->print for $packet->additional;
	
	printf(";; Received %d bytes from %s\n\n", $packet->answersize, $packet->answerfrom);
});


$res->query_dorecursion(@ARGV);
