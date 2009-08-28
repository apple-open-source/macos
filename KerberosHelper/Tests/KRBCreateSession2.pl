#!/usr/bin/perl
#
# How to run:
#
#   Compile dnswrapper
#
#   Configure Back To My Mac on this host and configure the
#   $btmm_member below
#
#   Test this in an enviroment where you get a hostname from DHCP and
#   where you don't (ie use configured hostname.local name).
#
#   Run this script like this: perl ./KRBCreateSession2.pl
#

use strict;
use Socket;
use Sys::Hostname;

my $verbose = 1;

my $btmm_member = "bitcollector";

my $KRBCreateSession = "../build/Debug/KRBCreateSession";
my $dnsRegister = "../build/Debug/dns-register";
my $dnswrapper = "$ENV{HOME}/src/cos/dnswrapper/dnswr.dylib";

########################################################################
# no configurable parts for users that want to keep their brain intact #
########################################################################

my $btmm_host = "dnedtce16"; #do-not-exists-dont-create-me-16
my $btmm_domain = "${btmm_member}.members.mac.com";
my $btmm_fqdn = "${btmm_host}.${btmm_domain}";

# counters
my $testfailed = 0;
my $numtests = 0;

die "You must build KRBCreateSession" if (! -x $KRBCreateSession);
die "You must build dnswrapper" if (! -f $dnswrapper);

$ENV{'DYLD_INSERT_LIBRARIES'} = $dnswrapper;

my $pwd = `pwd`;
chomp($pwd);

(my $lkdc = qx{$KRBCreateSession}) =~ s/REALM=(.*)\n.*/\1/;
chomp ($lkdc);

my $hostname = hostname();
(my $short_hostname = $hostname) =~ s/([^.]*).*/\1/;

die "hostname is short hostname" if ($hostname eq $short_hostname);

my $hostname_ip4 = "10.0.0.1";

print "my local lkdc realm: $lkdc\n" if ($verbose);
print "my hostname: $hostname ($short_hostname)\n" if ($verbose);
print "my addr: $hostname_ip4\n" if ($verbose);

system "perl -p -e \"s/%name%/$short_hostname/\" < dns-local.txt.in > dns-local.txt";
system "perl -pi -e \"s/%addr%/$hostname_ip4/\" dns-local.txt";

system "sudo launchctl stop edu.mit.Kerberos.krb5kdc";
sleep 1;
system "sudo launchctl start edu.mit.Kerberos.krb5kdc";

my $pid = run_in_background("$dnsRegister managedlocal.local local-only.local local-forward-only.local $btmm_fqdn");
sleep 1;

section_print("Getting LocalKDC realm");

runtest({ name => "Getting LocalKDC realm",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "",
	  REALM => "$lkdc",
	  SERVER => "host/$lkdc\@$lkdc"
	});

my $is_local_name = ($hostname =~ m/\.local$/);
printf "localname: $is_local_name\n" if ($verbose);

my $mrealm = "EXAMPLE.COM";
my $mservername = "host/${short_hostname}.example.com\@EXAMPLE.COM";

my $lrealm = "$lkdc";
my $lservername = "host/$lkdc\@$lkdc";

section_print("Test server selection using local machine");

runtest({ name => "bare hostname (own)",
	  dnsconf => "dns-local.txt",
	  krb5conf => "krb5.conf",
	  inserver => "$short_hostname",
	  REALM => $mrealm,
	  SERVER => $mservername,
	});

runtest({ name => "hostname.local (own)",
	  dnsconf => "dns-local.txt",
	  krb5conf => "krb5.conf",
	  inserver => "${short_hostname}.local",
	  REALM => $mrealm,
	  SERVER => $mservername,
	});

runtest({ name => "fqdn (own)",
	  dnsconf => "dns-local.txt",
	  krb5conf => "krb5.conf",
	  inserver => "$hostname",
	  REALM => $mrealm,
	  SERVER => $mservername,
	});

runtest({ name => "ipv4 address (own)",
	  dnsconf => "dns-local.txt",
	  krb5conf => "krb5.conf",
	  inserver => "$hostname_ip4",
	  REALM => $mrealm,
	  SERVER => $mservername
	});

section_print("Test same subnet managed hosts");

runtest({ name => "same subnet host: bare (other)",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "managedlocal",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/managedlocal.example.com\@EXAMPLE.COM"
	});

runtest({ name => "same subnet host: local",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "managedlocal.local",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/managedlocal.example.com\@EXAMPLE.COM"
	});

runtest({ name => "same subnet host: fqdn",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "managedlocal.example.com",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/managedlocal.example.com\@EXAMPLE.COM"
	});


runtest({ name => "same subnet host: local,should pass",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "local-forward-only.local",
	  REALM => "LKDC:SHA1.",
	  SERVER => "host/LKDC:SHA1."
	});

section_print("Test managed hosts");

runtest({ name => "base name",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "server",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/server.example.com\@EXAMPLE.COM"
	});

runtest({ name => "plain name",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "server.example.com",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/server.example.com\@EXAMPLE.COM"
	});

runtest({ name => "plain name end in dot",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "server.example.com.",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/server.example.com\@EXAMPLE.COM"
	});

runtest({ name => "plain local name",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "server.local",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/server.example.com\@EXAMPLE.COM"
	});

runtest({ name => "local name end in dot",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "server.local.",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/server.example.com\@EXAMPLE.COM"
	});

runtest({ name => "quoted name",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "s\\.c.example.com",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/s\\\\.c.example\\.com\@EXAMPLE\\.COM"
	});

runtest({ name => "quoted name end in dot",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "s\\.c.example.com.",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/s\\\\.c.example\\.com\@EXAMPLE\\.COM"
	});

section_print("ip address (server)");

runtest({ name => "ip address",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "10.0.0.1",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/server.example.com\@EXAMPLE.COM"
	});

section_print("local only hostname");

runtest({ name => "hostname (localonly)",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "local-only.local",
	  REALM => lkdc_realm_name("local-only.local"),
	  SERVER => lkdc_server_name("local-only.local"),
	});

runtest({ name => "bare hostname (localonly)",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "local-only",
	  REALM => lkdc_realm_name("local-only.local"),
	  SERVER => lkdc_server_name("local-only.local"),
	});


section_print("BTMM host");

runtest({ name => "BTMM host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => $btmm_fqdn,
	  REALM => lkdc_realm_name($btmm_fqdn),
	  SERVER => lkdc_server_name($btmm_fqdn),
	});

section_print("Annouced principal, local only hostname");

runtest({ name => "hostname (own) aprincipal=lkdc",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "local-only",
	  annoucedprincipal => lkdc_server_name("local-only.local"),
	  REALM => lkdc_realm_name("local-only.local"),
	  SERVER => lkdc_server_name("local-only.local"),
	});

runtest({ name => "local hostname (own) aprincipal=lkdc",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "local-only.local",
	  annoucedprincipal => lkdc_server_name("local-only.local"),
	  REALM => lkdc_realm_name("local-only.local"),
	  SERVER => lkdc_server_name("local-only.local"),
	});

section_print("Annouced principal, fqdn");

runtest({ name => "other hostname aprincipal=manged",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "server.example.com",
	  annoucedprincipal => "host/server.example.com\@EXAMPLE.COM",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/server.example.com\@EXAMPLE.COM",
	});

runtest({ name => "other bare hostname aprincipal=manged",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "server",
	  annoucedprincipal => "host/server.example.com\@EXAMPLE.COM",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/server.example.com\@EXAMPLE.COM",
	});

section_print("Annouced principal, ipv4 address");

runtest({ name => "ipv4 hostname aprincipal=manged",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "$hostname_ip4",
	  annoucedprincipal => "host/server.example.com\@EXAMPLE.COM",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/server.example.com\@EXAMPLE.COM",
	});

section_print("Annouced principal, fqdn, managed");

runtest({ name => "bare hostname aprincipal=manged",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "server",
	  annoucedprincipal => "host/server.example.com\@EXAMPLE.COM",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/server.example.com\@EXAMPLE.COM",
	});

runtest({ name => "bare hostname aprincipal=manged",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "server.local",
	  annoucedprincipal => "host/server.example.com\@EXAMPLE.COM",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/server.example.com\@EXAMPLE.COM",
	});

runtest({ name => "fqdn hostname aprincipal=manged",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "server.example.com",
	  annoucedprincipal => "host/server.example.com\@EXAMPLE.COM",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/server.example.com\@EXAMPLE.COM",
	});

section_print("BTMM host, Local KDC");

runtest({ name => "BTMM host (aprincipal)",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => $btmm_fqdn,
	  annoucedprincipal => lkdc_server_name($btmm_fqdn),
	  REALM => lkdc_realm_name($btmm_fqdn),
	  SERVER => lkdc_server_name($btmm_fqdn),
	});

section_print("Local subnet manged host");

runtest({ name => "barenamne local subnet manged host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "managedlocal",
	  annoucedprincipal => "host/managedlocal.example.com\@EXAMPLE.COM",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/managedlocal.example.com\@EXAMPLE.COM"
	});

runtest({ name => "fqdn local subnet manged host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "managedlocal.example.com",
	  annoucedprincipal => "host/managedlocal.example.com\@EXAMPLE.COM",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/managedlocal.example.com\@EXAMPLE.COM"
	});

runtest({ name => "fqdn local subnet manged host (end with dot)",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "managedlocal.example.com.",
	  annoucedprincipal => "host/managedlocal.example.com\@EXAMPLE.COM",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/managedlocal.example.com\@EXAMPLE.COM"
	});

runtest({ name => "local subnet local manged host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "managedlocal.local",
	  annoucedprincipal => "host/managedlocal.example.com\@EXAMPLE.COM",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/managedlocal.example.com\@EXAMPLE.COM"
	});

runtest({ name => "local subnet local manged host (end with dot)",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "managedlocal.local.",
	  annoucedprincipal => "host/managedlocal.example.com\@EXAMPLE.COM",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/managedlocal.example.com\@EXAMPLE.COM"
	});

section_print("Routed manged host");

runtest({ name => "barenamne routed subnet manged host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "managedrouted",
	  annoucedprincipal => "host/managedrouted.example.com\@EXAMPLE.COM",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/managedrouted.example.com\@EXAMPLE.COM"
	});

runtest({ name => "fqdn routed subnet manged host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "managedrouted.example.com",
	  annoucedprincipal => "host/managedrouted.example.com\@EXAMPLE.COM",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/managedrouted.example.com\@EXAMPLE.COM"
	});

runtest({ name => "fqdn routed subnet manged host (end with dot)",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "managedrouted.example.com.",
	  annoucedprincipal => "host/managedrouted.example.com\@EXAMPLE.COM",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/managedrouted.example.com\@EXAMPLE.COM"
	});

runtest({ name => "alias to fqdn routed subnet manged host (end with dot)",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "alias-mr.example.com",
	  annoucedprincipal => "host/managedrouted.example.com\@EXAMPLE.COM",
	  REALM => "EXAMPLE.COM",
	  SERVER => "host/managedrouted.example.com\@EXAMPLE.COM"
	});


runtest({ name => "using local name for routed host (failed test)",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "krb5.conf",
	  inserver => "managedrouted.local",
	  annoucedprincipal => "host/managedrouted.example.com\@EXAMPLE.COM",
	  ERROR => "KRBCreateSession",
	});


section_print("Without kerberos tests");

section_print("Getting LocalKDC realm");

runtest({ name => "Getting LocalKDC realm",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "",
	  REALM => "$lkdc",
	  SERVER => "host/$lkdc\@$lkdc"
	});

section_print("Test server selection using local machine");

runtest({ name => "bare hostname (own)",
	  dnsconf => "dns-local.txt",
	  krb5conf => "no-krb5-file",
	  inserver => $short_hostname,
	  REALM => $lrealm,
	  SERVER => $lservername,
	});

runtest({ name => "bare hostname (own)",
	  dnsconf => "dns-local.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "${short_hostname}.local",
	  REALM => $lrealm,
	  SERVER => $lservername,
	});

runtest({ name => "fqdn (own)",
	  dnsconf => "dns-local.txt",
	  krb5conf => "no-krb5-file",
	  inserver => $hostname,
	  REALM => $lrealm,
	  SERVER => $lservername,
	});

section_print("Test ipv4 address");

# XXX should this return LKDC realm for LKDC case ?
runtest({ name => "ipv4 address (own)",
	  dnsconf => "dns-local.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "$hostname_ip4",
	  ERROR => "KRBCreateSession",
	});

section_print("Test managed realm, local subnet (no krb5)");

runtest({ name => "barenamne local subnet manged host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedlocal",
	  REALM => lkdc_realm_name("managedlocal.local"),
	  SERVER => lkdc_server_name("managedlocal.local"),
	});

runtest({ name => "local local subnet manged host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedlocal.local",
	  REALM => lkdc_realm_name("managedlocal.local"),
	  SERVER => lkdc_server_name("managedlocal.local"),
	});

runtest({ name => "fqdn local subnet manged host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedlocal.example.com",
	  ERROR => "KRBCreateSession",
	});

section_print("Test managed realm, routed subnet");

runtest({ name => "barenamne routed subnet manged host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedrouted",
	  ERROR => "KRBCreateSession",
	});

runtest({ name => "fqdn routed subnet manged host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedrouted.example.com",
	  ERROR => "KRBCreateSession",
	});

runtest({ name => "fqdn routed subnet manged host (end with dot)",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedrouted.example.com.",
	  ERROR => "KRBCreateSession",
	});

section_print("Test BTMM host");

runtest({ name => "BTMM host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => $btmm_fqdn,
	  REALM => lkdc_realm_name($btmm_fqdn),
	  SERVER => lkdc_server_name($btmm_fqdn),
	});

section_print("Local subnet manged host");

runtest({ name => "barenamne local subnet manged host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedlocal",
	  annoucedprincipal => "host/managedlocal.example.com\@EXAMPLE.COM",
	  ERROR => "KRBCreateSession",
	});

runtest({ name => "fqdn local subnet manged host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedlocal.example.com",
	  annoucedprincipal => "host/managedlocal.example.com\@EXAMPLE.COM",
	  ERROR => "KRBCreateSession",
	});

runtest({ name => "fqdn local subnet manged host (end with dot)",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedlocal.example.com.",
	  annoucedprincipal => "host/managedlocal.example.com\@EXAMPLE.COM",
	  ERROR => "KRBCreateSession",
	});

runtest({ name => "local subnet local manged host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedlocal.local",
	  annoucedprincipal => "host/managedlocal.example.com\@EXAMPLE.COM",
	  ERROR => "KRBCreateSession",
	});

runtest({ name => "local subnet local manged host (end with dot)",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedlocal.local.",
	  annoucedprincipal => "host/managedlocal.example.com\@EXAMPLE.COM",
	  ERROR => "KRBCreateSession",
	});

section_print("Routed manged host");

runtest({ name => "barenamne routed subnet manged host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedrouted",
	  annoucedprincipal => "host/managedrouted.example.com\@EXAMPLE.COM",
	  ERROR => "KRBCreateSession",
	});

runtest({ name => "fqdn routed subnet manged host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedrouted.example.com",
	  annoucedprincipal => "host/managedrouted.example.com\@EXAMPLE.COM",
	  ERROR => "KRBCreateSession",
	});

runtest({ name => "fqdn routed subnet manged host (end with dot)",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedrouted.example.com.",
	  annoucedprincipal => "host/managedrouted.example.com\@EXAMPLE.COM",
	  ERROR => "KRBCreateSession",
	});

runtest({ name => "using local name for routed host (failed test)",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedrouted.local",
	  annoucedprincipal => "host/managedrouted.example.com\@EXAMPLE.COM",
	  ERROR => "KRBCreateSession",
	});

section_print("Test BTMM host (aprincipal)");

runtest({ name => "BTMM host (aprincipal)",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => $btmm_fqdn,
	  annoucedprincipal => lkdc_server_name($btmm_fqdn),
	  REALM => lkdc_realm_name($btmm_fqdn),
	  SERVER => lkdc_server_name($btmm_fqdn),
	});

section_print("Local subnet manged host");

runtest({ name => "barenamne local subnet manged host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedlocal",
	  annoucedprincipal => "host/managedlocal.example.com\@EXAMPLE.COM",
	  ERROR => "KRBCreateSession",
	});

runtest({ name => "fqdn local subnet manged host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedlocal.example.com",
	  annoucedprincipal => "host/managedlocal.example.com\@EXAMPLE.COM",
	  ERROR => "KRBCreateSession",
	});

runtest({ name => "fqdn local subnet manged host (end with dot)",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedlocal.example.com.",
	  annoucedprincipal => "host/managedlocal.example.com\@EXAMPLE.COM",
	  ERROR => "KRBCreateSession",
	});

runtest({ name => "local subnet local manged host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedlocal.local",
	  annoucedprincipal => "host/managedlocal.example.com\@EXAMPLE.COM",
	  ERROR => "KRBCreateSession",
	});

runtest({ name => "local subnet local manged host (end with dot)",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedlocal.local.",
	  annoucedprincipal => "host/managedlocal.example.com\@EXAMPLE.COM",
	  ERROR => "KRBCreateSession",
	});

section_print("Routed manged host");

runtest({ name => "barenamne routed subnet manged host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedrouted",
	  annoucedprincipal => "host/managedrouted.example.com\@EXAMPLE.COM",
	  ERROR => "KRBCreateSession",
	});

runtest({ name => "fqdn routed subnet manged host",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedrouted.example.com",
	  annoucedprincipal => "host/managedrouted.example.com\@EXAMPLE.COM",
	  ERROR => "KRBCreateSession",
	});

runtest({ name => "fqdn routed subnet manged host (end with dot)",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedrouted.example.com.",
	  annoucedprincipal => "host/managedrouted.example.com\@EXAMPLE.COM",
	  ERROR => "KRBCreateSession",
	});

runtest({ name => "using local name for routed host (failed test)",
	  dnsconf => "dns-t1.txt",
	  krb5conf => "no-krb5-file",
	  inserver => "managedrouted.local",
	  annoucedprincipal => "host/managedrouted.example.com\@EXAMPLE.COM",
	  ERROR => "KRBCreateSession",
	});


kill $pid;

die "FAIL: $testfailed test(s) failed" if ($testfailed);
print "PASS: all $numtests tests passed\n" if (not $testfailed);

exit 0;

sub runtest {
    my $params = shift;
    my %res;
    my $IN;
    my $failed = 0;

    $numtests++;

    $ENV{'KRB5_CONFIG'}     = "$pwd/$$params{krb5conf}";
    $ENV{'DNSWRAPPER_FILE'} = "$pwd/$$params{dnsconf}";

    # quote for shell
    my $host = $$params{inserver};
    $host =~ s/\\/\\\\/;

    my $aprincipal = $$params{annoucedprincipal} or "";

    #system("echo $KRBCreateSession $host $aprincipal");

    open IN, "$KRBCreateSession $host $aprincipal|" or
	die "$KRBCreateSession $!";
    while (<IN>) {
	if (m/^([^=]*)=(.*)/) {
	    $res{$1} = $2;
	}
    }
    close IN;

    foreach my $k ("REALM", "SERVER", "ERROR") {
	next if (not defined $$params{$k});
	unless ($res{$k} =~ m%^$$params{$k}%) {
	    print "$$params{name}: $k $res{$k} =~ $$params{$k} failed\n";
	    $failed++;
	}
    }
    $testfailed++ if ($failed);
    print "ERROR: $res{'ERROR'}\n" if ($failed and defined $res{'ERROR'});
    print "$$params{name}: passed\n" if ($failed == 0);
}

sub section_print
{
    my $msg = "# " . shift(). " #";
    (my $pad = $msg) =~ s/./\#/g;
    print "$pad\n";
    print "$msg\n";
    print "$pad\n";
}

sub run_in_background
{
    my $program = shift();

    my $pid = fork();
    die if ($pid < 0);

    if ($pid == 0) {
	exec $program;
	print "exit: $!\n";
	exit 1;
    }
    return $pid;
}

sub lkdc_realm_name
{
    my $base = shift;
    return "LKDC:SHA1.fake${base}";
}

sub lkdc_server_name
{
    my $realm = lkdc_realm_name(shift());
    return "host/${realm}\@${realm}";
}
