# $Id: 05-rr.t 643 2007-05-25 15:19:19Z olaf $   -*-perl-*-

use Test::More;
use strict;
use Net::DNS;
use vars qw( $HAS_DNSSEC $HAS_DLV $HAS_NSEC3 $HAS_NSEC3PARAM);

my $keypathrsa="Kexample.com.+005+24866.private";
my $rsakeyrr;

BEGIN {
    if(
	eval {require Net::DNS::SEC;}
	){
	$HAS_DNSSEC=1;
	if (
	    defined($Net::DNS::SEC::SVNVERSION) && 
	    $Net::DNS::SEC::SVNVERSION > 619 
	    )
	{
	    $HAS_NSEC3PARAM=1;
	    plan tests => 301;  # Hook
	}else{
	    plan tests => 301;
	}
    }else{
	$HAS_DNSSEC=0;
	plan tests => 270;
    }
};




if ($HAS_DNSSEC){  # Create key material    
    diag "The suite will run additonal DNSSEC tests";
    my $privrsakey= << 'ENDRSA' ;
Private-key-format: v1.2
Algorithm: 5 (RSASHA1)
Modulus: osG7zULAQoU3HxVnQl0dj8pLCcxA4ZQk9lgSzd+Q5GvhQYPS4vtnBRvwQDPTckfINqHYbxLQBZGYyl3n0ZQ0W5GDUlnDkeKk+2fe0UIbArY+xkODYGBmv6VGDk1K0kc7mH6cYHUciEtPMdyzYa9hIPfPDp2IE0+BRpr3hPkRnLE=
PublicExponent: Aw==
PrivateExponent: bIEn3iyALFjPag5E1ui+X9wyBogrQQ1t+ZAMiT+17Z1A1lfh7KeaA2f1gCKM9tqFecE69Lc1WQu7MZPv4Q14O/uDO/th5aF6oUL6kYYiSkbmxZ138w6g/PRh+Y/F135Hz8nVyTLrbmo+l5tjiaN5LOgUjvYYwSR3k1FFhgW3zks=
Prime1: zF8a/5xhYpBZH7uVB0xxuo7FbepslQnCSudXRd+1KFmpJ6z4XSDEJVl/XngaVw4j4IvHL9FpjF8JkH1PUn2c7Q==
Prime2: y99dYRRYDdywY6th8ZshkVXYaWUHNWuB68vAr8JZ4XY3qC66S5qehpfPFSX44x05uyRw/JGIDG7gEJHsngBKVQ==
Exponent1: iD9nVRLrlwrmFSe4r4hL0bSDnpxIY1vW3Jo6LpUjcDvGGnNQPhXYGOZU6aVm5LQX6wfaH+DxCD9btajfjFO98w==
Exponent2: h+o+QLg6s+h1l8eWoRIWYOPlm5ivePJWnTKAdSw766QlGsnRh7xprw/fY26l7L4mfML1/bZasvSVYGFIaVWG4w==
Coefficient: BV4xfdcDiyLKBr6647EUocgAziN3qfVsfJc0DdJjYW3VnuECVvNo8Q2ehAYTAwdzNRjBhwB7ZV3Mi6+S8OXFTQ==
ENDRSA


open (RSA,">$keypathrsa") or die "Could not open $keypathrsa";
    print RSA $privrsakey;
    close(RSA);
        
 $rsakeyrr=new Net::DNS::RR ("example.com. IN DNSKEY 256 3 5 AQOiwbvNQsBChTcfFWdCXR2PyksJzEDhlCT2WBLN35Dka+FBg9Li+2cF G/BAM9NyR8g2odhvEtAFkZjKXefRlDRbkYNSWcOR4qT7Z97RQhsCtj7G Q4NgYGa/pUYOTUrSRzuYfpxgdRyIS08x3LNhr2Eg988OnYgTT4FGmveE +RGcsQ==

");
    
    ok( $rsakeyrr, 'RSA public key created');     # test 5

    if ($HAS_DLV){
	diag("DLV Supported in this version of Net::DNS::SEC");
	my $dlv=new Net::DNS::RR ("dskey.example.com. 86400 IN DS 60485 5 2 ( 
                                                D4B7D520E7BB5F0F67674A0C
                                                CEB1E3E0614B93C4F9E99B83
                                                83F6A1E4469DA50A )");    
	ok( $dlv, "DLV RR created");
    }


    if ($HAS_NSEC3PARAM){
	diag("NSEC3PARAM / NSEC3 Supported in this version of Net::DNS::SEC (no tests yet)");
    }


}





BEGIN { use_ok('Net::DNS'); }

#------------------------------------------------------------------------------
# Canned data.
#------------------------------------------------------------------------------

my $name			= "foo.example.com";
my $class			= "IN";
my $ttl				= 43200;

my @rrs = (
	{  	#[0]
		type        => 'A',
	 	address     => '10.0.0.1',  
	}, 
	{	#[1]
		type        => 'AAAA',
		address     => '102:304:506:708:90a:b0c:d0e:ff10',
	}, 
	{	#[2]
		type         => 'AFSDB',
		subtype      => 1,
		hostname     => 'afsdb-hostname.example.com',
	}, 
	{	#[3]
		type         => 'CNAME',
		cname        => 'cname-cname.example.com',
	}, 
	{   #[4]
		type         => 'DNAME',
		dname        => 'dname.example.com',
	},
	{	#[5]
		type         => 'HINFO',
		cpu          => 'test-cpu',
		os           => 'test-os',
	}, 
	{	#[6]
		type         => 'ISDN',
		address      => '987654321',
		sa           => '001',
	}, 
	{	#[7]
		type         => 'MB',
		madname      => 'mb-madname.example.com',
	}, 
	{	#[8]
		type         => 'MG',
		mgmname      => 'mg-mgmname.example.com',
	}, 
	{	#[9]
		type         => 'MINFO',
		rmailbx      => 'minfo-rmailbx.example.com',
		emailbx      => 'minfo-emailbx.example.com',
	}, 
	{	#[10]
		type         => 'MR',
		newname      => 'mr-newname.example.com',
	}, 
	{	#[11]
		type         => 'MX',
		preference   => 10,
		exchange     => 'mx-exchange.example.com',
	},
	{	#[12]
		type         => 'NAPTR',
		order        => 100,
		preference   => 10,
		flags        => 'naptr-flags',
		service      => 'naptr-service',
		regexp       => 'naptr-regexp',
		replacement  => 'naptr-rEplacement.example.com',
	},
	{	#[13]
		type         => 'NS',
		nsdname      => 'ns-nsdname.example.com',
	},
	{	#[14]
		type         => 'NSAP',
		afi          => '47',
		idi          => '0005',
		dfi          => '80',
		aa           => '005a00',
		rd           => '1000',
		area         => '0020',
		id           => '00800a123456',
		sel          => '00',
	},
	{	#[15]
		type         => 'PTR',
		ptrdname     => 'ptr-ptrdname.example.com',
	},
	{	#[16] 
		type         => 'PX',
		preference   => 10,
		map822       => 'px-map822.example.com',
		mapx400      => 'px-mapx400.example.com',
	},
	{	#[17]
		type         => 'RP',
		mbox		 => 'rp-mbox.example.com',
		txtdname     => 'rp-txtdname.example.com',
	},
	{	#[18]
		type         => 'RT',
		preference   => 10,
		intermediate => 'rt-intermediate.example.com',
	},
	{	#[19]
		type         => 'SOA',
		mname        => 'soa-mname.example.com',
		rname        => 'soa-rname.example.com',
		serial       => 12345,
		refresh      => 7200,
		retry        => 3600,
		expire       => 2592000,
		minimum      => 86400,
	},
	{	#[20]
		type         => 'SRV',
		priority     => 1,
		weight       => 2,
		port         => 3,
		target       => 'srv-target.example.com',
	},
	{	#[21]
		type         => 'TXT',
		txtdata      => 'txt-txtdata',
	},
	{	#[22]
		type         => 'X25',
		psdn         => 123456789,
	},
	{	#[23]
		type         => 'LOC',
		version      => 0,
		size         => 3000,
		horiz_pre    => 500000,
		vert_pre     => 500,
		latitude     => 2001683648,
		longitude    => 1856783648,
		altitude     => 9997600,
	}, 	#[24]
	{
		type         => 'CERT',
		'format'     => 3,
		tag			 => 1,
		algorithm    => 1,
		certificate  => '123456789abcdefghijklmnopqrstuvwxyz',
	},

	{	#[25]
		type         => 'SPF',
		txtdata      => 'txt-txtdata',
	},
	
#   38.2.0.192.in-addr.arpa. 7200 IN     IPSECKEY ( 10 1 2
#                    192.0.2.38
#                    AQNRU3mG7TVTO2BkR47usntb102uFJtugbo6BSGvgqt4AQ== )

	{	#[26]
	        type           => 'IPSECKEY',
		precedence     => 10,
		algorithm      => 2,
		gatetype       => 1,
		gateway        => '192.0.2.38',
		pubkey         => "AQNRU3mG7TVTO2BkR47usntb102uFJtugbo6BSGvgqt4AQ==",
	},



	{	#[27]
	        type           => 'IPSECKEY',
		precedence     => 10,
		algorithm      => 2,
		gatetype       => 0,
		gateway        => '.',
		pubkey         => "AQNRU3mG7TVTO2BkR47usntb102uFJtugbo6BSGvgqt4AQ==",
	},


	{	#[28]
	        type           => 'IPSECKEY',
		precedence     => 10,
		algorithm      => 1,
		gatetype       => 2,
		gateway        => '2001:db8:0:8002:0:2000:1:0',
		pubkey         => "AQNRU3mG7TVTO2BkR47usntb102uFJtugbo6BSGvgqt4AQ==",
	},



	{	#[28]
	        type           => 'IPSECKEY',
		precedence     => 10,
		algorithm      => 2,
		gatetype       => 3,
		gateway        => 'gateway.example.com',
		pubkey         => "AQNRU3mG7TVTO2BkR47usntb102uFJtugbo6BSGvgqt4AQ==",
	},



);





#------------------------------------------------------------------------------
# Create the packet and signatures (if DNSSEC is available.)
#------------------------------------------------------------------------------

my @rrsigs;
my $packet = Net::DNS::Packet->new($name);
ok($packet,         'Packet created');

foreach my $data (@rrs) {
    my $RR=Net::DNS::RR->new(
	   name => $name,
	   ttl  => $ttl,
	   %{$data});


       if ($HAS_DNSSEC){
	   my $sigrr= create Net::DNS::RR::RRSIG( [ $RR ],
						  $keypathrsa,
						  (
						   ttl => 360, 
						   sigval => 100,
						  ));
#	   $sigrr->print;
	   push  @rrsigs, $sigrr;
       }
       

       $packet->push('answer', $RR );
}


#------------------------------------------------------------------------------
# Re-create the packet from data.
#------------------------------------------------------------------------------

my $data = $packet->data;
ok($data,            'Packet has data after pushes');

undef $packet;
$packet = Net::DNS::Packet->new(\$data);

ok($packet,          'Packet reconstructed from data');

my @answer = $packet->answer;

ok(@answer && @answer == @rrs, 'Packet returned correct answer section');





while (@answer and @rrs) {
	my $data = shift @rrs;
	my $rr   = shift @answer;
	my $type = $data->{'type'};

	ok($rr,                         "$type - RR defined");    
	is($rr->name,    $name,       	"$type - name() correct");         
	is($rr->class,   $class,      	"$type - class() correct");  
	is($rr->ttl,     $ttl,        	"$type - ttl() correct");              
 
	foreach my $meth (keys %{$data}) {
		is($rr->$meth(), $data->{$meth}, "$type - $meth() correct");
	}
	
	my $rr2 = Net::DNS::RR->new($rr->string);
	is($rr2->string, $rr->string,   "$type - Parsing from string works");
	if ($HAS_DNSSEC){
	    my $rrsig=shift @rrsigs;
	    ok($rrsig->verify([ $rr ], $rsakeyrr), "RR of type ".$type." signature creation/validation cycle");
	}
	
}






unlink($keypathrsa);
