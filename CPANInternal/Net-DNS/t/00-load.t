# -*-perl-*-
# $Id: 00-load.t 666 2007-06-21 15:08:49Z olaf $ 


use Test::More tests => 79;
use strict;


BEGIN { 
    use_ok('Net::DNS'); 
    use_ok('Net::DNS::Resolver::Recurse');
    use_ok('Net::DNS::Nameserver');
    use_ok('Net::DNS::Resolver::Cygwin');  
    # can't test windows, has registry stuff
}

diag("\nThese tests were ran with:\n");
diag("Net::DNS::VERSION:               ".
     $Net::DNS::VERSION);
diag("set environment variable NET_DNS_DEBUG to get all versions");


sub is_rr_loaded {
	my ($rr) = @_;
	
	return $INC{"Net/DNS/RR/$rr.pm"} ? 1 : 0;
}

# Skip all Net::DNS::SEC related records.
my %skip = map { $_ => 1 } qw(SIG NXT KEY DS NSEC RRSIG DNSKEY DLV NSEC3 NSEC3PARAM);

my @rrs = grep { !$skip{$_} } keys %Net::DNS::RR::RR;



#
# Make sure that we haven't loaded any of the RR classes
foreach my $rr (@rrs) {
	ok(!is_rr_loaded($rr), "Net::DNS::RR::$rr is not loaded");
}

#
# Check that we can load all the RR modules.
#
foreach my $rr (@rrs) {
	my $class;
	my $version;
	eval { $class = Net::DNS::RR->_get_subclass($rr); };

	diag($@) if $@;

	ok(is_rr_loaded($rr), "$class loaded");

	next unless is_rr_loaded($rr);

}

#
# Did we get things imported correctly?
#
{ 	
	no strict 'refs';
	foreach my $sym (@Net::DNS::EXPORT) {
		ok(defined &{$sym}, "$sym is imported");
	}
}
			
