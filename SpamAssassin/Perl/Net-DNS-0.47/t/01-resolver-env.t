# $Id: 01-resolver-env.t,v 1.1 2004/04/09 17:04:48 dasenbro Exp $


use Test::More tests => 11;
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
