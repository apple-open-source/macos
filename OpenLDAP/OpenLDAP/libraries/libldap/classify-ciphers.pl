#!/usr/bin/perl -w

# Copyright (c) 2012 Apple Inc. All Rights Reserved.
# Not for distribution outside Apple.

# Sort and classify cipher suites for XSCipherSpecParse().
# Compatibility with OpenSSL is provided for now to ease transition to
# the Security framework.  If OpenSSL support is removed from the
# product then all references to OpenSSL should be removed from this
# script and from XSCipherSpecParse().
#
# References:
# /System/Library/Frameworks/Security.framework/Headers/CipherSuite.h
# /usr/include/openssl/ssl.h
# http://www.openssl.org/docs/apps/ciphers.html
# rdar://problem/10587072
#
# Roughly:
# 1. CHACHA20 > AES-256 > AES-128 > 3DES > IDEA > RC4 > RC2 > FORTEZZA > DES-CBC > EXPORT > NULL
# 2. SHA-512 > SHA-384 > POLY1305 > SHA-256 > SHA-1 > MD5 > NULL
# 3. ECDHE-ECDSA > ECDHE-RSA > RSA > ECDH-ECDSA > ECDH-RSA > DHE-RSA > DHE-DSS > DH-RSA > DH-DSS > FORTEZZA > DHE-PSK > RSA-PSK > PSK > EXPORT > DH_anon > NULL
#
# This script will need to be updated when the sets of ciphers supported
# by OpenSSL and the Security framework change.  That's part of the
# reason this script exists:  to call attention to such changes so
# someone can adapt this script to the changes.  The other reason this
# script exists is to make such adaptations easy.
#
# To teach this script about new Security framework or OpenSSL ciphers,
# update these data structures as needed:
#	@keysig_ordered_annotated		if adding a new key/signature algorithm e.g. RSA_PSK
#	%openssl_to_keysig				ditto
#	@bulk_ordered_annotated			if adding a new bulk cipher e.g. CAMELLIA
#	%openssl_to_bulk				ditto
#	@mac_ordered_annotated			if adding a new MAC algorithm e.g. SHA3
#	%openssl_to_mac					ditto
#	@strength_ordered_annotated		if adding a new strength e.g. ULTRA
#	%strengths						if adding a new bulk cipher or strength
#	@export							if adding a new export grade e.g. EXPORT56
# Also keep @openssl_aliases up to date from documentation.  Unfortunately
# there's no way to generate this list dynamically.
# Also keep the definition of DEFAULT up to date.  HIGH plus MEDIUM is OK
# today but in the future DEFAULT might need to be e.g. ULTRA plus HIGH.
# References to DEFAULT are scattered throughout.
# Some ciphers are demoted because they don't work properly in servers.
#
# To test:
# ./classify-ciphers.pl --implcfile XSCipherData.c --implprefix xs --pubhfile XSCipher.h --pubefile ../CoreDaemon.exp --pubprefix XS


use strict;
use Getopt::Long;
$| = 1;

my $prog = $0;
$prog =~ s,.*/,,;

my $openssl_default = "/usr/bin/openssl";
my $ciphersuiteh_default = "/System/Library/Frameworks/Security.framework/Headers/CipherSuite.h";
my $tab_default = 4;

sub usage
{
	die <<EOT;
Usage: $prog <params> [options]
Required parameters:
  --implcfile path      write implementation C source to this file
  --implprefix string   use this prefix for implementation C identifiers (e.g. xx)
  --pubprefix string    use this prefix for public C identifiers (e.g. XX)
Options:
  --debug               extra output
  --implhfile path      write implementation C header to this file
  --openssl path        path to openssl binary [default $openssl_default]
  --pubefile path       replace templates in this .exp file
  --pubhfile path       replace templates in this public C header file
  --quiet               less output
  --suiteh path         path to CipherSuite.h file [default $ciphersuiteh_default]
  --tab width           width of a tab [default $tab_default for Xcode compatibility]
EOT
}

my $errors = 0;

my %opts;
GetOptions(\%opts,
	'debug',
	'implcfile=s',
	'implhfile=s',
	'implprefix=s',
	'openssl=s',
	'pubefile=s',
	'pubhfile=s',
	'pubprefix=s',
	'quiet',
	'suiteh=s',
	'tab=i',
) || usage();
usage() if @ARGV > 0;
if (!defined($opts{implcfile}) || $opts{implcfile} eq "") {
	warn("--implcfile needed\n");
	++$errors;
}
if (!defined($opts{implprefix}) || $opts{implprefix} eq "") {
	warn("--implprefix needed\n");
	++$errors;
}
if (!defined($opts{pubprefix}) || $opts{pubprefix} eq "") {
	warn("--pubprefix needed\n");
	++$errors;
}
$opts{openssl} = $openssl_default unless defined $opts{openssl};
$opts{suiteh} = $ciphersuiteh_default unless defined $opts{suiteh};
$opts{tab} = $tab_default unless defined($opts{tab}) && $opts{tab} > 0;
usage() if $errors;

# ciphers are named "prefix_keysig_WITH_bulk_mac"
my $prefix_pattern = qr{(?:SSL|TLS)};
my $cipher_pattern = qr{${prefix_pattern}_[a-zA-Z0-9_]+_WITH_[a-zA-Z0-9_]+};

# key/signature algorithms sorted by decreasing strength
my @keysig_ordered_annotated = (
	{ name => "ECDHE_ECDSA",	desc => "Ephemeral elliptic curve Diffie-Hellman with elliptic curve DSA",	demoted => 0 },
	{ name => "ECDHE_RSA",		desc => "Ephemeral elliptic curve Diffie-Hellman with RSA",					demoted => 0 },
	{ name => "RSA",			desc => "RSA",																demoted => 0 },
	{ name => "ECDH_ECDSA",		desc => "Elliptic curve Diffie-Hellman with elliptic curve DSA",			demoted => 0 },
	{ name => "ECDH_RSA",		desc => "Elliptic curve Diffie-Hellman with RSA",							demoted => 0 },
	{ name => "DHE_RSA",		desc => "Ephemeral Diffie-Hellman with RSA",								demoted => 0 },
	{ name => "DHE_DSS",		desc => "Ephemeral Diffie-Hellman with DSS",								demoted => 0 },
	{ name => "DH_RSA",			desc => "Diffie-Hellman with RSA",											demoted => 0 },
	{ name => "DH_DSS",			desc => "Diffie-Hellman with DSS",											demoted => 0 },
	{ name => "FORTEZZA_DMS",	desc => "Fortezza",															demoted => 0 },
	{ name => "ECDHE_PSK",		desc => "Elliptic curve Diffie-Hellman with pre-shared key",				demoted => 1 },	# PSK is unsuitable for servers
	{ name => "DHE_PSK",		desc => "Ephemeral Diffie-Hellman with pre-shared key",						demoted => 1 },	# PSK is unsuitable for servers
	{ name => "RSA_PSK",		desc => "RSA with pre-shared key",											demoted => 1 },	# PSK is unsuitable for servers
	{ name => "PSK",			desc => "Pre-shared key",													demoted => 1 },	# PSK is unsuitable for servers
	{ name => "RSA_EXPORT",		desc => "Export grade RSA",													demoted => 0 },
	{ name => "DHE_RSA_EXPORT",	desc => "Export grade ephemeral Diffie-Hellman with RSA",					demoted => 0 },
	{ name => "DHE_DSS_EXPORT",	desc => "Export grade ephemeral Diffie-Hellman with DSS",					demoted => 0 },
	{ name => "DH_RSA_EXPORT",	desc => "Export grade Diffie-Hellman with RSA",								demoted => 0 },
	{ name => "DH_DSS_EXPORT",	desc => "Export grade Diffie-Hellman with DSS",								demoted => 0 },
	{ name => "ECDH_anon",		desc => "Anonymous elliptic curve Diffie-Hellman",							demoted => 0 },
	{ name => "DH_anon",		desc => "Anonymous Diffie-Hellman",											demoted => 0 },
	{ name => "DH_anon_EXPORT",	desc => "Export grade anonymous Diffie-Hellman",							demoted => 0 },
	{ name => "NULL",			desc => "No encryption",													demoted => 0 },
);
my @keysig_ordered = map { $keysig_ordered_annotated[$_]->{name} } (0..$#keysig_ordered_annotated);

# bulk ciphers sorted by decreasing strength
my @bulk_ordered_annotated = (
	{ name => "CHACHA20",		desc => "ChaCha20",					strbits => 256,	algbits => 256,			demoted => 0 },
	{ name => "AES_256_GCM",	desc => "256-bit AES with GCM",		strbits => 256,	algbits => 256,			demoted => 0 },
	{ name => "AES_256_CBC",	desc => "256-bit AES",				strbits => 256,	algbits => 256,			demoted => 0 },
	{ name => "AES_128_GCM",	desc => "128-bit AES with GCM",		strbits => 128,	algbits => 128,			demoted => 0 },
	{ name => "AES_128_CBC",	desc => "128-bit AES",				strbits => 128,	algbits => 128,			demoted => 0 },
	{ name => "3DES_EDE_CBC",	desc => "Triple-DES",				strbits => 168,	algbits => 168,			demoted => 0 },
	{ name => "IDEA_CBC",		desc => "IDEA",						strbits => 128,	algbits => 128,			demoted => 0 },
	{ name => "RC4_128",		desc => "128-bit RC4",				strbits => 128,	algbits => 128,			demoted => 0 },
	{ name => "RC2_CBC",		desc => "RC2",						strbits => 128,	algbits => 128,			demoted => 0 },
	{ name => "FORTEZZA_CBC",	desc => "Fortezza",					strbits =>  96,	algbits =>  96,			demoted => 0 },
	{ name => "DES_CBC",		desc => "DES",						strbits =>  56,	algbits =>  56,			demoted => 0 },
	{ name => "RC4_40",			desc => "40-bit RC4",				strbits =>  40,	algbits => 128,			demoted => 0 },
	{ name => "RC2_CBC_40",		desc => "40-bit RC2",				strbits =>  40,	algbits => 128,			demoted => 0 },
	{ name => "DES40_CBC",		desc => "40-bit DES",				strbits =>  40,	algbits =>  56,			demoted => 0 },
	{ name => "NULL",			desc => "No encryption",			strbits =>   0,	algbits =>   0,			demoted => 0 },
);
my @bulk_ordered = map { $bulk_ordered_annotated[$_]->{name} } (0..$#bulk_ordered_annotated);
my %bulk_strbits = map { $bulk_ordered_annotated[$_]->{name} => $bulk_ordered_annotated[$_]->{strbits} } (0..$#bulk_ordered_annotated);
my %bulk_algbits = map { $bulk_ordered_annotated[$_]->{name} => $bulk_ordered_annotated[$_]->{algbits} } (0..$#bulk_ordered_annotated);

# MAC algorithms sorted by decreasing strength
my @mac_ordered_annotated = (
	{ name => "SHA512",				desc => "SHA-512",			demoted => 0 },
	{ name => "SHA384",				desc => "SHA-384",			demoted => 0 },
	{ name => "POLY1305_SHA256",	desc => "POLY1305",			demoted => 0 },
	{ name => "SHA256",				desc => "SHA-256",			demoted => 0 },
	{ name => "SHA",				desc => "SHA-1",			demoted => 0 },
	{ name => "MD5",				desc => "MD5",				demoted => 0 },
	{ name => "NULL",				desc => "No encryption",	demoted => 0 },
);
my @mac_ordered = map { $mac_ordered_annotated[$_]->{name} } (0..$#mac_ordered_annotated);

# make patterns for matching the above unambiguously
my @keysig_bylength = sort { length($b) <=> length($a) } @keysig_ordered;
my $keysig_pattern = join("|", @keysig_bylength);
$keysig_pattern = qr($keysig_pattern);
my @bulk_bylength = sort { length($b) <=> length($a) } @bulk_ordered;
my $bulk_pattern = join("|", @bulk_bylength);
$bulk_pattern = qr($bulk_pattern);
my @mac_bylength = sort { length($b) <=> length($a) } @mac_ordered;
my $mac_pattern = join("|", @mac_bylength);
$mac_pattern = qr($mac_pattern);
my @bulk_strbits_bylength = sort { length("$b") <=> length("$a") } values %bulk_strbits;
my @bulk_algbits_bylength = sort { length("$b") <=> length("$a") } values %bulk_algbits;

# make hashes for sorting the above easily
my %keysig_pri;
$keysig_pri{$keysig_ordered[$_]} = $_ for (0..$#keysig_ordered);
my %bulk_pri;
$bulk_pri{$bulk_ordered[$_]} = $_ for (0..$#bulk_ordered);
my %mac_pri;
$mac_pri{$mac_ordered[$_]} = $_ for (0..$#mac_ordered);

# rules for determining overall strength
my @strength_ordered_annotated = (
	{ name => "HIGH",			desc => "High strength ciphers",																	demoted => 0 },
	{ name => "HIGH_aNULL",		desc => "High strength ciphers which aren't in HIGH because they use anonymous key exchange",		demoted => 0 },
	{ name => "MEDIUM",			desc => "Medium strength ciphers",																	demoted => 0 },
	{ name => "MEDIUM_aNULL",	desc => "Medium strength ciphers which aren't in MEDIUM because they use anonymous key exchange",	demoted => 0 },
	{ name => "LOW",			desc => "Low strength ciphers",																		demoted => 0 },
	{ name => "LOW_aNULL",		desc => "Low strength ciphers which aren't in MEDIUM because they use anonymous key exchange",		demoted => 0 },
	{ name => "EXPORT",			desc => "Export grade ciphers",																		demoted => 0 },
	{ name => "EXPORT_aNULL",	desc => "Export grade ciphers which aren't in MEDIUM because they use anonymous key exchange",		demoted => 0 },
	{ name => "eNULL",			desc => "No encryption",																			demoted => 0 },
	{ name => "eNULL_aNULL",	desc => "No encryption and anonymous key exchange",													demoted => 0 },
);
my @strength_ordered = map { $strength_ordered_annotated[$_]->{name} } (0..$#strength_ordered_annotated);
my %strengths = (
	"HIGH"		=> [@bulk_ordered[0..5]],	# CHACHA20, AES-256, AES-128, 3DES
	"MEDIUM"	=> [@bulk_ordered[6..9]],	# IDEA, RC4, RC2, FORTEZZA
	"LOW"		=> [@bulk_ordered[10..10]],	# DES
	"EXPORT"	=> [@bulk_ordered[11..13]],	# RC4-40, RC2-40, DES-40
	"eNULL"		=> [@bulk_ordered[14..14]],	# NULL
);

# internal consistency checks
if (@bulk_ordered != 15) {
	warn("Incomplete mapping of bulk ciphers to strengths.  Update \%strengths in $prog!\n");
	++$errors;
}
for my $strength (@strength_ordered) {
	next if $strength =~ /_aNULL$/;
	if (!defined($strengths{$strength})) {
		warn("Unknown strength \"$strength\".  Update \%strengths in $prog!\n");
		++$errors;
	}
}
for my $strength (keys %strengths) {
	if (!grep { $_ eq $strength } @strength_ordered) {
		warn("Unknown strength \"$strength\".  Update \@strength_ordered_annotated in $prog!\n");
		++$errors;
	}
}
if ($errors) {
	die("$prog: $errors internal consistency botch(es)\n");
}

# make strength descriptions even more helpful
for my $i (0..$#strength_ordered) {
	my $strength = $strength_ordered[$i];
	next unless defined $strengths{$strength};
	$strength_ordered_annotated[$i]->{desc} .= " (";
	my $first = 1;
	for my $bulk (sort @{$strengths{$strength}}) {
		$strength_ordered_annotated[$i]->{desc} .= ", " unless $first;
		$first = 0;
		$strength_ordered_annotated[$i]->{desc} .= $bulk;
	}
	$strength_ordered_annotated[$i]->{desc} .= ")";
}
my @strength_bylength = sort { length($b) <=> length($a) } @strength_ordered;
my %strength_pri;
$strength_pri{$strength_ordered[$_]} = $_ for (0..$#strength_ordered);

# invert %strengths for easy lookups
my %bulk_strength;
for my $strength (keys %strengths) {
	for my $bulk (@{$strengths{$strength}}) {
		$bulk_strength{$bulk} = $strength;
	}
}

# since we've already sorted, determine the longest of each string now too
my %widths = (
	"keysig"		=> length($keysig_bylength[0]),
	"bulk"			=> length($bulk_bylength[0]),
	"mac"			=> length($mac_bylength[0]),
	"strength"		=> length($strength_bylength[0]),
	"bulk_strbits"	=> length($bulk_strbits_bylength[0]),
	"bulk_algbits"	=> length($bulk_algbits_bylength[0]),
);

# SSLv2 ciphers, listed explicitly
my @sslv2_ordered = (
	"SSL_RSA_WITH_3DES_EDE_CBC_MD5",
	"SSL_RSA_WITH_IDEA_CBC_MD5",
	"SSL_RSA_WITH_RC4_128_MD5",		# TLS_RSA_WITH_RC4_128_MD5
	"SSL_RSA_WITH_RC2_CBC_MD5",
	"SSL_RSA_WITH_DES_CBC_MD5",
	"SSL_RSA_EXPORT_WITH_RC4_40_MD5",
	"SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5",
);
my %sslv2;

# map OpenSSL cipher name parts to keysig/bulk/mac
# trailing hyphens are needed ("EXP-") because some parts are empty ("")
my $unsupported = "not supported";	# by the Security framework
my @export = (
	"",			# domestic
	"EXP-",		# export
);
my %openssl_to_keysig = (
	""	=> {
		""				=> "RSA",
		"ADH-"			=> "DH_anon",
		"AEAD-"			=> $unsupported,        # placeholder
		"AECDH-"		=> $unsupported,		# placeholder
		"DHE-DSS-"		=> "DHE_DSS",
		"DHE-RSA-"		=> "DHE_RSA",
		"GOST2001-"		=> $unsupported,		# placeholder
		"GOST2012256-"	=> $unsupported,		# placeholder
		"ECDH-ECDSA-"	=> "ECDH_ECDSA",
		"ECDH-RSA-"		=> "ECDH_RSA",
		"ECDHE-ECDSA-"	=> "ECDHE_ECDSA",
		"ECDHE-RSA-"	=> "ECDHE_RSA",
		"ECDHE_PSK-"	=> "ECDHE_PSK",
		"EDH-DSS-"		=> "DHE_DSS",
		"EDH-RSA-"		=> "DHE_RSA",
		"NULL-"			=> "NULL",
		"PSK-"			=> "PSK",
		"SRP-"			=> $unsupported,		# placeholder
		"SRP-DSS-"		=> $unsupported,		# placeholder
		"SRP-RSA-"		=> $unsupported,		# placeholder
	},
	"EXP-"	=> {
		""				=> "RSA_EXPORT",
		"ADH-"			=> "DH_anon_EXPORT",
		"EDH-"			=> "DHE_RSA_EXPORT",
		"EDH-DSS-"		=> "DHE_DSS_EXPORT",
		"EDH-RSA-"		=> "DHE_RSA_EXPORT",
	},
);
my %openssl_to_bulk = (
	""	=> {
		"3DES-EDE-CBC-"	=> "3DES_EDE_CBC",
		"AEAD-CHACHA20-POLY1305-" => $unsupported,        # placeholder
		"AES-128-CBC-"	=> "AES_128_CBC",
		"AES-128-GCM-"	=> "AES_128_GCM",
		"AES-256-CBC-"	=> "AES_256_CBC",
		"AES-256-GCM-"	=> "AES_256_GCM",
		"AES128-"		=> "AES_128_CBC",
		"AES128-CBC-"	=> "AES_128_CBC",
		"AES128-GCM-"	=> "AES_128_GCM",
		"AES256-"		=> "AES_256_CBC",
		"AES256-CBC-"	=> "AES_256_CBC",
		"AES256-GCM-"	=> "AES_256_GCM",
		"CAMELLIA128-"	=> $unsupported,		# placeholder
		"CAMELLIA256-"	=> $unsupported,		# placeholder
		"CHACHA20-"		=> "CHACHA20",
		"DES-CBC-"		=> "DES_CBC",
		"DES-CBC3-"		=> "3DES_EDE_CBC",
		"GOST89-"		=> $unsupported,		# placeholder
		"IDEA-CBC-"		=> "IDEA_CBC",
		"NULL-"			=> "NULL",
		"RC2-CBC-"		=> "RC2_CBC",
		"RC4-"			=> "RC4_128",
		"SEED-"			=> $unsupported,		# placeholder
	},
	"EXP-"	=> {
		"DES-CBC-"		=> "DES40_CBC",
		"RC2-CBC-"		=> "RC2_CBC_40",
		"RC4-"			=> "RC4_40",
	},
);
my %openssl_to_mac = (
	""	=> {
		"GOST89"		=> $unsupported,		# placeholder
		"GOST94"		=> $unsupported,		# placeholder
		"MD5"			=> "MD5",
		"POLY1305"		=> "POLY1305_SHA256",
		"SHA"			=> "SHA",
		"SHA256"		=> "SHA256",
		"SHA384"		=> "SHA384",
		"SHA512"		=> "SHA512",
		"STREEBOG256"	=> $unsupported,		# placeholder
	},
	"EXP-"	=> {
		"MD5"			=> "MD5",
		"SHA"			=> "SHA",
	},
);

# internal consistency checks
$errors = 0;
for my $export (@export) {
	if (!defined($openssl_to_keysig{$export}) ||
		!defined($openssl_to_bulk{$export}) ||
		!defined($openssl_to_mac{$export})) {
		warn("OpenSSL mappings for export \"$export\" undefined.  Update \%openssl_to_* in $prog!\n");
		++$errors;
	}
	for my $keysig (values %{$openssl_to_keysig{$export}}) {
		if ($keysig ne $unsupported && !grep {$_ eq $keysig} @keysig_ordered) {
			warn("Key/signature algorithm \"$keysig\" unknown.  Update \%openssl_to_keysig in $prog!\n");
			++$errors;
		}
	}
	for my $bulk (values %{$openssl_to_bulk{$export}}) {
		if ($bulk ne $unsupported && !grep {$_ eq $bulk} @bulk_ordered) {
			warn("Bulk cipher \"$bulk\" unknown.  Update \%openssl_to_bulk in $prog!\n");
			++$errors;
		}
	}
	for my $mac (values %{$openssl_to_mac{$export}}) {
		if ($mac ne $unsupported && !grep {$_ eq $mac} @mac_ordered) {
			warn("MAC algorithm \"$mac\" unknown.  Update \%openssl_to_mac in $prog!\n");
			++$errors;
		}
	}
}
for my $export (keys %openssl_to_keysig, keys %openssl_to_bulk, keys %openssl_to_mac) {
	if (!grep { $_ eq $export } @export) {
		warn("OpenSSL export \"$export\" unknown.  Update \@export in $prog!\n");
		++$errors;
	}
}
if ($errors) {
	die("$prog: $errors internal consistency botch(es)\n");
}

# make patterns for matching the above unambiguously
my @export_bylength = sort { length($b) <=> length($a) } @export;
my $export_pattern = join("|", @export_bylength);
$export_pattern = qr($export_pattern);
for my $export (@export) {
	@{$openssl_to_keysig{$export . "_bylength"}} = sort { length($b) <=> length($a) } keys %{$openssl_to_keysig{$export}};
	$openssl_to_keysig{$export . "_pattern"} = join("|", @{$openssl_to_keysig{$export . "_bylength"}});
	$openssl_to_keysig{$export . "_pattern"} = qr{$openssl_to_keysig{$export . "_pattern"}};
	@{$openssl_to_bulk{$export . "_bylength"}} = sort { length($b) <=> length($a) } keys %{$openssl_to_bulk{$export}};
	$openssl_to_bulk{$export . "_pattern"} = join("|", @{$openssl_to_bulk{$export . "_bylength"}});
	$openssl_to_bulk{$export . "_pattern"} = qr{$openssl_to_bulk{$export . "_pattern"}};
	@{$openssl_to_mac{$export . "_bylength"}} = sort { length($b) <=> length($a) } keys %{$openssl_to_mac{$export}};
	$openssl_to_mac{$export . "_pattern"} = join("|", @{$openssl_to_mac{$export . "_bylength"}});
	$openssl_to_mac{$export . "_pattern"} = qr{$openssl_to_mac{$export . "_pattern"}};
}

# this list from "man ciphers"
my @openssl_aliases = (
	"3DES",
	"ADH",
	"AES",
	"ALL",
	"CAMELLIA",
	"CHACHA20",
	"COMPLEMENTOFALL",
	"COMPLEMENTOFDEFAULT",
	"DEFAULT",
	"DES",
	"DH",
	[ "EXP",		"EXPORT", ],
	"EXPORT40",
	"EXPORT56",
	"FZA",
	"HIGH",
	"IDEA",
	"LOW",
	"MD5",
	"MEDIUM",
	"RC2",
	"RC4",
	"SEED",
	[ "SHA1",		"SHA", ],
	"SSLv2",
	"SSLv3",
	"TLSv1",
	"TLSv1.2",
	"aDH",
	[ "aDSS",		"DSS", ],
	"aFZA",
	"aNULL",
	"aRSA",
	"eFZA",
	[ "eNULL",		"NULL", ],
	"kDHd",
	"kDHr",
	"kEDH",
	"kFZA",
	[ "kRSA",		"RSA", ],
);

print "Gathering ciphers from Security framework\n" unless $opts{quiet};
my %cipher_values;
my @ciphers;
my %cipher_attrs;
open(CSH, "<", $opts{suiteh}) or die("$prog: $opts{suiteh}: $!\n");
$errors = 0;
my $lineno = 0;
while (my $line = <CSH>) {
	++$lineno;
	$line =~ s,/\*.*?\*/,,g;	# remove C-style comments
	$line =~ s,//.*,,;			# remove C++-style comment
	next unless $line =~ /^{?\s*($cipher_pattern)\s*=\s*(0x[0-9a-fA-F]+)/;
	my $cipher = $1;
	my $value = $2;
	$value =~ tr/a-f/A-F/;
	next if $cipher eq "SSL_NO_SUCH_CIPHERSUITE";

	# some ciphers appear with both SSL_ and TLS_ prefixes
	my $conflict = $cipher_values{$value};
	if (defined($conflict)) {
		my $morph = $conflict;
		next if $morph =~ s/^SSL_/TLS_/ && $morph eq $cipher;
	}
	$cipher_values{$value} = $cipher;

	push @ciphers, $cipher;
	print "line $lineno: $cipher\n" if $opts{debug};

	# As we read the ciphers, classify them
	my $remain = $cipher;
	if ($remain =~ s/^${prefix_pattern}_//) {
		# ok
	} else {
		my $x = ($remain =~ s/^([^_]+)_//) ? $1 : "";
		warn("$opts{suiteh}:$lineno: $cipher: unknown prefix \"$x\".  Update \$prefix_pattern in $prog!\n");
		++$errors;
	}

	my ($strength, $keysig, $bulk, $mac);
	if ($remain =~ s/^($keysig_pattern)_WITH_//) {
		$keysig = $1;
	} else {
		my $x = ($remain =~ s/^(.*)_WITH_//) ? $1 : "";
		warn("$opts{suiteh}:$lineno: $cipher: unknown key/signature algorithm \"$x\".  Update \@keysig_ordered_annotated in $prog!\n");
		++$errors;
		$keysig = "unknown";
	}

	if ($remain =~ s/^($bulk_pattern)_//) {
		$bulk = $1;

		$strength = $bulk_strength{$1};
		$strength .= "_aNULL" if $keysig =~ /anon/ || $keysig eq "NULL";
	} else {
		my $x = ($remain =~ s/^(.*)_//) ? $1 : "";
		warn("$opts{suiteh}:$lineno: $cipher: unknown bulk cipher \"$x\".  Update \@bulk_ordered_annotated in $prog!\n");
		++$errors;
		$bulk = "unknown";
		$strength = "unknown";
	}

	if ($remain =~ s/^($mac_pattern)$//) {
		$mac = $1;
	} else {
		warn("$opts{suiteh}:$lineno: $cipher: unknown MAC algorithm \"$remain\".  Update \@mac_ordered_annotated in $prog!\n");
		++$errors;
		$mac = "unknown";
	}

	$cipher_attrs{$cipher}->{index} = $#ciphers;
	$cipher_attrs{$cipher}->{strength} = $strength;
	$cipher_attrs{$cipher}->{keysig} = $keysig;
	$cipher_attrs{$cipher}->{bulk} = $bulk;
	$cipher_attrs{$cipher}->{mac} = $mac;

	$cipher_attrs{$cipher}->{demoted} = 0;
	for (@strength_ordered_annotated) {
		if ($_->{name} eq $strength) {
			$cipher_attrs{$cipher}->{demoted} |= $_->{demoted};
			last;
		}
	}
	for (@keysig_ordered_annotated) {
		if ($_->{name} eq $keysig) {
			$cipher_attrs{$cipher}->{demoted} |= $_->{demoted};
			last;
		}
	}
	for (@bulk_ordered_annotated) {
		if ($_->{name} eq $bulk) {
			$cipher_attrs{$cipher}->{demoted} |= $_->{demoted};
			last;
		}
	}
	for (@mac_ordered_annotated) {
		if ($_->{name} eq $mac) {
			$cipher_attrs{$cipher}->{demoted} |= $_->{demoted};
			last;
		}
	}

	if (grep { $cipher eq $_ } @sslv2_ordered) {
		$sslv2{$cipher} = 1;
	}
}
close(CSH);
my $ciphers_count = @ciphers;
print "$ciphers_count cipher(s)\n" unless $opts{quiet};
my @ciphers_bylength = sort { length($b) <=> length($a) } @ciphers;
$widths{cipher} = length($ciphers_bylength[0]);

for (@sslv2_ordered) {
	if (!defined($sslv2{$_})) {
		warn("SSLv2 cipher $_ missing from $opts{suiteh}.  Update \@sslv2_ordered in $prog!\n");
		++$errors;
	}
}

# see if Security framework supports something new
for my $export (@export) {
	my %maps = (
		"keysig"	=> \%openssl_to_keysig,
		"bulk"		=> \%openssl_to_bulk,
		"mac"		=> \%openssl_to_mac,
	);
	for my $part ("keysig", "bulk", "mac") {
		my %map = %{$maps{$part}};
		for my $key (keys %{$map{$export}}) {
			if ($map{$export}->{$key} eq $unsupported) {
				my $pattern = $key;
				$pattern = "RSA" if $pattern eq "";
				$pattern =~ s/-$//;
				$pattern =~ tr/a-zA-Z0-9/./c;
				$pattern = qr{$pattern};
				for my $cipher (@ciphers) {
					if ($cipher =~ /$pattern/) {
						warn("Looks like the Security framework now supports $part \"$key\" ($cipher).  Update \%openssl_to_$part in $prog!\n");
						++$errors;
					}
				}
			}
		}
	}
}

if ($errors) {
	die("$prog: $errors error(s)\n");
} else {
	print "OK\n" unless $opts{quiet};
}

print "Determining version of OpenSSL\n" unless $opts{quiet};
my $openssl_version;
open(OCV, "-|", "$opts{openssl} version") or die("$prog: $opts{openssl} version: $!\n");
while (my $line = <OCV>) {
	next unless $line =~ /((OpenSSL|LibreSSL) \S+)/;
	$openssl_version = $1;
}
close(OCV);
if (defined($openssl_version)) {
	print "OK\n" unless $opts{quiet};
} else {
	die("$prog: $opts{openssl} version failed\n");
}

print "Gathering ciphers from OpenSSL\n" unless $opts{quiet};
my %openssl_to_cipher;
open(OCA, "-|", "$opts{openssl} ciphers -v ALL:eNULL") or die("$prog: $opts{openssl} ciphers: $!\n");
$errors = 0;
$lineno = 0;
while (my $line = <OCA>) {
	++$lineno;
	next unless $line =~ /^([A-Z0-9-]+)\s/;
	my $openssl_cipher = $1;
	next if defined $openssl_to_cipher{$openssl_cipher};

	my $remain = $openssl_cipher;
	my ($export, $keysig, $bulk, $mac);
	if ($remain =~ s/^($export_pattern)//) {
		$export = $1;
	} else {
		warn("$opts{openssl} ciphers:$lineno: $openssl_cipher: unknown export \"$remain\".  Update \@export in $prog!\n");
		++$errors;
		next;
	}

	my $openssl_to_keysig_pattern = $openssl_to_keysig{$export . "_pattern"};
	my $openssl_to_bulk_pattern = $openssl_to_bulk{$export . "_pattern"};
	my $openssl_to_mac_pattern = $openssl_to_mac{$export . "_pattern"};

	# since parts may be omitted, parse right-to-left
	if ($remain =~ s/($openssl_to_mac_pattern)$//) {
		$mac = $1;
	} else {
		my $x = ($remain =~ s/-([^-]+)$//) ? $1 : "";
		warn("$opts{openssl} ciphers:$lineno: $openssl_cipher: unknown MAC algorithm \"$x\".  Update \%openssl_to_mac in $prog!\n");
		++$errors;
		next;
	}

	if ($remain =~ s/($openssl_to_bulk_pattern)$//) {
		$bulk = $1;
	} else {
		warn("$opts{openssl} ciphers:$lineno: $openssl_cipher: unknown bulk cipher \"$remain\".  Update \%openssl_to_bulk in $prog!\n");
		++$errors;
		next;
	}

	if ($remain =~ s/^($openssl_to_keysig_pattern)$//) {
		$keysig = $1;
	} else {
		warn("$opts{openssl} ciphers:$lineno: $openssl_cipher: unknown key/signature algorithm \"$remain\".  Update \%openssl_to_keysig in $prog!\n");
		++$errors;
		next;
	}

	# now find the matching Security framework cipher suite
	my $rump = $openssl_to_keysig{$export}->{$keysig} .
		"_WITH_" . $openssl_to_bulk{$export}->{$bulk} .
		"_" . $openssl_to_mac{$export}->{$mac};
	my $cipher_pattern = qr{^${prefix_pattern}_$rump$};
	my @matches = grep(/$cipher_pattern/, @ciphers);
	if (@matches == 0) {
		warn("OpenSSL cipher \"$openssl_cipher\" not supported by Security framework; ignored\n");
	} elsif (@matches == 1) {
		my $cipher = $matches[0];
		$openssl_to_cipher{$openssl_cipher} = $cipher;
		print "line $lineno: $openssl_cipher -> $cipher\n" if $opts{debug};
	} else {
		warn("$opts{openssl} ciphers:$lineno: $openssl_cipher: ambiguous: " . join(", ", @matches) . ".  Update \%openssl_to_* in $prog!\n");
		++$errors;
	}
}
close(OCA);
my $openssl_ciphers_count = keys %openssl_to_cipher;
print "$openssl_ciphers_count cipher(s)\n" unless $opts{quiet};
if ($errors) {
	die("$prog: $errors error(s)\n");
} else {
	print "OK\n" unless $opts{quiet};
}
my @openssl_to_cipher_bylength = sort { length($b) <=> length($a) } keys %openssl_to_cipher;
$widths{openssl_cipher} = length($openssl_to_cipher_bylength[0]);

print "Expanding aliases from OpenSSL\n" unless $opts{quiet};
my %openssl_to_class;
$errors = 0;
for my $openssl_alias (@openssl_aliases) {
	my $alias = ref($openssl_alias) ? @{$openssl_alias}[0] : $openssl_alias;
	next if defined $openssl_to_class{$alias};

	if (defined($openssl_to_cipher{$alias})) {
		warn("OpenSSL alias \"$alias\" has same name as a cipher?!\n");
		++$errors;
	}
	@{$openssl_to_class{$alias}} = ();

	open(OCA, "-|", "$opts{openssl} ciphers -v $alias 2>/dev/null") or die("$prog: $opts{openssl} ciphers: $!\n");
	while (my $line = <OCA>) {
		next unless $line =~ /^([A-Z0-9-]+)\s/;
		my $openssl_cipher = $1;

		my $cipher = $openssl_to_cipher{$openssl_cipher};
		push @{$openssl_to_class{$alias}}, $cipher if defined $cipher and !grep { $_ eq $cipher} @{$openssl_to_class{$alias}};
	}
	close(OCA);
	print "$alias -> " . join(", ", @{$openssl_to_class{$alias}}) . "\n" if $opts{debug};
	if (ref($openssl_alias)) {
		for my $syn (@$openssl_alias) {
			next if $syn eq $alias;
			if (defined($openssl_to_cipher{$syn})) {
				warn("OpenSSL alias \"$syn\" has same name as a cipher?!\n");
				++$errors;
			}
			@{$openssl_to_class{$syn}} = @{$openssl_to_class{$alias}};
			print "$syn == $alias\n" if $opts{debug};
		}
	}
}
my $openssl_classes_count = keys %openssl_to_class;
print "$openssl_classes_count alias(es)\n" unless $opts{quiet};
if ($errors) {
	die("$prog: $errors error(s)\n");
} else {
	print "OK\n" unless $opts{quiet};
}
my @openssl_to_class_bylength = sort { length($b) <=> length($a) } keys %openssl_to_class;
$widths{openssl_class} = length($openssl_to_class_bylength[0]);

my @ciphers_sorted = sort cipher_cmp keys %cipher_attrs;

# remove all references to demoted ciphers (cleverly after setting $widths{cipher})
my @demoted = grep { $cipher_attrs{$_}->{demoted} } @ciphers;
if (@demoted > 0) {
	warn("Skipping demoted cipher $_.\n") for (@demoted);
	@ciphers = grep { !$cipher_attrs{$_}->{demoted} } @ciphers;
	@ciphers_sorted = grep { !$cipher_attrs{$_}->{demoted} } @ciphers_sorted;
	while (my ($openssl_cipher, $cipher) = each %openssl_to_cipher) {
		delete $openssl_to_cipher{$openssl_cipher} if $cipher_attrs{$cipher}->{demoted};
	}
	while (my ($openssl_class, $list) = each %openssl_to_class) {
		@$list = grep { !$cipher_attrs{$_}->{demoted} } @$list;
	}
}

# classify the Security framework ciphers
my %classes;
for my $cipher (@ciphers_sorted) {
	my $strength = $cipher_attrs{$cipher}->{strength};
	my $keysig = $cipher_attrs{$cipher}->{keysig};
	my $bulk = $cipher_attrs{$cipher}->{bulk};
	my $mac = $cipher_attrs{$cipher}->{mac};

	# ALL
	push @{$classes{ALL}}, $cipher;

	# define a reasonable DEFAULT: HIGH plus MEDIUM (keep in sync with $class_description{DEFAULT} below)
	push @{$classes{DEFAULT}}, $cipher if $strength eq "HIGH" || $strength eq "MEDIUM";

	# strength
	push @{$classes{$strength}}, $cipher;

	# keysig
	push @{$classes{$keysig}}, $cipher unless grep { $_ eq $cipher } @{$classes{$keysig}};

	# bulk
	push @{$classes{$bulk}}, $cipher unless grep { $_ eq $cipher } @{$classes{$bulk}};

	# mac
	push @{$classes{$mac}}, $cipher unless grep { $_ eq $cipher } @{$classes{$mac}};

	# aNULL
	push @{$classes{aNULL}}, $cipher if $strength =~ /_aNULL$/;
}
for my $cipher (@sslv2_ordered) {
	# SSLv2
	push @{$classes{SSLv2}}, $cipher;
}
my @classes_bylength = sort { length($b) <=> length($a) } keys %classes;
$widths{class} = length($classes_bylength[0]);

# now print
print "Generating source files\n" unless $opts{quiet};
open(IMPLCOUT, ">", $opts{implcfile}) or die("$prog: $opts{implcfile}: $!\n");
print IMPLCOUT "\n/* Begin code automatically generated by $prog.  Do not edit. */";

my $implhfile;
my $static;
if (defined($opts{implhfile})) {
	open(IMPLHOUT, ">", $opts{implhfile}) or die("$prog: $opts{implhfile}: $!\n");
	$implhfile = $opts{implhfile};
	$implhfile =~ s,.*/,,;
	my $hguard = $implhfile;
	$hguard =~ tr/a-zA-Z0-9_/_/c;
print IMPLHOUT "\n/* Begin code automatically generated by $prog.  Do not edit. */";
print IMPLHOUT "\n";
print IMPLHOUT <<EOT;
#ifndef $opts{implprefix}_$hguard
#define $opts{implprefix}_$hguard
EOT
	print IMPLCOUT "\n#include \"$implhfile\"";
	print IMPLCOUT "\n";

	$static = "";
	*SOUT = \*IMPLHOUT;		# struct definitions to implhfile
} else {
	$static = "static ";
	*SOUT = \*IMPLCOUT;		# struct definitions to implcfile
}
print SOUT <<EOT;

#include <Security/CipherSuite.h>

/* Map from a name to a single cipher */
struct $opts{implprefix}Cipher {
	const char *name;
	SSLCipherSuite cipher;
	const CFStringRef *keysig;
	const CFStringRef *bulk;
	const CFStringRef *mac;
	const CFStringRef *strength;
	int strbits;			/* bits of effective strength */
	int algbits;			/* bits the algorithm is capable of */
	unsigned int export:1;
};

/* Map from a name to a set of ciphers */
struct $opts{implprefix}CipherSet {
	const char *name;
	const SSLCipherSuite *ciphers;
	const size_t count;
};
EOT
undef *SOUT;

# first the Security framework names
if ($implhfile) {
	print IMPLHOUT "\n/* Ciphers known to the Security framework */";
	print IMPLHOUT "\nextern const struct $opts{implprefix}Cipher $opts{implprefix}Ciphers[];";
	print IMPLHOUT "\nextern const size_t $opts{implprefix}CiphersCount;";
	print IMPLHOUT "\n";
}

my @properties_ordered = (
	{ name => "Name" },
	{ name => "Keysig",		properties => \@keysig_ordered },
	{ name => "Bulk",		properties => \@bulk_ordered },
	{ name => "MAC",		properties => \@mac_ordered },
	{ name => "Strength",	properties => \@strength_ordered },
	{ name => "StrengthBits" },
	{ name => "AlgBits" },
	{ name => "Export" },
);
my %propnames;
my @propsymbols;
for my $property (@properties_ordered) {
	print IMPLCOUT "\n/* Cipher property constants for k$opts{pubprefix}CipherProperty$property->{name} */";
	print IMPLCOUT "\nconst CFStringRef k$opts{pubprefix}CipherProperty$property->{name} = CFSTR(\"k$opts{pubprefix}CipherProperty$property->{name}\");";
	$propnames{$property->{name}} = 1;
	push @propsymbols, $property->{name};
	if (exists($property->{properties})) {
		for (sort @{$property->{properties}}) {
			if (defined($propnames{$_})) {
				print IMPLCOUT "\n/* k$opts{pubprefix}CipherProperty$_ defined above */";
			} else {
				print IMPLCOUT "\nconst CFStringRef k$opts{pubprefix}CipherProperty$_ = CFSTR(\"k$opts{pubprefix}CipherProperty$_\");";
				$propnames{$_} = 1;
				push @propsymbols, $_;
			}
		}
	}
	print IMPLCOUT "\n";
}

print IMPLCOUT "\n/* Ciphers known to the Security framework */";
print IMPLCOUT "\n${static}const struct $opts{implprefix}Cipher $opts{implprefix}Ciphers[] = {";
my $first = 1;
my @colwidths_Cipher = (
	0,
	3 + $widths{cipher} + 2,
	$widths{cipher} + 1,
	2 + length($opts{implprefix}) + 14 + $widths{keysig} + 1,
	2 + length($opts{implprefix}) + 14 + $widths{bulk} + 1,
	2 + length($opts{implprefix}) + 14 + $widths{mac} + 1,
	2 + length($opts{implprefix}) + 14 + $widths{strength} + 1,
	$widths{bulk_strbits} + 1,
	$widths{bulk_algbits} + 1,
	1
);
for my $cipher (@ciphers) {
	print IMPLCOUT "," unless $first;
	$first = 0;
	print IMPLCOUT "\n";

	my @coldata = (
		"",
		"{ \"$cipher\",",
		"$cipher,",
		"&k$opts{pubprefix}CipherProperty$cipher_attrs{$cipher}->{keysig},",
		"&k$opts{pubprefix}CipherProperty$cipher_attrs{$cipher}->{bulk},",
		"&k$opts{pubprefix}CipherProperty$cipher_attrs{$cipher}->{mac},",
		"&k$opts{pubprefix}CipherProperty$cipher_attrs{$cipher}->{strength},",
		"$bulk_strbits{$cipher_attrs{$cipher}->{bulk}},",
		"$bulk_algbits{$cipher_attrs{$cipher}->{bulk}},",
		$cipher_attrs{$cipher}->{strength} =~ /export/i ? 1 : 0
	);
	format_row(\*IMPLCOUT, \@colwidths_Cipher, \@coldata);
	print IMPLCOUT "}";
}
print IMPLCOUT "\n};";
print IMPLCOUT "\n${static}const size_t $opts{implprefix}CiphersCount = sizeof $opts{implprefix}Ciphers / sizeof $opts{implprefix}Ciphers[0];";
print IMPLCOUT "\n";

# then the demoted ciphers
print IMPLCOUT "\n/* Demoted ciphers */";
print IMPLCOUT "\n${static}const struct $opts{implprefix}Cipher $opts{implprefix}Ciphers_Demoted[] = {";
$first = 1;
for my $cipher (@demoted) {
	print IMPLCOUT "," unless $first;
	$first = 0;
	print IMPLCOUT "\n";

	my @coldata = (
		"",
		"{ \"$cipher\",",
		"$cipher,",
		"&k$opts{pubprefix}CipherProperty$cipher_attrs{$cipher}->{keysig},",
		"&k$opts{pubprefix}CipherProperty$cipher_attrs{$cipher}->{bulk},",
		"&k$opts{pubprefix}CipherProperty$cipher_attrs{$cipher}->{mac},",
		"&k$opts{pubprefix}CipherProperty$cipher_attrs{$cipher}->{strength},",
		"$bulk_strbits{$cipher_attrs{$cipher}->{bulk}},",
		"$bulk_algbits{$cipher_attrs{$cipher}->{bulk}},",
		$cipher_attrs{$cipher}->{strength} =~ /export/i ? 1 : 0
	);
	format_row(\*IMPLCOUT, \@colwidths_Cipher, \@coldata);
	print IMPLCOUT "}";
}
print IMPLCOUT "\n};";
print IMPLCOUT "\n${static}const size_t $opts{implprefix}CiphersCount_Demoted = sizeof $opts{implprefix}Ciphers_Demoted / sizeof $opts{implprefix}Ciphers_Demoted[0];";
print IMPLCOUT "\n";

# then the Security framework classifications
for my $class (sort keys %classes) {
	my $name = $class;
	$name =~ tr/a-zA-Z0-9/_/c;
	if ($implhfile) {
		print IMPLHOUT "\n/* Security framework ciphers of class $class */";
		print IMPLHOUT "\nextern const SSLCipherSuite $opts{implprefix}CipherClass_${name}[];";
		print IMPLHOUT "\nextern const size_t $opts{implprefix}CipherClassCount_${name};";
		print IMPLHOUT "\n";
	}

	print IMPLCOUT "\n/* Security framework ciphers of class $class */";
	print IMPLCOUT "\n${static}const SSLCipherSuite $opts{implprefix}CipherClass_${name}[] = {";
	$first = 1;
	for my $cipher (@{$classes{$class}}) {
		print IMPLCOUT "," unless $first;
		$first = 0;
		print IMPLCOUT "\n\t$cipher";
	}
	print IMPLCOUT "\n};";
	print IMPLCOUT "\n${static}const size_t $opts{implprefix}CipherClassCount_${name} = sizeof $opts{implprefix}CipherClass_$name / sizeof $opts{implprefix}CipherClass_${name}[0];";
	print IMPLCOUT "\n";
}

# then the Security framework sets
if ($implhfile) {
	print IMPLHOUT "\n/* Security framework cipher sets */";
	print IMPLHOUT "\nextern const struct $opts{implprefix}CipherSet $opts{implprefix}CipherSets[];";
	print IMPLHOUT "\nextern const size_t $opts{implprefix}CipherSetsCount;";
	print IMPLHOUT "\n";
}

print IMPLCOUT "\n/* Security framework cipher sets */";
print IMPLCOUT "\n${static}const struct $opts{implprefix}CipherSet $opts{implprefix}CipherSets[] = {";
$first = 1;
my @colwidths_Set = (
	0,
	3 + $widths{class} + 2,
	length($opts{implprefix}) + 6 + $widths{class} + 1,
	7 + length($opts{implprefix}) + 6 + $widths{class} + 10 + length($opts{implprefix}) + 6 + $widths{class} + 3
);
for my $class (sort keys %classes) {
	print IMPLCOUT "," unless $first;
	$first = 0;
	print IMPLCOUT "\n";

	my @coldata = (
		"",
		"{ \"$class\",",
		"$opts{implprefix}CipherClass_$class,",
		"sizeof $opts{implprefix}CipherClass_$class / sizeof $opts{implprefix}CipherClass_${class}[0]"
	);
	format_row(\*IMPLCOUT, \@colwidths_Set, \@coldata);
	print IMPLCOUT "}";
}
print IMPLCOUT "\n};";
print IMPLCOUT "\n${static}const size_t $opts{implprefix}CipherSetsCount = sizeof $opts{implprefix}CipherSets / sizeof $opts{implprefix}CipherSets[0];";
print IMPLCOUT "\n";

# then the OpenSSL names
if ($implhfile) {
	print IMPLHOUT "\n/* Ciphers known to $openssl_version */";
	print IMPLHOUT "\nextern const struct $opts{implprefix}Cipher $opts{implprefix}CiphersOpenSSL[];";
	print IMPLHOUT "\nextern const size_t $opts{implprefix}CiphersOpenSSLCount;";
	print IMPLHOUT "\n";
}

print IMPLCOUT "\n/* Ciphers known to $openssl_version */";
print IMPLCOUT "\n${static}const struct $opts{implprefix}Cipher $opts{implprefix}CiphersOpenSSL[] = {";
$first = 1;
my @colwidths_OpenSSL_Cipher = (
	0,
	3 + $widths{openssl_cipher} + 2,
	$widths{cipher} + 1,
	2 + length($opts{implprefix}) + 14 + $widths{keysig} + 1,
	2 + length($opts{implprefix}) + 14 + $widths{bulk} + 1,
	2 + length($opts{implprefix}) + 14 + $widths{mac} + 1,
	2 + length($opts{implprefix}) + 14 + $widths{strength} + 1,
	$widths{bulk_strbits} + 1,
	$widths{bulk_algbits} + 1,
	1
);
for my $openssl_cipher (sort keys %openssl_to_cipher) {
	print IMPLCOUT "," unless $first;
	$first = 0;
	print IMPLCOUT "\n";

	my $cipher = $openssl_to_cipher{$openssl_cipher};
	my @coldata = (
		"",
		"{ \"$openssl_cipher\",",
		"$cipher,",
		"&k$opts{pubprefix}CipherProperty$cipher_attrs{$cipher}->{keysig},",
		"&k$opts{pubprefix}CipherProperty$cipher_attrs{$cipher}->{bulk},",
		"&k$opts{pubprefix}CipherProperty$cipher_attrs{$cipher}->{mac},",
		"&k$opts{pubprefix}CipherProperty$cipher_attrs{$cipher}->{strength},",
		"$bulk_strbits{$cipher_attrs{$cipher}->{bulk}},",
		"$bulk_algbits{$cipher_attrs{$cipher}->{bulk}},",
		$cipher_attrs{$cipher}->{strength} =~ /export/i ? 1 : 0
	);
	format_row(\*IMPLCOUT, \@colwidths_OpenSSL_Cipher, \@coldata);
	print IMPLCOUT "}";
}
print IMPLCOUT "\n};";
print IMPLCOUT "\n${static}const size_t $opts{implprefix}CiphersOpenSSLCount = sizeof $opts{implprefix}CiphersOpenSSL / sizeof $opts{implprefix}CiphersOpenSSL[0];";
print IMPLCOUT "\n";

# then the OpenSSL classes
for my $class (sort keys %openssl_to_class) {
	my $name = $class;
	$name =~ tr/a-zA-Z0-9/_/c;
	if ($implhfile) {
		print IMPLHOUT "\n/* OpenSSL ciphers of class $class */";
		print IMPLHOUT "\nextern const SSLCipherSuite $opts{implprefix}CipherOpenSSLClass_${name}[];";
		print IMPLHOUT "\nextern const size_t $opts{implprefix}CipherOpenSSLClassCount_${name};";
		print IMPLHOUT "\n";
	}

	print IMPLCOUT "\n/* OpenSSL ciphers of class $class */";
	print IMPLCOUT "\n${static}const SSLCipherSuite $opts{implprefix}CipherOpenSSLClass_${name}[] = {";
	$first = 1;
	for my $cipher (@{$openssl_to_class{$class}}) {
		print IMPLCOUT "," unless $first;
		$first = 0;
		print IMPLCOUT "\n\t$cipher";
	}
	print IMPLCOUT "\n};";
	print IMPLCOUT "\n${static}const size_t $opts{implprefix}CipherOpenSSLClassCount_${name} = sizeof $opts{implprefix}CipherOpenSSLClass_$name / sizeof $opts{implprefix}CipherOpenSSLClass_${name}[0];";
	print IMPLCOUT "\n";
}

# finally the OpenSSL sets
if ($implhfile) {
	print IMPLHOUT "\n/* OpenSSL cipher sets */";
	print IMPLHOUT "\nextern const struct $opts{implprefix}CipherSet $opts{implprefix}CipherOpenSSLSets[];";
	print IMPLHOUT "\nextern const size_t $opts{implprefix}CipherOpenSSLSetsCount;";
	print IMPLHOUT "\n";
}

print IMPLCOUT "\n/* OpenSSL cipher sets */";
print IMPLCOUT "\n${static}const struct $opts{implprefix}CipherSet $opts{implprefix}CipherOpenSSLSets[] = {";
$first = 1;
my @colwidths_OpenSSL_Set = (
	0,
	3 + $widths{openssl_class} + 2,
	length($opts{implprefix}) + 13 + $widths{openssl_class} + 1,
	7 + length($opts{implprefix}) + 13 + $widths{openssl_class} + 10 + length($opts{implprefix}) + 13 + $widths{openssl_class} + 3
);
for my $class (sort keys %openssl_to_class) {
	print IMPLCOUT "," unless $first;
	$first = 0;
	print IMPLCOUT "\n";

	my $name = $class;
	$name =~ tr/a-zA-Z0-9/_/c;
	my @coldata = (
		"",
		"{ \"$class\",",
		"$opts{implprefix}CipherOpenSSLClass_$name,",
		"sizeof $opts{implprefix}CipherOpenSSLClass_$name / sizeof $opts{implprefix}CipherOpenSSLClass_${name}[0]"
	);
	format_row(\*IMPLCOUT, \@colwidths_OpenSSL_Set, \@coldata);
	print IMPLCOUT "}";
}
print IMPLCOUT "\n};";
print IMPLCOUT "\n${static}const size_t $opts{implprefix}CipherOpenSSLSetsCount = sizeof $opts{implprefix}CipherOpenSSLSets / sizeof $opts{implprefix}CipherOpenSSLSets[0];";
print IMPLCOUT "\n";

if ($implhfile) {
	print IMPLHOUT "\n#endif";
	print IMPLHOUT "\n/* End code automatically generated by $prog. */";
	print IMPLHOUT "\n";
}

print IMPLCOUT "\n/* End code automatically generated by $prog. */";
print IMPLCOUT "\n";

close(IMPLHOUT) if $implhfile;
close(IMPLCOUT);

print "OK\n" unless $opts{quiet};

if (defined($opts{pubhfile})) {
	print "Updating public header file\n" unless $opts{quiet};

	my %class_description;
	$class_description{$_->{name}} = $_->{desc} for @strength_ordered_annotated;
	$class_description{$_->{name}} = $_->{desc} for @keysig_ordered_annotated;
	$class_description{$_->{name}} = $_->{desc} for @bulk_ordered_annotated;
	$class_description{$_->{name}} = $_->{desc} for @mac_ordered_annotated;
	$class_description{ALL} = "All supported ciphers";
	$class_description{DEFAULT} = "All high and medium strength ciphers";
	$class_description{SSLv2} = "All SSLv2 ciphers";
	$class_description{aNULL} = "All ciphers which use anonymous key exchange";

	my $pubhin = $opts{pubhfile};
	my $pubhout = $pubhin . ".new";
	open(PUBHIN, "<", $pubhin) or die("$prog: $pubhin: $!\n");
	open(PUBHOUT, ">", $pubhout) or die("$prog: $pubhout: $!\n");
	my $replaced = 0;
	undef %propnames;
	while (my $line = <PUBHIN>) {
		if ($line =~ /^(.*)== AUTOMATICALLY GENERATED TABLE OF SETS GOES HERE ==/) {
			my $prefix = $1;

			# header line
			print PUBHOUT "$prefix<table>\n";
			print PUBHOUT "$prefix<tr><th align=\"left\">Name</th><th align=\"left\">Value</th></tr>\n";

			# classes
			for my $class (sort keys %classes) {
				print PUBHOUT "$prefix<tr><td>$class</td><td>$class_description{$class}</td></tr>\n";
			}

			# end table
			print PUBHOUT "$prefix</table>\n";

			++$replaced;
		} elsif ($line =~ /== AUTOMATICALLY GENERATED (\S+) STRINGS GO HERE ==/) {
			my $name = $1;

			my $property;
			for (@properties_ordered) {
				if ($_->{name} eq $name) {
					$property = $_;
					last;
				}
			}

			if (defined($property)) {
				for (sort @{$property->{properties}}) {
					if (defined($propnames{$_})) {
						print PUBHOUT "/* k$opts{pubprefix}CipherProperty$_ defined above */\n";
					} else {
						print PUBHOUT "extern const CFStringRef k$opts{pubprefix}CipherProperty$_;\n";
						$propnames{$_} = 1;
					}
				}
				++$replaced;
			} else {
				unlink($pubhout);
				die("$prog: unknown property template $name\n");
			}
		} else {
			print PUBHOUT $line;
		}
	}
	close(PUBHOUT);
	close(PUBHIN);
	if ($replaced == 5) {
		rename($pubhout, $pubhin) or die("$prog: rename $pubhout -> $pubhin: $!\n");
		print "OK\n" unless $opts{quiet};
	} else {
		unlink($pubhout);
		die("$prog: template(s) missing from $pubhin\n");
	}
}

if (defined($opts{pubefile})) {
	print "Updating public exported-symbols file\n" unless $opts{quiet};

	my $pubein = $opts{pubefile};
	my $pubeout = $pubein . ".new";
	open(PUBEIN, "<", $pubein) or die("$prog: $pubein: $!\n");
	open(PUBEOUT, ">", $pubeout) or die("$prog: $pubeout: $!\n");
	my $replaced = 0;
	while (my $line = <PUBEIN>) {
		if ($line =~ /== AUTOMATICALLY GENERATED PUBLIC CIPHER SYMBOLS GO HERE ==/) {
			print PUBEOUT "_k$opts{pubprefix}CipherProperty$_\n" for (sort @propsymbols);
			$replaced = 1;
		} else {
			print PUBEOUT $line;
		}
	}
	close(PUBEOUT);
	close(PUBEIN);
	if ($replaced) {
		rename($pubeout, $pubein) or die("$prog: rename $pubeout -> $pubein: $!\n");
		print "OK\n" unless $opts{quiet};
	} else {
		unlink($pubeout);
		die("$prog: template missing from $pubein\n");
	}
}

exit 0;

sub cipher_cmp
{
	my $strengtha = $cipher_attrs{$a}->{strength};
	my $strengthb = $cipher_attrs{$b}->{strength};
	my $keysiga = $cipher_attrs{$a}->{keysig};
	my $keysigb = $cipher_attrs{$b}->{keysig};
	my $bulka = $cipher_attrs{$a}->{bulk};
	my $bulkb = $cipher_attrs{$b}->{bulk};
	my $maca = $cipher_attrs{$a}->{mac};
	my $macb = $cipher_attrs{$b}->{mac};

	# sort strongest-to-weakest
	return $strength_pri{$strengtha} <=> $strength_pri{$strengthb} ||
		   $bulk_pri{$bulka} <=> $bulk_pri{$bulkb} ||
		   $mac_pri{$maca} <=> $mac_pri{$macb} ||
		   $keysig_pri{$keysiga} <=> $keysig_pri{$keysigb};
}

sub format_row
{
	my $fhref = shift or die;
	my $widthref = shift or die;
	my $dataref = shift or die;

	my @widths = @$widthref;
	my @data = @$dataref;
	die unless scalar(@widths) == scalar(@data);

	for my $i (0..$#widths) {
		print $fhref $data[$i];
		my $width += length($data[$i]);
		# colums start on tab boundaries
		while ($width <= $widths[$i]) {
			print $fhref "\t";
			$width += $opts{tab} - ($width % $opts{tab});
		}
	}
}
