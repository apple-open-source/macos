# $Id: 01-resolver-env.t 616 2006-10-18 09:15:48Z olaf $


use Test::More tests => 13;
use strict;

BEGIN { 

	local $ENV{'RES_NAMESERVERS'} = '10.0.1.128 10.0.2.128';
	local $ENV{'RES_SEARCHLIST'}  = 'net-dns.org lib.net-dns.org';
	local $ENV{'LOCALDOMAIN'}     = 't.net-dns.org';
	local $ENV{'RES_OPTIONS'}     = 'retrans:3 retry:2 debug';

    use_ok('Net::DNS'); 
}


my $res = Net::DNS::Resolver->new;

ok($res,                       "new() returned something");
ok(scalar $res->nameservers,   "nameservers() works");

my @servers = $res->nameservers;

is($servers[0], '10.0.1.128',  'Nameserver set correctly');
is($servers[1], '10.0.2.128',  'Nameserver set correctly');


my @search = $res->searchlist;
is($search[0], 'net-dns.org',     'Search set correctly' );
is($search[1], 'lib.net-dns.org', 'Search set correctly' );

is($res->domain,  't.net-dns.org', 'Local domain works'  );
is($res->retrans, 3,               'Retransmit works'    );
is($res->retry,   2,               'Retry works'         );
ok($res->debug,                    'Debug works'         );





eval {
	$Net::DNS::DNSSEC=0;
	local $SIG{__WARN__}=sub { ok ($_[0]=~/You called the Net::DNS::Resolver::dnssec\(\)/, "Correct warning in absense of Net::DNS::SEC") };	
	$res->dnssec(1);
};

{ 
	$Net::DNS::DNSSEC=1;			
	local $SIG{__WARN__}=sub { diag "We are ignoring that Net::DNS::SEC not installed."	 };
	$res->dnssec(1);	
	is ($res->udppacketsize(),2048,"dnssec() sets udppacketsize to 2048");
};