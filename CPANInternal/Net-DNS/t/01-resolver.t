# $Id: 01-resolver.t 673 2007-08-01 09:56:57Z olaf $  -*-perl-*-

use Test::More tests => 45;
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
	force_v4       => 1,
);

#diag "\n\nIf you do not have Net::DNS::SEC installed you will see a warning.\n";
#diag "It is safe to ignore this\n";


while (my ($param, $value) = each %good_input) {
    open (TMPFH,">/dev/null") or die "can't open /dev/null";
    local *STDERR=*TMPFH;
    
    
    is_deeply($res->$param($value), $value, "setting $param returns correctly");
    is_deeply($res->$param(), $value,       "setting $param sticks");
    
    close (TMPFH);	

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

# Some people try to run these on private address space."

use Net::IP;

use IO::Socket::INET;

my $sock = IO::Socket::INET->new(PeerAddr => '193.0.14.129', # k.root-servers.net.
				 PeerPort => '25',
				 Proto    => 'udp');


my $ip=Net::IP->new(inet_ntoa($sock->sockaddr));
	    

SKIP: {
	skip 'Online tests disabled.', 3
		unless -e 't/online.enabled';

	skip 'Tests may not run succesful from private IP('.$ip->ip() .')', 3
	    if ($ip->iptype() ne "PUBLIC");

	my $res = Net::DNS::Resolver->new;
	
	$res->nameservers('a.t.net-dns.org');
	my $ip = ($res->nameservers)[0];
	is($ip, '10.0.1.128', 'Nameservers() looks up IP.') or
	    diag ($res->errorstring . $res->print) ;
	
	$res->nameservers('cname.t.net-dns.org');
	$ip = ($res->nameservers)[0];
	is($ip, '10.0.1.128', 'Nameservers() looks up cname.') or
	    diag ($res->errorstring . $res->print) ;


	# Test to trigger a bug in release 0.59 of Question.pm
	# (rt.cpan.org #28198) (modification of $_ value in various
	# places
	my $die = 0;
	undef ($res); # default values again
	$res = Net::DNS::Resolver->new();

	eval{
	    
	    local $^W = 1;
	    local $SIG{__DIE__} = sub { $die++ };

	    for (0)   # Sets $_ to 0
	    {
		my  $q=$res->send("net-dns.org","SOA");
	    }
	    
	    
	    
	};
	is($die, 0, 'No deaths because of \$_');


}	


