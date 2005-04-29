# $Id: 01-resolver.t,v 1.1 2004/04/09 17:04:48 dasenbro Exp $

use Test::More tests => 42;
use strict;
use File::Spec;

BEGIN { use_ok('Net::DNS'); }

my $res = Net::DNS::Resolver->new();

ok($res,                           'new() returned something');
isa_ok($res, 'Net::DNS::Resolver', 'new() returns an object of the correct class.');
ok(scalar $res->nameservers,       'nameservers() works');

my $searchlist = [qw(t.net-dns.org t2.net-dns.org)];

is_deeply([$res->searchlist(@$searchlist)], $searchlist, 'setting searchlist returns correctly.');
is_deeply([$res->searchlist],               $searchlist, 'setting searchlist stickts.');

my %good_input = (
	port		   => 54,
	srcaddr        => '10.1.0.1',
	srcport        => 53,
	domain	       => 'net-dns.org',
	retrans	       => 6,
	retry		   => 5,
	usevc		   => 1,
	stayopen       => 1,
	igntc          => 1,
	recurse        => 0,
	defnames       => 0,
	dnsrch         => 0,
	debug          => 1,
	tcp_timeout    => 60,
	udp_timeout    => 60,
	persistent_tcp => 1,
	dnssec         => 1,
);

while (my ($param, $value) = each %good_input) {
	is_deeply($res->$param($value), $value, "setting $param returns correctly");
	is_deeply($res->$param(), $value,       "setting $param sticks");
}
	
	

my %bad_input = (
	tsig_rr        => 'set',
	errorstring    => 'set',
	answerfrom     => 'set',
	answersize     => 'set',
	querytime      => 'set',
	axfr_sel       => 'set',
	axfr_rr        => 'set',
	axfr_soa_count => 'set',
	udppacketsize  => 'set',
	cdflag         => 'set',
);	

SKIP: {
	skip 'Online tests disabled.', 2
		unless -e 't/online.enabled';

	my $res = Net::DNS::Resolver->new;
	
	$res->nameservers('a.t.net-dns.org');
	my $ip = ($res->nameservers)[0];
	is($ip, '10.0.1.128', 'Nameservers() looks up IP.');
	
	$res->nameservers('cname.t.net-dns.org');
	$ip = ($res->nameservers)[0];
	is($ip, '10.0.1.128', 'Nameservers() looks up cname.');
}	


