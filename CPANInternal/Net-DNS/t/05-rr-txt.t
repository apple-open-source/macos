# $Id: 05-rr-txt.t 657 2007-06-21 12:57:58Z olaf $

use Test::More tests => 33;
use strict;

my $uut;

BEGIN { use_ok('Net::DNS'); }

#------------------------------------------------------------------------------
# Canned data.
#------------------------------------------------------------------------------

my $name			= 'foo.example.com';
my $class			= 'IN';
my $type			= 'TXT';
my $ttl				= 43201;

my $rr_base	= join(' ', $name, $ttl, $class, $type, "    " );

#Stimulus, expected response, and test name:

my @Testlist =	(
		    {	# 2-5
			stim		=>	q|""|,
			rdatastr	=>	q|""|,
			char_str_list_r	=>	['',],
			descr		=>	'Double-quoted null string',
			},
		    {	# 6-9
			stim		=>	q|''|,
			rdatastr	=>	q|""|,
			char_str_list_r	=>	['',],
			descr		=>	'Single-quoted null string',
			},
		    {	# 10-13
			stim		=>	qq|" \t"|,
			rdatastr	=>	qq|" \t"|,
			char_str_list_r	=>	[ qq| \t|, ],
			descr		=>	'Double-quoted whitespace string',
			},
		    {	# 14-17
			stim		=>	q|noquotes|,
			rdatastr	=>	q|"noquotes"|,
			char_str_list_r	=>	[ q|noquotes|, ],
			descr		=>	'unquoted single string',
			},
		    {	# 18-21
			stim		=>	q|"yes_quotes"|,
			rdatastr	=>	q|"yes_quotes"|,
			char_str_list_r	=>	[ q|yes_quotes|, ],
			descr		=>	'Double-quoted single string',
			},
		    {	# 22-25
			stim		=>	q|"escaped \" quote"|,
			rdatastr	=>	q|"escaped \" quote"|,
			char_str_list_r	=>	[ q|escaped " quote|, ],
			descr		=>	'Quoted, escaped double-quote',
			},
		    {	# 26-29
			stim		=>	q|two tokens|,
			rdatastr	=>	q|"two" "tokens"|,
			char_str_list_r	=>	[ q|two|, q|tokens|, ],
			descr		=>	'Two unquoted strings',
			},
			{ # 30-33
			stim		=> q|"missing quote|,
			rdatastr    => q||,
			char_str_list_r	=>	[],
			descr		=> 'Unbalanced quotes work',
			}
		);

#------------------------------------------------------------------------------
# Run the tests
#------------------------------------------------------------------------------

foreach my $test_hr ( @Testlist ) {
    ok( $uut = Net::DNS::RR->new($rr_base . $test_hr->{'stim'}), 	
		$test_hr->{'descr'} . " -- Stimulus " ); 
		
    is($uut->rdatastr(), $test_hr->{'rdatastr'}, 			
		$test_hr->{'descr'} . " -- Response ( rdatastr ) " ); 
	
	my @list = $uut->char_str_list();	
			
    is_deeply(\@list, $test_hr->{'char_str_list_r'}, 
		$test_hr->{'descr'} . " -- char_str_list equality"  ) ;		
}

my $string1 = q|no|;
my $string2 = q|quotes|;

my $rdata = pack("C", length $string1) . $string1;
$rdata .= pack("C", length $string2) . $string2;

# RR->new_from_hash() drops stuff straight into the hash and 
# re-blesses it, breaking encapsulation.

my %work_hash = (
	Name		=> $name,
	TTL		=> $ttl,
	Class		=> $class,
	Type		=> $type,
	);


# Don't break RR->new_from_hash (e.i. "See the manual pages for each RR 
# type to see what fields the type requires.").

$work_hash{'txtdata'} = q|no quotes|;	

ok( $uut = Net::DNS::RR->new(%work_hash), 		# 30
    "RR->new_from_hash with txtdata -- Stimulus");
ok( $uut->rdatastr() eq q|"no" "quotes"|, 		# 31
    "RR->new_from_hash with txtdata -- Response (rdatastr())");

ok( $uut->rr_rdata() eq $rdata , "TXT->rr_rdata" );	# 32




# And HINFO inherits its parsing from TXT and should therefore work OK as well
my $rr = Net::DNS::RR->new("SRI-NIC.ARPA. HINFO 'DEC-2060 2006' TOPS20");
is($rr->cpu,"DEC-2060 2006","Character string in quotes 1");
is($rr->os,"TOPS20","Character string in quotes 2");
my $rr2 = Net::DNS::RR->new("SRI-NIC.ARPA. HINFO DEC-2060 2006 TOPS20");
ok( !defined($rr2), "Failed parsing of to many HINFO strings");

my $rr3 = Net::DNS::RR->new("SRI-NIC.ARPA. HINFO DEC-2060  TOPS20");

is($rr3->cpu,"DEC-2060","Character string in quotes 3");
is($rr3->os,"TOPS20","Character string in quotes 4");


