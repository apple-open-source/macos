# $Id: 01-resolver-file.t 616 2006-10-18 09:15:48Z olaf $


use Test::More tests => 8;
use strict;

BEGIN { 
	chdir 't/' || die "Couldn't chdir to t/\n";  
	unshift(@INC, '../blib/lib', '../blib/arch');
	use_ok('Net::DNS');	
}

SKIP: {

	skip 'File parsing only supported on unix.', 7
		unless $Net::DNS::Resolver::ISA[0] eq 'Net::DNS::Resolver::UNIX';
		
	skip 'Could not read configuration file', 7
		unless -r '.resolv.conf' && -o _;

	my $res = Net::DNS::Resolver->new;

	ok($res,                "new() returned something");
	ok($res->nameservers,   "nameservers() works");

	my @servers = $res->nameservers;

	is($servers[0], '10.0.1.128',  'Nameserver set correctly');
	is($servers[1], '10.0.2.128',  'Nameserver set correctly');


	my @search = $res->searchlist;
	is($search[0], 'net-dns.org',     'Search set correctly' );
	is($search[1], 'lib.net-dns.org', 'Search set correctly' );

	is($res->domain,  't.net-dns.org', 'Local domain works'  );
}

 
