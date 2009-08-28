# $Id: 10-recurse.t 695 2007-12-28 11:17:12Z olaf $ -*-perl-*-

use Test::More;
use strict;

BEGIN {
	if (-e 't/online.enabled') {

	    #
	    # Some people try to run these on private address space."
	    use IO::Socket::INET;
	    my $sock = IO::Socket::INET->new(PeerAddr => '193.0.14.129', # k.root-servers.net.
					  PeerPort => '25',
					  Proto    => 'udp');
	    
	    
	    unless($sock){
		plan skip_all => "Cannot bind to socket:\n\t".$!."\n";
		diag "This is an indication you do not have network problems";
		exit;
	    }else{

		use Net::IP;
		my $ip=Net::IP->new(inet_ntoa($sock->sockaddr));
	    
		if ($ip->iptype() ne "PUBLIC"){
		    plan skip_all => 'Cannot run these tests from this IP:' .$ip->ip() ;		
		}else{
		    plan tests => 12;
		}
	    }

	} else {

		    plan skip_all => 'Online tests disabled.';		



	}
}


BEGIN { use_ok('Net::DNS::Resolver::Recurse'); }


{
	my $res = Net::DNS::Resolver::Recurse->new;

	isa_ok($res, 'Net::DNS::Resolver::Recurse');

	$res->debug(0);	
	$res->udp_timeout(20);
	
	# Hard code A and K.ROOT-SERVERS.NET hint 
	ok($res->hints("193.0.14.129", "198.41.0.4" ), "hints() set");
	
	ok(%{ $res->{'hints'} }, 'sanity check worked');
	
	my $packet;
	
	# Try a domain that is a CNAME
	$packet = $res->query_dorecursion("www.google.com.","A");
	ok($packet, 'got a packet');
	ok(scalar $packet->answer, 'answer has RRs');
	
	# Try a big hairy one
	undef $packet;
	$packet = $res->query_dorecursion("www.rob.com.au.","A");
	ok($packet, 'got a packet');
	ok(scalar $packet->answer, 'anwer section had RRs');
}

# test the callback



{
	my $res = Net::DNS::Resolver::Recurse->new ;
	my $count;
	$res->debug(1);
	# Hard code root hints, there are some environments that will fail
	# the test otherwise
	$res->hints( qw(
			
			192.33.4.12
			128.8.10.90
			192.203.230.10
			192.5.5.241
			192.112.36.4
			128.63.2.53
			192.36.148.17
			192.58.128.30
			193.0.14.129
			199.7.83.42
			202.12.27.33
			198.41.0.4
			192.228.79.201

			));
 

	$res->recursion_callback(sub {
		my $packet = shift;
		
		isa_ok($packet, 'Net::DNS::Packet');
		
		$count++;
	});

	$res->query_dorecursion('a.t.net-dns.org', 'A');
	
	is($count, 3);
}
