# $Id: 11-inet6.t 694 2007-12-28 10:32:17Z olaf $ -*-perl-*-


my $has_inet6;
use Test::More tests=>12;
use strict;


BEGIN { use_ok('Net::DNS');

	if ( eval {require Socket6;} &&
		 # INET6 older than 2.01 will not work; sorry.
		 eval {require IO::Socket::INET6; IO::Socket::INET6->VERSION("2.01");}) {
	    import Socket6;
	    $has_inet6=1;
	}else{
	    $has_inet6=0;
	}
       
      }


SKIP: { skip "Socket6 and or IO::Socket::INET6 not loaded\n".
	    "You will need to install these modules for IPv6 transport support", 11 unless $has_inet6;

	diag "";
	diag "The libraries needed for IPv6 support have been found\n";
	diag "\t\tNow we establish if  we can bind to ::1";

	# First test is to bind a nameserver to the ::1 port.
	# That beast should be available on every machine.

	
	# Let us bind a nameserver to ::1. First lets see if we can open a
	# socket anyhow.

	my    $tstsock = IO::Socket::INET6->new(
						Proto => 'tcp',
						LocalAddr => '::1'
					       ) 

	    or 	diag "\n\n\t\tFailed to bind to ::1\n\t\t$!\n\n".
	    "\t\tWe assume there is no IPv6 connectivity and skip the tests\n\n";
	    ;
	    

    }


exit unless $has_inet6; #This prevents nested SKIP blocks.. 



my $answer;
my $res= Net::DNS::Resolver->new;
;
my $res2;

my $AAAA_address;
my $A_address;


SKIP: { skip "online tests are not enabled", 3 unless -e 't/online.enabled';

	# First use the local resolver to query for the AAAA record of a 
        # well known nameserver, than use v6 transport to get to that record.
	diag "";
	diag "";
	diag "\tTesting for global IPv6 connectivity...\n";
	diag "\t\t preparing...";
	$res->debug(1);
	my $nsanswer=$res->send("ripe.net","NS","IN");
	use Data::Dumper;
	print Dumper $nsanswer->answer;
	is (($nsanswer->answer)[0]->type, "NS","Preparing  for v6 transport, got NS records for ripe.net");

	foreach my $ns ($nsanswer->answer){
#	    print $ns->nsdname ."\n";
	    next if $ns->nsdname !~ /ripe\.net/i; # User ripe.net only
	    my $a_answer=$res->send($ns->nsdname,"A","IN");
	    next if ($a_answer->header->ancount == 0);
	    is (($a_answer->answer)[0]->type,"A", "Preparing  for v4 transport, got A records for ". $ns->nsdname);
	    $A_address=($a_answer->answer)[0]->address;


	    diag ("\n\t\t Will try to connect to  ". $ns->nsdname . " ($A_address)");
	    last;
	}



foreach my $ns ($nsanswer->answer){
	    next if $ns->nsdname !~ /ripe\.net/i; # User ripe.net only
	    my $aaaa_answer=$res->send($ns->nsdname,"AAAA","IN");
	    next if ($aaaa_answer->header->ancount == 0);
	    is (($aaaa_answer->answer)[0]->type,"AAAA", "Preparing  for v6 transport, got AAAA records for ". $ns->nsdname);
	    $AAAA_address=($aaaa_answer->answer)[0]->address;


	    diag ("\n\t\t Will try to connect to  ". $ns->nsdname . " ($AAAA_address)");
	    last;
	}

	$res->nameservers($AAAA_address);
#        $res->print;
	$answer=$res->send("ripe.net","SOA","IN");
	if($res->errorstring =~ /Send error: /){
	    diag "\n\t\t Connection failed: " . $res->errorstring ;
	    diag "\n\t\t It seems you do not have global IPv6 connectivity' \n" ;
	    diag "\t\t This is not an error in Net::DNS \n";
	    
	    diag "\t\t You can confirm this by trying 'ping6 ".$AAAA_address."' \n\n";
	}
	
}


 SKIP: { skip "No answer available to analyse (". $res->errorstring.")\n  or online tests are not enabledd", 1 unless ( -e 't/online.enabled' && $answer );
	 
	 # $answer->print;
	 is (($answer->answer)[0]->type, "SOA","Query over udp6 succeeded");
}


 SKIP: { skip "online tests are not enabled", 2 unless -e 't/online.enabled';
	 $res->usevc(1);
	 $res->force_v4(1);
# $res->print;
# $res->debug(1);
	 $answer=$res->send("ripe.net","SOA","IN");
	 is ($res->errorstring,"no nameservers","Correct errorstring when forcing v4");
	 
	 
	 $res->force_v4(0);
	 $answer=$res->send("ripe.net","NS","IN");
	 if ($answer){
	     is (($answer->answer)[0]->type, "NS","Query over tcp6  succeeded");
	 }else{
	     diag "You can safely ignore the following message:";
	     diag ($res->errorstring) if ($res->errorstring ne "connection failed(IPv6 socket failure)");
	     diag ("configuring ".$AAAA_address." ". $A_address." as nameservers");
	     $res->nameservers($AAAA_address,$A_address);
	     undef $answer;
#	$res->print;
	     $answer=$res->send("ripe.net","NS","IN");
	     is (($answer->answer)[0]->type, "NS","Fallback to V4 succeeded");
	     
	     
	 }
	 
}


#
#
#  Now test AXFR functionality.
#
#
my $socket;
SKIP: { skip "online tests are not enabled", 2 unless -e 't/online.enabled';

	# First use the local resolver to query for the AAAA record of a 

	$res2=Net::DNS::Resolver->new;
	# $res2->debug(1);
	my $nsanswer=$res2->send("net-dns.org","NS","IN");
	is (($nsanswer->answer)[0]->type, "NS","Preparing  for v6 transport, got NS records for net-dns.org");
	my $AAAA_address;
	foreach my $ns ($nsanswer->answer){
#	    next if $ns->nsdname !~ /ripe\.net/; # User ripe.net only
	    my $aaaa_answer=$res2->send($ns->nsdname,"AAAA","IN");
	    next if ($aaaa_answer->header->ancount == 0);
	    is (($aaaa_answer->answer)[0]->type,"AAAA", "Preparing  for v6 transport, got AAAA records for ". $ns->nsdname);
	    $AAAA_address=($aaaa_answer->answer)[0]->address;

	    diag ("\n\t\t Trying to connect to  ". $ns->nsdname . " ($AAAA_address)");
	    last;
	}

	$res2->nameservers($AAAA_address);
	# $res2->print;
	
        $socket=$res2->axfr_start('example.com');
}



SKIP: { skip "axfr_start did not return a socket", 2 unless defined($socket);
	is(ref($socket),"IO::Socket::INET6","axfr_start returns IPv6 Socket");
	my ($rr,$err)=$res2->axfr_next;
	is($res2->errorstring,'Response code from server: NOTAUTH',"Transfer is not authorized (but our connection worked)");

}


use Net::DNS::Nameserver;
my $ns = Net::DNS::Nameserver->new(
               LocalAddr        => ['::1'  ],
               LocalPort        => "5363",
               ReplyHandler => \&reply_handler,
               Verbose          => 1
        );


ok($ns,"nameserver object created on IPv6 ::1");
