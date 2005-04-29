# $Id: 08-online.t,v 1.1 2004/04/09 17:04:49 dasenbro Exp $

use Test::More;
use strict;

BEGIN {
	if (-e 't/online.enabled') {
		plan tests => 72;
	} else {
		plan skip_all => 'Online tests disabled.';
	}
}

BEGIN { use_ok('Net::DNS'); }

my $res = Net::DNS::Resolver->new;

my @rrs = (
	{
		type   		=> 'A',
		name   		=> 'a.t.net-dns.org',
		address 	=> '10.0.1.128',
	},
	{
		type		=> 'MX',
		name		=> 'mx.t.net-dns.org',
		exchange	=> 'a.t.net-dns.org',
		preference 	=> 10,
	},
	{
		type		=> 'CNAME',
		name		=> 'cname.t.net-dns.org',
		cname		=> 'a.t.net-dns.org',
	},
	{
		type		=> 'TXT',
		name		=> 'txt.t.net-dns.org',
		txtdata		=> 'Net-DNS',
	},
		
);

		

foreach my $data (@rrs) {
	my $packet = $res->send($data->{'name'}, $data->{'type'}, 'IN');
	
	ok($packet, "Got an answer for $data->{name} IN $data->{type}");
	is($packet->header->qdcount, 1, 'Only one question');
	is($packet->header->ancount, 1, 'Got single answer');
	
	my $question = ($packet->question)[0];
	my $answer   = ($packet->answer)[0];
	
	ok($question,                           'Got question'            );
	is($question->qname,  $data->{'name'},  'Question has right name' );
	is($question->qtype,  $data->{'type'},  'Question has right type' );
	is($question->qclass, 'IN',             'Question has right class');
	
	ok($answer,                                                       );
	is($answer->class,    'IN',             'Class correct'           );

	
	foreach my $meth (keys %{$data}) {
		is($answer->$meth(), $data->{$meth}, "$meth correct ($data->{name})");
	}
}

# Does the mx() function work.
my @mx = mx('mx2.t.net-dns.org');

my $wanted_names = [qw(a.t.net-dns.org a2.t.net-dns.org)];
my $names        = [ map { $_->exchange } @mx ];

is_deeply($names, $wanted_names, "mx() seems to be working");
		
# some people seem to use mx() in scalar context
is(scalar mx('mx2.t.net-dns.org'), 2,  "mx() works in scalar context");

#
# test that search() and query() DTRT with reverse lookups
#
{
	my @tests = (
		{
			ip => '198.41.0.4',
			host => 'a.root-servers.net',
		},
		{
			ip => '2001:500:1::803f:235',
			host => 'h.root-servers.net',
		},
	);

	foreach my $test (@tests) {
		foreach my $method (qw(search query)) {
			my $packet = $res->$method($test->{'ip'});
			
			isa_ok($packet, 'Net::DNS::Packet');
			
			next unless $packet;
			
			is(($packet->answer)[0]->ptrdname, $test->{'host'}, "$method($test->{'ip'}) works");
		}
	}
}

$res = Net::DNS::Resolver->new(
	domain     => 't.net-dns.org',
	searchlist => ['t.net-dns.org', 'net-dns.org'],
);


#
# test the search() and query() append the default domain and 
# searchlist correctly.
#
{
	$res->defnames(1); $res->dnsrch(1);
	
	my @tests = (
		{
			method => 'search',
			name   => 'a',
		},
		{
			method => 'search',
			name   => 'a.t',
		},
		{
			method => 'query',
			name   => 'a',
		},
	);
	
	foreach my $test (@tests) {
		my $method = $test->{'method'};

		my $ans = $res->$method($test->{'name'});
		
		isa_ok($ans, 'Net::DNS::Packet');
		
		is($ans->header->ancount, 1);
		
		my ($a) = $ans->answer;
		
		isa_ok($a, 'Net::DNS::RR::A');
		is($a->name, 'a.t.net-dns.org');
	}
}
