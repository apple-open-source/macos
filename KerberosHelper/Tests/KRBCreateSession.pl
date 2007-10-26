#!/usr/bin/perl

use Socket;

my $krb_create_session = "/usr/local/bin/KRBCreateSession";

if (! -x $krb_create_session) {
	die "You must build KerberosHelper '-target Everything' to get ". $krb_create_session;
}

(my $lkdc = qx{$krb_create_session}) =~ s/REALM=//;
chomp ($lkdc);

chomp (my $my_hostname = qx{/bin/hostname -s});

my $l = '.local.';				# er, .local?
my $d = '.apple.com';				# FQDN domain
my $btmm_d = '.s\\\\.p\\\\.cooper.members.mac.com.'; 	# BTMM domain

my $l_h = 'kura';		# Local hostname
my $r_h = 'src';		# A routed host
my $mr  = 'OD.APPLE.COM';	# Managed realm
my $ml_h = 'homedepot';		# A local host in a managed realm
my $mr_h = 'od'; 		# A routed host in a managed realm
my $btmm_h = 'kura';		# A BTMM host (not this one)

my ($name,$aliases,$addrtype,$length,@addrs) = gethostbyname ($l_h . $d);

my $ip4_h = inet_ntoa ($addrs[0]);

my $s_princ_l  = 'cifs/'. $lkdc .'@'. $lkdc;
my $s_princ_ml = 'cifs/'. $ml_h. $d .'@'. $mr;
my $s_princ_mr = 'cifs/'. $mr_h. $d .'@'. $mr;

print <<__EOD__;

My hostname:			      $my_hostname
Another host reachable by bonjour:    $l_h
Managed realm			      $mr
A host in a managed realm:	      $mr_h
A routed host:			      $r_h

Derived names:

Barename will be:                     $l_h
.local host will be:		      $l_h$l
FQDN host will be:		      $l_h$d
FQDN (routed) host will be:	      $r_h$d
IP4 addr will be:		      $ip4_h
Barename local managed host will be:  $ml_h
FQDN local managed host will be:      $ml_h$d
Barename routed managed host will be: $mr_h
FQDN routed managed host will be:     $mr_h$d
BTMM host will be:		      $btmm_h$btmm_d

LocalKDC service principal s_princ_lkdc:  $s_princ_l
Managed realm service principal (local):  $s_princ_ml
Managed realm service principal (routed): $s_princ_mr

__EOD__

if ($my_hostname eq $l_h or $my_hostname eq $btmm_h) {
	die "The l_h host or btmm_h cannot be the same as this host";
}

my $s_princ_l  = 'cifs/'. $lkdc .'@'. $lkdc;
my $s_princ_ml = 'cifs/'. $ml_h. $d .'@'. $mr;
my $s_princ_mr = 'cifs/'. $mr_h. $d .'@'. $mr;

my $r_lkdc = [0, 'REALM=LKDC:SHA1.'];		# A LocalKDC realm result
my $r_mged = [0, 'REALM='. $mr];		# A managed realm result
my $r_norlm= [1, 'Error.*'];	 		# No realm can be found

my $no_krb_tests = [
		[ undef, undef, $r_lkdc, "Getting our LocalKDC Realm"],
		[ $l_h, undef, $r_lkdc, "barename host, no s_princ"],
		[ $l_h . $l, undef, $r_lkdc, ".local host, no s_princ"],
		[ $l_h . $d, undef, $r_norlm, "FQDN host, no s_princ (req unicast)"],
		[ $r_h . $d, undef, $r_norlm, "FQDN (routed) host, no s_princ (req unicast)"],
		[ $ip4_h, undef, $r_norlm, "IP4 addr, no s_princ (req unicast)"],
		[ $ml_h, undef, $r_lkdc, "barename local managed host, no s_princ (req krb-conf)"],
		[ $ml_h . $d, undef, $r_norlm, "FQDN local managed host, no s_princ (req krb-conf)"],
		[ $mr_h, undef, $r_norlm, "barename routed managed host, no s_princ (req krb-conf)"],
		[ $mr_h . $d, undef, $r_norlm, "FQDN routed managed host, no s_princ (req krb-conf)"],
		[ $btmm_h . $btmm_d, undef, $r_lkdc, "Using BTMM host, no s_princ"],

		[ $l_h, $s_princ_l, $r_lkdc, "barename host, s_princ_lkdc"],
		[ $l_h . $l, $s_princ_l, $r_lkdc, ".local host, s_princ_lkdc"],
		[ $l_h . $d, $s_princ_l, $r_norlm, "FQDN host, s_princ_lkdc (req unicast)"],
		[ $r_h . $d, $s_princ_l, $r_norlm, "FQDN (routed) host, s_princ_lkdc (req unicast)"],
		[ $ip4_h, $s_princ_l, $r_norlm, "IP4 addr, s_princ_lkdc (req unicast)"],
		[ $ml_h, $s_princ_l, $r_lkdc, "barename local managed host, s_princ_lkdc (req krb-conf)"],
		[ $ml_h . $d, $s_princ_l, $r_norlm, "FQDN local managed host, s_princ_lkdc (req krb-conf)"],
		[ $mr_h, $s_princ_l, $r_norlm, "barename routed managed host, s_princ_lkdc (req krb-conf)"],
		[ $mr_h . $d, $s_princ_l, $r_norlm, "FQDN routed managed host, s_princ_lkdc (req krb-conf)"],
		[ $btmm_h . $btmm_d, $s_princ_l, $r_lkdc, "Using BTMM host, s_princ_lkdc"],

		[ $ml_h, $s_princ_ml, $r_norlm, "barename local managed host, s_princ_managed (req krb-conf)"],
		[ $ml_h . $d, $s_princ_ml, $r_norlm, "FQDN local managed host, s_princ_managed (req krb-conf)"],
		[ $mr_h, $s_princ_mr, $r_norlm, "barename routed managed host, s_princ_managed (req krb-conf)"],
		[ $mr_h . $d, $s_princ_mr, $r_norlm, "FQDN routed managed host, s_princ_managed (req krb-conf)"],
	];

my $krb_tests = [
		[ undef, undef, $r_lkdc, "Getting our LocalKDC Realm"],
		[ $l_h, undef, $r_lkdc, "barename host, no s_princ"],
		[ $l_h . $l, undef, $r_lkdc, ".local host, no s_princ"],
		[ $l_h . $d, undef, $r_mged, "FQDN host, no s_princ (req unicast)"],
		[ $r_h . $d, undef, $r_mged, "FQDN (routed) host, no s_princ (req unicast)"],
		[ $ip4_h, undef, $r_mged, "IP4 addr, no s_princ (req unicast)"],
		[ $ml_h, undef, $r_lkdc, "barename local managed host, no s_princ"],
		[ $ml_h . $d, undef, $r_mged, "FQDN local managed host, no s_princ"],
		[ $mr_h, undef, $r_mged, "barename routed managed host, no s_princ"],
		[ $mr_h . $d, undef, $r_mged, "FQDN routed managed host, no s_princ"],
		[ $btmm_h . $btmm_d, undef, $r_lkdc, "Using BTMM host, no s_princ"],

		[ $l_h, $s_princ_l, $r_lkdc, "barename host, s_princ_lkdc"],
		[ $l_h . $l, $s_princ_l, $r_lkdc, ".local host, s_princ_lkdc"],
		[ $l_h . $d, $s_princ_l, $r_mged, "FQDN host, s_princ_lkdc (req unicast)"],
		[ $r_h . $d, $s_princ_l, $r_mged, "FQDN (routed) host, s_princ_lkdc (req unicast)"],
		[ $ip4_h, $s_princ_l, $r_mged, "IP4 addr, s_princ_lkdc (req unicast)"],
		[ $ml_h, $s_princ_l, $r_lkdc, "barename local managed host, s_princ_lkdc"],
		[ $ml_h . $d, $s_princ_l, $r_mged, "FQDN local managed host, s_princ_lkdc (req unicast)"],
		[ $mr_h, $s_princ_l, $r_mged, "barename routed managed host, s_princ_lkdc (req unicast)"],
		[ $mr_h . $d, $s_princ_l, $r_mged, "FQDN routed managed host, s_princ_lkdc (req unicast)"],
		[ $btmm_h . $btmm_d, $s_princ_l, $r_lkdc, "Using BTMM host, s_princ_lkdc"],

		[ $ml_h, $s_princ_ml, $r_mged, "barename local managed host, s_princ_managed"],
		[ $ml_h . $d, $s_princ_ml, $r_mged, "FQDN local managed host, s_princ_managed"],
		[ $mr_h, $s_princ_mr, $r_mged, "barename routed managed host, s_princ_managed"],
		[ $mr_h . $d, $s_princ_mr, $r_mged, "FQDN routed managed host, s_princ_managed"],
	];


my $passed = 0;
my $failures = 0;

my $krb_conf = '/Library/Preferences/edu.mit.Kerberos';
my $krb_conf_off = $krb_conf .'.OFF';

if (-s $krb_conf > 0) {
	print $krb_conf. " should be empty to run these tests - moving aside!\n\n";
	system ('sudo', 'mv', $krb_conf, $krb_conf_off) == 0 or die "Failed to move ". $krb_conf;
	system ('sudo', 'touch', $krb_conf) == 0 or die "Failed to touch ". $krb_conf;
}

run_tests ($no_krb_tests);

if (-s $krb_conf_off) {
	print "\nrestoring ". $krb_conf. "\n\n";
	system ('sudo', 'mv', $krb_conf_off, $krb_conf) == 0 or die "Failed to restore ". $krb_conf;
}

run_tests ($krb_tests); 

printf "\n\nTotal tests run = %d.  %d passes, %d failures\n", $passed+$failures, $passed, $failures;

sub run_tests {
	my $tests = shift;

	foreach $t (@{$tests}) {
		my $hostname = $t->[0];
		my $s_princ  = $t->[1];
		my ($exit_code, $pattern) = @{$t->[2]};
		my $desc     = $t->[3];

		my $return = qx{$krb_create_session $hostname $s_princ};
		my $exit = $? >> 8;
		chomp ($return);

		if ($exit == $exit_code and $return =~ m{$pattern}) {
			printf "%7s: %s -> %d: %s\n", 'PASSED', $desc, $exit, $return;
			$passed++;
		} else {
			printf "%7s: %s -> %d (%d): %s (%s)\n", 'FAILED', $desc, $exit, $exit_code, $return, $pattern;
			$failures++;
		}
	}
}
