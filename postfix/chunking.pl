#!/usr/bin/perl -w

# Test Apple's BDAT/CHUNKING/BINARYMIME extension to postfix.

# Copyright (c) 2013 Apple Inc.  All Rights Reserved.
# 
# @APPLE_LICENSE_HEADER_START@
# 
# This file contains Original Code and/or Modifications of Original Code
# as defined in and that are subject to the Apple Public Source License
# Version 2.0 (the 'License').  You may not use this file except in
# compliance with the License.  Please obtain a copy of the License at
# http://www.opensource.apple.com/apsl/ and read it before using this
# file.
# 
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  Please
# see the License for the specific language governing rights and
# limitations under the License.
# 
# @APPLE_LICENSE_HEADER_END@

use strict;
use IO::Socket::INET;
use Getopt::Long;
use IPC::Open3;
use Digest::HMAC_MD5;
use APR::Base64;
use List::Util 'shuffle';
use MIME::QuotedPrint;
use feature 'state';

sub usage
{
	die <<EOT;
Usage: $0 --host smtp+imap-server --user name --password pw
Options:
	--bufsiz n	output buffer size
	--buftag	tag output buffer flushes
	--debug
	--deliveries n	number of messages to deliver and check
	--light		don't test with random binary gibberish
	--test		test message generation
	--quiet
	--verbose
EOT
}

my %opts;
GetOptions(\%opts,
    'bufsiz=i',
    'buftag',
    'debug',
    'deliveries=i',
    'host=s',
    'light',
    'password=s',
    'quiet',
    'test',
    'user=s',
    'verbose',
) || usage();

$opts{deliveries} = 1000 unless defined($opts{deliveries});
usage() unless $opts{deliveries} > 0;
usage() unless $opts{host};
if ($opts{host} !~ /\./) {
	print STDERR "Warning: --host $opts{host} is not fully-qualified and probably won't work.\n";
}
usage() unless $opts{user};
usage() unless $opts{password};

$| = 1;

my ($smtppid, $imappid);
local $SIG{__DIE__} = sub {
	kill(9, $smtppid) if defined $smtppid;
	kill(9, $imappid) if defined $imappid;
};

my $reply;

my %typefuncs = (
    ""				=> [\&header_plain,	\&body_plain],
    "text/plain"		=> [\&header_plain,	\&body_plain],
    "message/rfc822"		=> [\&header_message,	\&body_message],
    "application/octet-stream"	=> [\&header_gibberish,	\&body_gibberish],
    "multipart/mixed"		=> [\&header_mixed,	\&body_mixed],
);
if ($opts{light}) {
	$typefuncs{"application/octet-stream"} = $typefuncs{"text/plain"};
}
my @types = keys %typefuncs;

my @encodings_top = ("", "7bit", "8bit", "binary");
my @encodings_sub = (@encodings_top, "base64", "quoted-printable");
my %encodingfuncs = (
    ""				=> \&clean_7bit,
    "7bit"			=> \&clean_7bit,
    "8bit"			=> \&clean_8bit,
    "binary"			=> \&clean_binary,
    "base64"			=> \&clean_base64,
    "quoted-printable"		=> \&clean_qp,
);

my $top_encoding;

if ($opts{test}) {
	my ($raw, $clean, $rawsections, $cleansections, $fetchable) = message("body=binarymime", "test");
	my @rawsections = @$rawsections;
	my @cleansections = @$cleansections;
	my @fetchable = @$fetchable;
	print "=== RAW ===\n$raw".
	    "\n=== CLEAN ===\n$clean".
	    "\n=== RAW SECTIONS ===\n".join("//\n",@rawsections).
	    "\n=== CLEAN SECTIONS ===\n".join("//\n",@cleansections).
	    "\n=== FETCHABLE ===\n".join("//\n",@fetchable).
	    "\n=== END ===\n";
	my $sanity = "";
	$sanity .= $_ for @rawsections;
	die "Internal consistency botch: sectioned message does not match whole.\nRaw:\n$raw\nSectioned:\n$sanity\n"
		unless $sanity eq $raw;
	$sanity = "";
	$sanity .= $_ for @cleansections;
	die "Internal consistency botch: sectioned message does not match whole.\nClean:\n$clean\nSectioned:\n$sanity\n"
		unless $sanity eq $clean;
	open RAW, ">/tmp/chunking.raw" or die;
	print RAW $raw;
	close RAW;
	open CLEAN, ">/tmp/chunking.clean" or die;
	print CLEAN $clean;
	close CLEAN;
	open RAWSECTIONS, ">/tmp/chunking.rawsections" or die;
	print RAWSECTIONS join("//\n",@rawsections);
	close RAWSECTIONS;
	open CLEANSECTIONS, ">/tmp/chunking.cleansections" or die;
	print CLEANSECTIONS join("//\n",@cleansections);
	close CLEANSECTIONS;
	open FETCHABLE, ">/tmp/chunking.fetchable" or die;
	print FETCHABLE join("//\n",@fetchable);
	close FETCHABLE;
	#system("xdiff -a /tmp/chunking.raw /tmp/chunking.clean /tmp/chunking.cleansections");
	exit 0;
}

# try connecting via imaps, imap + starttls, imap, in that order
my ($to_imap, $from_imap);
print "connecting (imaps)...\n" unless $opts{quiet};
my @imapargv = ("/usr/bin/openssl", "s_client", "-ign_eof",
		"-connect", "$opts{host}:imaps");
push @imapargv, "-quiet" unless $opts{verbose};
$imappid = open3(\*TO_IMAP, \*FROM_IMAP, \*FROM_IMAP, @imapargv);
sub openssl_imap_happy_or_clean_up
{
	my $label = shift or die;

	if (!defined($imappid)) {
		print "$label: couldn't run openssl: $!\n" if $opts{verbose};
	} else {
		while ($reply = <FROM_IMAP>) {
			print "<OPENSSL< $reply" if $opts{verbose};
			$reply =~ s/[\r\n]+$//;
			return 1 if $reply =~ /^\S+ OK /;
			if ($reply =~ /^connect:/i || $reply =~ /errno/) {
				print "$label: $reply\n" if $opts{verbose};
				last;
			}
		}
		if (!defined($reply)) {
			print "$label: EOF\n" if $opts{verbose};
		}
	}

	close(TO_IMAP);
	close(FROM_IMAP);
	if (defined($imappid)) {
		kill(9, $imappid);
		waitpid($imappid, 0);
		undef $imappid;
	}
	return 0;
}
if (openssl_imap_happy_or_clean_up("$opts{host}:imaps")) {
	$to_imap = IO::Handle->new_from_fd(*TO_IMAP, "w");
	$from_imap = IO::Handle->new_from_fd(*FROM_IMAP, "r");
	if (!defined($to_imap) || !defined($from_imap)) {
		die "IO::Handle.new_from_fd: $!\n";
	}
} else {
	print "connecting (imap + starttls)...\n" unless $opts{quiet};
	@imapargv = ("/usr/bin/openssl", "s_client", "-ign_eof",
		     "-connect", "$opts{host}:imap", "-starttls", "imap");
	push @imapargv, "-quiet" unless $opts{verbose};
	$imappid = open3(\*TO_IMAP, \*FROM_IMAP, \*FROM_IMAP, @imapargv);
	if (openssl_imap_happy_or_clean_up("$opts{host}:imap + starttls")) {
		$to_imap = IO::Handle->new_from_fd(*TO_IMAP, "w");
		$from_imap = IO::Handle->new_from_fd(*FROM_IMAP, "r");
		if (!defined($to_imap) || !defined($from_imap)) {
			die "IO::Handle.new_from_fd: $!\n";
		}
	} else {
		print "connecting (imap)...\n" unless $opts{quiet};
		$to_imap = IO::Socket::INET->new(
		    PeerAddr	=> $opts{host},
		    PeerPort	=> 'imap(143)',
		    Proto	=> 'tcp',
		    Type	=> SOCK_STREAM,
		    Timeout	=> 30,
		);
		$from_imap = $to_imap;
		if (!defined($to_imap) || !defined($from_imap)) {
			die "IO::Socket::INET.new: $!\n";
		}

		$reply = $from_imap->getline();
		die "I/O error\n" if $from_imap->error;
		imap_printS($reply) if $opts{verbose};
		$reply =~ s/[\r\n]+$//;
		if ($reply !~ /\* OK (\[.*\] )?Dovecot.* ready\./) {
			die "Bad greeting: <$reply>\n";
		}
	}
}
$to_imap->autoflush(1);

print "capability...\n" unless $opts{quiet};
imap_send_data("c capability\r\n");
imap_flush();
my $imap_auth_plain = 0;
my $imap_auth_cram_md5 = 0;
while ($reply = $from_imap->getline()) {
	imap_printS($reply) if $opts{verbose};
	$reply =~ s/[\r\n]+$//;
	if ($reply =~ /^c /) {
		if ($reply !~ /c OK (\[.*\])?/) {
			die "Capability failed: <$reply>\n";
		}
		last;
	}
	$imap_auth_plain = 1 if $reply =~ /CAPABILITY.*AUTH=PLAIN/i;
	$imap_auth_cram_md5 = 1 if $reply =~ /CAPABILITY.*AUTH=CRAM-MD5/i;
}
die "I/O error\n" if $from_imap->error;
if (!$imap_auth_plain && !$imap_auth_cram_md5) {
	die "$opts{host} supports neither PLAIN nor CRAM-MD5 auth so I don't know how to log in.\n";
}

print "logging in...\n" unless $opts{quiet};
my $imap_auth = $imap_auth_cram_md5 ? "CRAM-MD5" : "PLAIN";
imap_send_data("a authenticate $imap_auth\r\n");
imap_flush();
$reply = $from_imap->getline();
die "I/O error\n" if $from_imap->error;
imap_printS($reply) if $opts{verbose};
$reply =~ s/[\r\n]+$//;
if ($reply !~ /^\+/) {
	die "Authenticate failed: <$reply>\n";
}
if ($imap_auth_cram_md5) {
	my ($challenge) = ($reply =~ /^\+ (.*)/);
	$challenge = APR::Base64::decode($challenge);
	print "Decoded challenge: $challenge\n" if $opts{verbose};
	my $digest = Digest::HMAC_MD5::hmac_md5_hex($challenge, $opts{password});
	$imap_auth = APR::Base64::encode("$opts{user} $digest");
} else {
	$imap_auth = APR::Base64::encode("\0$opts{user}\0$opts{password}");
}
$imap_auth .= "\r\n";
imap_send_data($imap_auth);
imap_flush();
while ($reply = $from_imap->getline()) {
	imap_printS($reply) if $opts{verbose};
	$reply =~ s/[\r\n]+$//;
	if ($reply =~ /^a /) {
		if ($reply !~ /a OK /) {
			die "Login failed: <$reply>\n";
		}
		last;
	}
}
die "I/O error\n" if $from_imap->error;

print "create scratchbox...\n" unless $opts{quiet};
imap_send_data("b create scratchbox\r\n");
imap_flush();
while ($reply = $from_imap->getline()) {
	imap_printS($reply) if $opts{verbose};
	$reply =~ s/[\r\n]+$//;
	if ($reply =~ /^b /) {
		last;
	}
}
die "I/O error\n" if $from_imap->error;

print "select...\n" unless $opts{quiet};
imap_send_data("c select inbox\r\n");
imap_flush();
my $inbox_message_count;
while ($reply = $from_imap->getline()) {
	imap_printS($reply) if $opts{verbose};
	$reply =~ s/[\r\n]+$//;
	if ($reply =~ /^c OK /) {
		last;
	} elsif ($reply =~ /^\* (\d+) EXISTS/i) {
		$inbox_message_count = $1;
	}
}
die "I/O error\n" if $from_imap->error;

print "idle...\n" unless $opts{quiet};
imap_send_data("i idle\r\n");
imap_flush();
my $imap_idle = 0;
while ($reply = $from_imap->getline()) {
	imap_printS($reply) if $opts{verbose};
	$reply =~ s/[\r\n]+$//;
	if ($reply =~ /^\+ /) {
		$imap_idle = 1;
		last;
	} elsif ($reply =~ /^i /) {
		die "Idle failed: <$reply>\n";
	}
}
die "I/O error\n" if $from_imap->error;

# try connecting via submission + starttls, smtp + starttls, smtp, in that order
my $submission = 0;
my ($to_smtp, $from_smtp);
print "connecting (submission + starttls)...\n" unless $opts{quiet};
my @smtpargv = ("/usr/bin/openssl", "s_client", "-ign_eof",
	     "-connect", "$opts{host}:submission", "-starttls", "smtp");
push @smtpargv, "-quiet" unless $opts{verbose};
$smtppid = open3(\*TO_SMTP, \*FROM_SMTP, \*FROM_SMTP, @smtpargv);
sub openssl_smtp_happy_or_clean_up
{
	my $label = shift or die;

	if (!defined($smtppid)) {
		print "$label: couldn't run openssl: $!\n" if $opts{verbose};
	} else {
		while ($reply = <FROM_SMTP>) {
			print "<OPENSSL< $reply" if $opts{verbose};
			$reply =~ s/[\r\n]+$//;
			return 1 if $reply =~ /^250 /;
			if ($reply =~ /^connect:/i || $reply =~ /errno/) {
				print "$label: $reply\n" if $opts{verbose};
				last;
			}
		}
		if (!defined($reply)) {
			print "$label: EOF\n" if $opts{verbose};
		}
	}

	close(TO_SMTP);
	close(FROM_SMTP);
	if (defined($smtppid)) {
		kill(9, $smtppid);
		waitpid($smtppid, 0);
		undef $smtppid;
	}
	return 0;
}
if (openssl_smtp_happy_or_clean_up("$opts{host}:submission + starttls")) {
	$to_smtp = IO::Handle->new_from_fd(*TO_SMTP, "w");
	$from_smtp = IO::Handle->new_from_fd(*FROM_SMTP, "r");
	if (!defined($to_smtp) || !defined($from_smtp)) {
		die "IO::Handle.new_from_fd: $!\n";
	}
	$submission = 1;
} else {
	print "connecting (smtp + starttls)...\n" unless $opts{quiet};
	@smtpargv = ("/usr/bin/openssl", "s_client", "-ign_eof",
		     "-connect", "$opts{host}:smtp", "-starttls", "smtp");
	push @smtpargv, "-quiet" unless $opts{verbose};
	$smtppid = open3(\*TO_SMTP, \*FROM_SMTP, \*FROM_SMTP, @smtpargv);
	if (openssl_smtp_happy_or_clean_up("$opts{host}:smtp + starttls")) {
		$to_smtp = IO::Handle->new_from_fd(*TO_SMTP, "w");
		$from_smtp = IO::Handle->new_from_fd(*FROM_SMTP, "r");
		if (!defined($to_smtp) || !defined($from_smtp)) {
			die "IO::Handle.new_from_fd: $!\n";
		}
	} else {
		print "connecting (smtp)...\n" unless $opts{quiet};
		$to_smtp = IO::Socket::INET->new(
		    PeerAddr	=> $opts{host},
		    PeerPort	=> 'smtp(25)',
		    Proto	=> 'tcp',
		    Type	=> SOCK_STREAM,
		    Timeout	=> 30,
		);
		$from_smtp = $to_smtp;
		if (!defined($to_smtp) || !defined($from_smtp)) {
			die "IO::Socket::INET.new: $!\n";
		}

		$reply = $from_smtp->getline();
		die "I/O error\n" if $from_smtp->error;
		smtp_printS($reply) if $opts{verbose};
		$reply =~ s/[\r\n]+$//;
		if ($reply !~ /^220 /) {
			die "Bad greeting: <$reply>\n";
		}
	}
}
$to_smtp->autoflush(1);

my $submit_burl = 0;
if ($submission) {
	print "ehlo...\n" unless $opts{quiet};
	smtp_send_data("ehlo bdat.pl\r\n");
	smtp_flush();
	my $submit_auth_plain = 0;
	my $submit_auth_cram_md5 = 0;
	while ($reply = $from_smtp->getline()) {
		smtp_printS($reply) if $opts{verbose};
		$reply =~ s/[\r\n]+$//;
		$submit_auth_plain = 1 if $reply =~ /^\d+.AUTH.*PLAIN/i;
		$submit_auth_cram_md5 = 1 if $reply =~ /^\d+.AUTH.*CRAM-MD5/i;
		if ($reply =~ /^\d+.BURL/) {
			if ($reply !~ /^\d+.BURL$/) {
				die "Unexpected BURL arguments: <$reply>\n";
			}
			$submit_burl = 1;
		}
		if ($reply =~ /^\d+ /) {
			if ($reply !~ /^2/) {
				die "Ehlo failed: <$reply>\n";
			}
			last;
		}
	}
	die "I/O error\n" if $from_smtp->error;
	if (!$submit_auth_plain && !$submit_auth_cram_md5) {
		print STDERR "Submission server supports neither PLAIN nor CRAM-MD5 auth so I don't know how to log in.\n";
		print STDERR "Continuing without BURL\n";
		$submit_burl = 0;
	} elsif (!$submit_burl) {
		print STDERR "Submission server does not support BURL\n";
		print STDERR "Continuing without BURL\n";
	} else {
		print "logging in...\n" unless $opts{quiet};
		my $submit_auth = $submit_auth_cram_md5 ? "CRAM-MD5" : "PLAIN";
		smtp_send_data("auth $submit_auth\r\n");
		smtp_flush();
		while ($reply = $from_smtp->getline()) {
			smtp_printS($reply) if $opts{verbose};
			$reply =~ s/[\r\n]+$//;
			if ($reply =~ /^\d+/) {
				if ($reply !~ /^3/) {
					die "Auth failed: <$reply>\n";
				}
				last;
			}
		}
		die "I/O error\n" if $from_smtp->error;
		if ($submit_auth_cram_md5) {
			my ($challenge) = ($reply =~ /^\d+ (.*)/);
			$challenge = APR::Base64::decode($challenge);
			print "Decoded challenge: $challenge\n" if $opts{verbose};
			my $digest = Digest::HMAC_MD5::hmac_md5_hex($challenge, $opts{password});
			smtp_send_data(APR::Base64::encode("$opts{user} $digest") . "\r\n");
		} else {
			smtp_send_data(APR::Base64::encode("\0$opts{user}\0$opts{password}") . "\r\n");
		}
		smtp_flush();
		while ($reply = $from_smtp->getline()) {
			smtp_printS($reply) if $opts{verbose};
			$reply =~ s/[\r\n]+$//;
			if ($reply =~ /^\d+ /) {
				if ($reply !~ /^2/) {
					die "Auth failed: <$reply>\n";
				}
				last;
			}
		}
		die "I/O error\n" if $from_smtp->error;
	}
}

print "ehlo...\n" unless $opts{quiet};
smtp_send_data("ehlo bdat.pl\r\n");
smtp_flush();
my $smtp_binarymime;
my $smtp_chunking;
my $smtp_burl_imap;
while ($reply = $from_smtp->getline()) {
	smtp_printS($reply) if $opts{verbose};
	$reply =~ s/[\r\n]+$//;
	$smtp_binarymime = 1 if $reply =~ /^250[- ]BINARYMIME$/;
	$smtp_chunking = 1 if $reply =~ /^250[- ]CHUNKING$/;
	$smtp_burl_imap = 1 if $reply =~ /^250[- ]BURL imap$/;
	if ($reply =~ /^\d+ /) {
		if ($reply !~ /250 /) {
			die "Ehlo failed: <$reply>\n";
		}
		last;
	}
}
die "I/O error\n" if $from_imap->error;
die "$opts{host} did not advertise BINARYMIME in ehlo reply\n"
	unless $smtp_binarymime;
die "$opts{host} did not advertise CHUNKING in ehlo reply\n"
	unless $smtp_chunking;
warn "$opts{host} did not advertise BURL imap in ehlo reply; continuing without BURL\n"
	if $submit_burl && !$smtp_burl_imap;

my $ok = 1;
my $expect_OK;
my $explanation;
for my $delivery (1..$opts{deliveries}) {
	$expect_OK = 1;
	undef $explanation;
	my $status = deliver($delivery);
	if ($status < 0) {
		$ok = 0;
		last;
	} elsif ($status == 0) {
		print "rset...\n" unless $opts{quiet};
		smtp_send_data("rset\r\n");
		smtp_flush();
		while ($reply = $from_smtp->getline()) {
			smtp_printS($reply) if $opts{verbose};
			$reply =~ s/[\r\n]+$//;
			if ($reply =~ /^\d+ /) {
				if ($reply !~ /250 /) {
					die "Rset failed: <$reply>\n";
				}
				last;
			}
		}
		die "I/O error\n" if $from_smtp->error;
	}
}

print "quit...\n" unless $opts{quiet};
smtp_send_data("quit\r\n");
smtp_flush();
while ($reply = $from_smtp->getline()) {
	smtp_printS($reply) if $opts{verbose};
	$reply =~ s/[\r\n]+$//;
	if ($reply =~ /^\d+ /) {
		if ($reply !~ /221 /) {
			die "Quit failed: <$reply>\n";
		}
		last;
	}
}
die "I/O error\n" if $from_smtp->error;

$to_smtp->close();
if (defined($smtppid)) {
	$from_smtp->close();
	waitpid($smtppid, 0);
	undef $smtppid;
}

print "logout...\n" unless $opts{quiet};
if ($imap_idle) {
	imap_send_data("done\r\n");
	imap_flush();
	while ($reply = $from_imap->getline()) {
		imap_printS($reply) if $opts{verbose};
		$reply =~ s/[\r\n]+$//;
		if ($reply =~ /^i /) {
			if ($reply !~ /i OK (\[.*\])?/) {
				die "Idle failed: <$reply>\n";
			}
			last;
		}
	}
	die "I/O error\n" if $from_imap->error;
	$imap_idle = 0;
}
imap_send_data("z logout\r\n");
imap_flush();
while ($reply = $from_imap->getline()) {
	imap_printS($reply) if $opts{verbose};
	$reply =~ s/[\r\n]+$//;
	if ($reply =~ /^z /) {
		if ($reply !~ /z OK (\[.*\])?/) {
			die "Logout failed: <$reply>\n";
		}
		last;
	}
}
die "I/O error\n" if $from_imap->error;

$to_imap->close();
if (defined($imappid)) {
	$from_imap->close();
	waitpid($imappid, 0);
	undef $imappid;
}

if ($ok) {
	print "All tests passed.\n";
	exit 0;
} else {
	print "At least one test failed.\n";
	exit 1;
}

sub deliver
{
	my $delivery = shift or die;

	my $dtag = "deliver$delivery";
	my $ctag = "check$delivery";

	my @formats = ("", " body=8bitmime", " body=binarymime");
	my $r = int(rand(10));
	if ($r < 2) {
		$r = 0;
	} elsif ($r < 4) {
		$r = 1;
	} else {
		$r = 2;
	}
	my $format = $formats[$r];
	if (int(rand(20)) == 0) {
		failif(1, "sent no MAIL Fail: command");
	} else {
		print "$dtag (mail)...\n" unless $opts{quiet};
		smtp_send_data("mail from: $dtag$format\r\n");
		smtp_flush();
		while ($reply = $from_smtp->getline()) {
			smtp_printS($reply) if $opts{verbose};
			$reply =~ s/[\r\n]+$//;
			if ($reply =~ /^\d+ /) {
				if ($reply !~ /^250 /) {
					die "Mail failed: <$reply>\n";
				}
				last;
			}
		}
		die "I/O error\n" if $from_smtp->error;
	}

	if (int(rand(20)) == 0) {
		failif(1, "sent no RCPT command");
	} else {
		print "$dtag (rcpt)...\n" unless $opts{quiet};
		smtp_send_data("rcpt to: $opts{user}\r\n");
		smtp_flush();
		while ($reply = $from_smtp->getline()) {
			smtp_printS($reply) if $opts{verbose};
			$reply =~ s/[\r\n]+$//;
			if ($reply =~ /^\d+ /) {
				if ($expect_OK) {
					if ($reply !~ /^250 /) {
						print STDERR "Fail: Rcpt failed but should have succeeded: <$reply>\n";
						return -1;
					}
				} else {
					if ($reply =~ /^250 /) {
						print STDERR "Fail: Rcpt command succeeded but should have failed ($explanation): <$reply>\n";
						return -1;
					} else {
						print "Success: Rcpt command failed as it should have ($explanation): <$reply>\n" unless $opts{quiet};
						return 0;
					}
				}
				last;
			}
		}
		die "I/O error\n" if $from_smtp->error;
	}

	my ($message, $cleaned, $rawsections, $cleansections, $fetchable) = message($format, $dtag);
	my @rawsections = @$rawsections;
	my @cleansections = @$cleansections;
	my @fetchable = @$fetchable;
	print "=== RAW ===\n$message".
	    "\n=== CLEAN ===\n$cleaned".
	    "\n=== RAW SECTIONS ===\n".join("//\n",@rawsections).
	    "\n=== CLEAN SECTIONS ===\n".join("//\n",@cleansections).
	    "\n=== FETCHABLE ===\n".join("//\n", (map { defined($_) ? $_ : "" } @fetchable)).
	    "\n=== END ===\n"
		if $opts{debug};
	die "Internal consistency botch: rawsections has ".scalar(@rawsections)." but fetchable has ".scalar(@fetchable)."\n"
		unless @rawsections == @fetchable;
	die "Internal consistency botch: cleansections has ".scalar(@cleansections)." but fetchable has ".scalar(@fetchable)."\n"
		unless @cleansections == @fetchable;
	my @fragments;
	my $burl_ok;
	if (int(rand(2)) == 0) {
		# break the message up into random fragments, don't use burl
		my $consumed = "";
		my $remaining = $message;
		my $stuck = 0;
		do {
			my $cut = int(rand(length($remaining) + 1));    # 0 is ok
			my $fragment = substr($remaining, 0, $cut);

			# postfix does not handle fragmented header labels (e.g., "Fr" + "om: foo")
			# or fragmented MIME separators (e.g., "--Apple-Ma" + "il-57-197753312--")
			# also avoid breaking a header at a space (e.g., "From: foo" + " <foo@bar.baz")
			# or breaking any CRLF
			my $linestart = "$consumed$fragment";
			$linestart =~ s/.*\n//s;
			my $linecont = substr($remaining, $cut);
			$linecont =~ s/\n.*//s;
			if (($linestart !~ /^[!-9;-~][ -9;-~]*$/ || $linecont !~ /^[ -9;-~]*:/) &&
			    ($linestart !~ /^[!-9;-~][ -9;-~]*:/ || $linecont !~ /^[ \t]/) &&
			    "$linestart$linecont" !~ /^--sep\d+(--)?\r?\z/ &&
			    $linestart !~ /\r\z/) {
				$remaining = substr($remaining, $cut);
				$consumed .= $fragment;
				push @fragments, $fragment;
				$stuck = 0;
			} else {
				print "NOT cutting: |$linestart|<-HERE->|$linecont|\n...".substr($consumed,-20)."|<-HERE->|".substr($remaining,0,20)."...\n" if $opts{debug};
				if (++$stuck >= 1000) {
					print "Can't fragment this message, giving up on it.\n" unless $opts{quiet};
					return 0;
				}
			}
		} while (length $remaining > 0);
		$burl_ok = 0;

		my $sanity = "";
		$sanity .= $_ for @fragments;
		die "Internal consistency botch: fragmented message does not match whole.\nWhole:\n$message\nFragmented:\n$sanity\n"
			unless $sanity eq $message;
	} else {
		# break the message up into natural fragments, can use burl
		@fragments = @fetchable;
		$burl_ok = 1;

		my $sanity = "";
		$sanity .= $_ for @rawsections;		# sanity needs headers
		die "Internal consistency botch: sectioned message does not match whole.\nRaw:\n$message\nSectioned:\n$sanity\n"
			unless $sanity eq $message;
		$sanity = "";
		$sanity .= $_ for @cleansections;	# sanity needs headers
		die "Internal consistency botch: sectioned message does not match whole.\nClean:\n$cleaned\nSectioned:\n$sanity\n"
			unless $sanity eq $cleaned;
	}

	my $secno = 0;
	my $lasturl;
	for my $fragno (1..@fragments) {
		my $fragment = $fragments[$fragno - 1];
		++$secno if defined $fragment;

		my $r = int(rand(20));
		if ($r == 0) {
			print "$dtag (data)...\n" unless $opts{quiet};
			smtp_send_data("data\r\n");
			smtp_flush();
			failif($fragno > 1, "mixed BDAT/BURL/DATA commands");
			failif(scalar($format =~ /binarymime/i), "DATA with BINARYMIME");
			while ($reply = $from_smtp->getline()) {
				smtp_printS($reply) if $opts{verbose};
				$reply =~ s/[\r\n]+$//;
				if ($reply =~ /^\d+ /) {
					if ($expect_OK) {
						if ($reply !~ /^3\d\d /) {
							print STDERR "Fail: Data failed but should have succeeded: <$reply>\n";
							return -1;
						}
					} else {
						if ($reply =~ /^[23]\d\d /) {
							print STDERR "Fail: Data succeeded but should have failed ($explanation): <$reply>\n";
							return -1;
						} else {
							print "Success: Data failed as it should have ($explanation): <$reply>\n" unless $opts{quiet};
							return 0;
						}
					}
					last;
				}
			}
			die "I/O error\n" if $from_smtp->error;

			print "$dtag (message)...\n" unless $opts{quiet};
			smtp_send_data($message);	# send $message not $fragment
			#smtp_send_data("\r\n") unless $message =~ /\r\n$/s;
			die unless $message =~ /\r\n\z/;
			smtp_send_data(".\r\n");
			smtp_flush();
			while ($reply = $from_smtp->getline()) {
				smtp_printS($reply) if $opts{verbose};
				$reply =~ s/[\r\n]+$//;
				if ($reply =~ /^\d+ /) {
					if ($expect_OK) {
						if ($reply !~ /^250 /) {
							print STDERR "Fail: Data transaction failed but should have succeeded: <$reply>\n";
							return -1;
						}
					} else {
						if ($reply =~ /^250 /) {
							print STDERR "Fail: Data transaction succeeded but should have failed ($explanation): <$reply>\n";
							return -1;
						} else {
							print "Success: Data transaction failed as it should have ($explanation): <$reply>\n" unless $opts{quiet};
							return 0;
						}
					}
					last;
				}
			}
			die "I/O error\n" if $from_smtp->error;

			last;	    # sent whole message, go check receipt
		} elsif ($r <= 8 && $smtp_burl_imap && $burl_ok && defined($fragment) &&
			 $rawsections[$fragno - 2] !~ /Content-transfer-encoding: binary/i &&
			 $fragment !~ /Content-transfer-encoding: binary/i) {
			print "$dtag (burl append)...\n" unless $opts{quiet};
			imap_send_data("done\r\n");
			imap_flush();
			while ($reply = $from_imap->getline()) {
				imap_printS($reply) if $opts{verbose};
				$reply =~ s/[\r\n]+$//;
				if ($reply =~ /^i /) {
					if ($reply !~ /i OK (\[.*\])?/) {
						die "Idle failed: <$reply>\n";
					}
					last;
				}
			}
			die "I/O error\n" if $from_imap->error;
			$imap_idle = 0;

			my $size = length($message);
			imap_send_data("a$dtag append scratchbox {$size}\r\n");
			imap_flush();
			$reply = $from_imap->getline();
			die "I/O error\n" if $from_imap->error;
			imap_printS($reply) if $opts{verbose};
			$reply =~ s/[\r\n]+$//;
			if ($reply ne "+ OK") {
				die "Append failed: <$reply>\n";
			}
			imap_send_data("$message\r\n");
			imap_flush();
			while ($reply = $from_imap->getline()) {
				imap_printS($reply) if $opts{verbose};
				$reply =~ s/[\r\n]+$//;
				if ($reply =~ /^a$dtag /) {
					if ($reply !~ /a$dtag OK /) {
						die "Append failed: <$reply>\n";
					}
					last;
				}
			}
			die "I/O error\n" if $from_imap->error;
			my ($uidvalidity, $uid) = ($reply =~ /\[APPENDUID (\d+) (\d+)\]/);
			die "Append reply missing APPENDUID: <$reply>\n" unless defined $uid;

			print "$dtag (burl genurlauth)...\n" unless $opts{quiet};
			imap_send_data("g$dtag genurlauth imap://$opts{user}\@$opts{host}/scratchbox;uidvalidity=$uidvalidity/;uid=$uid/;section=$secno;urlauth=submit+$opts{user} internal\r\n");
			imap_flush();
			my $url;
			while ($reply = $from_imap->getline()) {
				imap_printS($reply) if $opts{verbose};
				$reply =~ s/[\r\n]+$//;
				if ($reply =~ /^g$dtag /) {
					if ($reply !~ /g$dtag OK /) {
						die "Genurlauth failed: <$reply>\n";
					}
					last;
				} elsif ($reply =~ /^\* GENURLAUTH "(.*)"/i ||
					 $reply =~ /^\* GENURLAUTH (.*)/i) {
					$url = $1;
					$lasturl = $1;
				}
			}
			die "I/O error\n" if $from_imap->error;
			die "Genurlauth returned no URL\n" unless defined $url;

			print "$dtag (burl idle)...\n" unless $opts{quiet};
			imap_send_data("i idle\r\n");
			imap_flush();
			while ($reply = $from_imap->getline()) {
				imap_printS($reply) if $opts{verbose};
				$reply =~ s/[\r\n]+$//;
				if ($reply =~ /^\+ /) {
					$imap_idle = 1;
					last;
				} elsif ($reply =~ /^i /) {
					die "Idle failed: <$reply>\n";
				}
			}
			die "I/O error\n" if $from_imap->error;

			my $last = $fragno == @fragments ? " last" : "";
			print "$dtag (burl$last)...\n" unless $opts{quiet};
			smtp_send_data("burl $url$last\r\n");
			smtp_flush();
			while ($reply = $from_smtp->getline()) {
				smtp_printS($reply) if $opts{verbose};
				$reply =~ s/[\r\n]+$//;
				if ($reply =~ /^\d+ /) {
					if ($expect_OK) {
						if ($reply !~ /^250 /) {
							print STDERR "Fail: Burl failed but should have succeeded: <$reply>\n";
							return -1;
						}
					} else {
						if ($reply =~ /^250 /) {
							print STDERR "Fail: Burl succeeded but should have failed ($explanation): <$reply>\n";
							return -1;
						} else {
							print "Success: Burl failed as it should have ($explanation): <$reply>\n" unless $opts{quiet};
							return 0;
						}
					}
					last;
				}
			}
			die "I/O error\n" if $from_smtp->error;
		} else {
			$fragment = $rawsections[$fragno - 1] unless defined $fragment;

			my $last = $fragno == @fragments ? " last" : "";
			print "$dtag (bdat$last)...\n" unless $opts{quiet};
			my $size = length($fragment);
			smtp_send_data("bdat $size$last\r\n");
			smtp_send_data($fragment);
			smtp_flush();
			while ($reply = $from_smtp->getline()) {
				smtp_printS($reply) if $opts{verbose};
				$reply =~ s/[\r\n]+$//;
				if ($reply =~ /^\d+ /) {
					if ($expect_OK) {
						if ($reply !~ /^250 /) {
							print STDERR "Fail: Bdat failed but should have succeeded: <$reply>\n";
							return -1;
						}
					} else {
						if ($reply =~ /^250 /) {
							print STDERR "Fail: Bdat succeeded but should have failed ($explanation): <$reply>\n";
							return -1;
						} else {
							print "Success: Bdat failed as it should have ($explanation): <$reply>\n" unless $opts{quiet};
							return 0;
						}
					}
					last;
				}
			}
			die "I/O error\n" if $from_smtp->error;
		}
	}

	while (int(rand(4)) == 0) {
		# make sure extra bdat or burl fails
		# make sure bdat input is properly eaten on error
		my $cmd = defined $lasturl ? "burl" : "bdat";
		my $last = int(rand(2)) == 0 ? " last" : "";
		print "$dtag ($cmd$last)...\n" unless $opts{quiet};
		if ($cmd eq "burl") {
			smtp_send_data("burl $lasturl$last\r\n");
		} else {
			smtp_send_data("bdat 6$last\r\nfail\r\n");
		}
		smtp_flush();
		failif(1, "BURL/BDAT after DATA or LAST");
		while ($reply = $from_smtp->getline()) {
			smtp_printS($reply) if $opts{verbose};
			$reply =~ s/[\r\n]+$//;
			if ($reply =~ /^\d+ /) {
				if ($reply =~ /^250 /) {
					print STDERR "Fail: Extra bdat/burl succeeded but should have failed ($explanation): <$reply>\n";
					return -1;
				}
				last;
			}
		}
		die "I/O error\n" if $from_smtp->error;
	}

	# now verify correct receipt
	print "waiting for receipt...\n" unless $opts{quiet};
	die unless $imap_idle;
	my ($exists, $recent);
	my $keepalive = 0;
	while ($reply = $from_imap->getline()) {
		imap_printS($reply) if $opts{verbose};
		$reply =~ s/[\r\n]+$//;
		if ($reply =~ /^\* (\d+) EXISTS/i) {
			$exists = $1;
		} elsif ($reply =~ /^\* (\d+) RECENT/i) {
			$recent = $1;
		} elsif ($reply =~ /^\* OK/) {
			++$keepalive;
		}
		last if defined $exists and defined $recent;
		last if $keepalive >= 2;	# 2 minutes per...
	}
	die "I/O error\n" if $from_imap->error;
	imap_send_data("done\r\n");
	imap_flush();
	while ($reply = $from_imap->getline()) {
		imap_printS($reply) if $opts{verbose};
		$reply =~ s/[\r\n]+$//;
		if ($reply =~ /^i /) {
			if ($reply !~ /i OK (\[.*\])?/) {
				die "Idle failed: <$reply>\n";
			}
			last;
		}
	}
	die "I/O error\n" if $from_imap->error;
	$imap_idle = 0;

	if (!defined($exists)) {
		# idle failed for some reason.  try closing and reselecting the inbox
		print STDERR "Warning: IMAP IDLE command did not inform of the new message.\n" .
			     "Trying to recover but the message may be stuck in the queue....\n";

		print "close...\n" unless $opts{quiet};
		imap_send_data("x close\r\n");
		imap_flush();
		while ($reply = $from_imap->getline()) {
			imap_printS($reply) if $opts{verbose};
			$reply =~ s/[\r\n]+$//;
			if ($reply =~ /^x /) {
				last;
			}
		}
		die "I/O error\n" if $from_imap->error;

		print "select...\n" unless $opts{quiet};
		imap_send_data("c select inbox\r\n");
		imap_flush();
		while ($reply = $from_imap->getline()) {
			imap_printS($reply) if $opts{verbose};
			$reply =~ s/[\r\n]+$//;
			if ($reply =~ /^c OK /) {
				last;
			} elsif ($reply =~ /^\* (\d+) EXISTS/i) {
				$exists = $1;
			}
		}
		die "I/O error\n" if $from_imap->error;

		die "Can't determine number of messages in INBOX\n"
			if !defined($exists);
	}
	if ($exists <= $inbox_message_count) {
		print STDERR "Fail: Message not delivered.  (EXISTS $exists now, $inbox_message_count before)\n";
		return -1;
	}
	$inbox_message_count = $exists;

	my $cleaned_len = length($cleaned);

	print "fetch...\n" unless $opts{quiet};
	imap_send_data("$ctag fetch $exists rfc822\r\n");
	imap_flush();
	my $verify = "";
	my $verify_size = 0;
	while ($reply = $from_imap->getline()) {
		imap_printS($reply) if $opts{verbose};
		$reply =~ s/[\r\n]+$//;
		if ($reply =~ /^$ctag /) {
			if ($reply !~ /$ctag OK (\[.*\])?/) {
				die "Fetch failed: <$reply>\n";
			}
			last;
		} elsif ($reply =~ /^\* (\d+) FETCH .*{(\d+)}/i) {
			if ($1 != $exists) {
				die "Fetch returned wrong message $1, expected $exists\n";
			} elsif ($2 < $cleaned_len) {
				print STDERR "Fetch returned wrong size $2, expected >= $cleaned_len\n";
			}
			$verify_size = $2;
		} else {
			$verify .= "$reply\r\n";
		}
	}
	die "I/O error\n" if $from_imap->error;
	$verify =~ s/\)\r\n$//;
	if ($verify_size < $cleaned_len ||
	    !message_fuzzy_equal($verify, $cleaned)) {
		print STDERR "Fail: Fetched data does not match delivered message.\nFormat: $format\nOriginal:\n$message\nExpected:\n$cleaned\nGot:\n$verify\n";
		return -1;
	}

	print "idle...\n" unless $opts{quiet};
	imap_send_data("i idle\r\n");
	imap_flush();
	while ($reply = $from_imap->getline()) {
		imap_printS($reply) if $opts{verbose};
		$reply =~ s/[\r\n]+$//;
		if ($reply =~ /^\+ /) {
			$imap_idle = 1;
			last;
		} elsif ($reply =~ /^i /) {
			die "Idle failed: <$reply>\n";
		}
	}
	die "I/O error\n" if $from_imap->error;

	print "Success: Delivery and fetch succeeded and matched.\n" unless $opts{quiet};
	return $expect_OK;
}

sub message
{
	my $format = shift;
	my $tag = shift or die;
	my $message = "";
	my $cleaned = "";
	my @rawsections = ();
	my @cleansections = ();
	my @fetchable = ();
	my $encoding;
	do {
		# 33% chance for 7bit, 8bit, binary
		$encoding = $encodings_top[int(rand(@encodings_top - 1)) + 1];
	} while (!sub_encoding_allowed($encoding));
	# 16% "", 16% 7bit, 33% 8bit, 33% binary
	$encoding = "" if $encoding eq "7bit" && int(rand(2)) == 0;
	my $type;
	do {
		# like above
		$type = $types[int(rand(@types - 1)) + 1];
	} while (!type_encoding_allowed($type, $encoding));
	$type = "" if $type eq "text/plain" && int(rand(2)) == 0;
	my @funcs = @{$typefuncs{$type}};
	my $header_func = $funcs[0];
	my $body_func = $funcs[1];
	my ($type_header, $context) = $header_func->($type);

	my $am_top = !defined($top_encoding);

	my @headers;
	push @headers, "Message-Id: <$tag-".int(rand(2_000_000_000))."\@bdat.pl>";
	push @headers, "From: Bdat Script <bdat\@bdat.pl>";
	push @headers, "To: $opts{user}\@$opts{host}";
	push @headers, $type_header unless $type eq "";
	push @headers, "Content-Transfer-Encoding: $encoding" unless $encoding eq "";
	push @headers, "Subject: test message $tag from bdat.pl pid $$";
	push @headers, "MIME-Version: 1.0";
	@headers = shuffle(@headers);
	push @headers, "";
	my $header = join("\r\n", @headers) . "\r\n";
	$message .= $header;
	push @rawsections, $header if $am_top;
	if ($format =~ /binarymime/) {
		if ($type =~ /multipart/ || $type =~ /message/) {
			$header =~ s/(content-transfer-encoding): binary/$1: 7bit/i;
		} else {
			$header =~ s/(content-transfer-encoding): binary/$1: base64/i;
		}
	}
	$cleaned .= $header;

	if ($am_top) {
		$top_encoding = $encoding;
		push @cleansections, $header;
		push @fetchable, undef;	# top-level headers not available via urlfetch
	}
	my ($fullpart, $cleanpart, $rawsectionspart, $cleansectionspart, $fetchablepart) = $body_func->($format, $encoding, $context);
	$message .= $fullpart;
	$cleaned .= $cleanpart;
	if ($am_top) {
		undef $top_encoding;
		push @rawsections, @$rawsectionspart;
		push @cleansections, @$cleansectionspart;
		push @fetchable, @$fetchablepart;

		die unless @rawsections == @cleansections;
		if (@rawsections > 0 &&
		    $rawsections[$#rawsections] eq $cleansections[$#cleansections] &&
		    substr($rawsections[$#rawsections], -2) ne "\r\n") {
			die if substr($message, -2) eq "\r\n";
			die if substr($cleaned, -2) eq "\r\n";
			$message .= "\r\n";
			$cleaned .= "\r\n";
			$rawsections[$#rawsections] .= "\r\n";
			$cleansections[$#cleansections] .= "\r\n";
			$fetchable[$#fetchable] .= "\r\n" if defined($fetchable[$#fetchable]);
		}
	} else {
		push @rawsections, $message;
		push @cleansections, $cleaned;
		push @fetchable, $cleaned;
	}

	return ($message, $cleaned, \@rawsections, \@cleansections, \@fetchable);
}

sub header_plain
{
	my $type = shift;
	my $header = "Content-type: $type";
	return ($header, undef);
}

sub body_plain
{
	my $format = shift;
	my $encoding = shift;

	my @words = ("", "I", "hi", "cod", "sole", "shark", "salmon", "walleye",
		     "flounder", "orange roughy");
	push @words, <<EOT;
Four score and seven years ago our fathers brought forth on this
continent a new nation, conceived in Liberty, and dedicated to the
proposition that all men are created equal.

Now we are engaged in a great civil war, testing whether that nation, or
any nation, so conceived and so dedicated, can long endure. We are met
on a great battle-field of that war. We have come to dedicate a portion
of that field, as a final resting place for those who here gave their
lives that that nation might live. It is altogether fitting and proper
that we should do this.

But, in a larger sense, we can not dedicate... we can not consecrate...
we can not hallow this ground. The brave men, living and dead, who
struggled here, have consecrated it, far above our poor power to add or
detract. The world will little note, nor long remember what we say here,
but it can never forget what they did here. It is for us the living,
rather, to be dedicated here to the unfinished work which they who
fought here have thus far so nobly advanced. It is rather for us to be
here dedicated to the great task remaining before us -- that from these
honored dead we take increased devotion to that cause for which they
gave the last full measure of devotion -- that we here highly resolve
that these dead shall not have died in vain -- that this nation, under
God, shall have a new birth of freedom -- and that government of the
people, by the people, for the people, shall not perish from the earth.
EOT
	my $word = $words[int(rand(@words))];
	chomp $word;
	$word = "$word\n";
	$word .= "\n" x int(rand(3));
	$word =~ s/\n/\r\n/gs;

	my $clean = $encodingfuncs{$encoding}->($format, $word);
	$word = $clean if $encoding eq "base64" || $encoding eq "quoted-printable";
	return ($word, $clean, [$word], [$clean], [$word]);
}

sub header_message
{
	return header_plain(shift);
}

sub body_message
{
	my $format = shift;

	return message($format, "encapsulated");
}

sub header_gibberish
{
	return header_plain(shift);
}

sub body_gibberish
{
	my $format = shift;
	my $encoding = shift;

	my ($gibberish, $clean);
	do {
		$gibberish = "";

		my $length = int(rand(5000)) + 1;
		$gibberish .= chr(int(rand(256))) for (1..$length);
		if ($encoding =~ /8bit/) {
			$gibberish =~ s/\0//g;		# 8bit forbids NUL
			1 while $gibberish =~ s/(\A|[^\r])\n/$1\r\n/g;	# 8bit is line-oriented
			1 while $gibberish =~ s/\r([^\n]|\z)/\r\n$1/g;	# 8bit is line-oriented
			$gibberish .= "\r\n" unless substr($gibberish, -2) eq "\r\n";
		}

		$clean = $encodingfuncs{$encoding}->($format, $gibberish);

	# . at beginning of line will be removed, so try again
	} while (($encoding ne "binary" || $format !~ /binarymime/) && ($gibberish =~ /^\./m || $clean =~ /^\./m));

	$gibberish = $clean if $encoding eq "base64" || $encoding eq "quoted-printable";
	return ($gibberish, $clean, [$gibberish], [$clean], [$gibberish]);
}

sub header_mixed
{
	my $type = shift;
	my $sep = "sep" . int(rand(2_000_000_000));
	my $header = "Content-type: $type;\r\n\tboundary=$sep";
	return ($header, $sep);
}

sub body_mixed
{
	my $format = shift;
	my $encoding = shift;
	my $sep = shift or die;
	my $nparts = int(rand(5)) + 1;

	# preamble
	my ($data, $clean, @rawsections, @cleansections, @fetchable);
	$data .= "preamble\r\n";
	$clean .= "preamble\r\n";
	push @rawsections, "preamble\r\n";
	push @cleansections, "preamble\r\n";
	push @fetchable, undef;

	for my $partno (1..$nparts) {
		my $partencoding;
		do {
			$partencoding = $encodings_sub[int(rand(@encodings_sub - 1)) + 1];
		} while (!sub_encoding_allowed($partencoding));
		$partencoding = "" if $partencoding eq "7bit" && int(rand(2)) == 0;
		my $parttype;
		do {
			$parttype = $types[int(rand(@types - 1)) + 1];
		} while (!type_encoding_allowed($parttype, $partencoding));
		$parttype = "" if $parttype eq "text/plain" && int(rand(2)) == 0;
		my @partfuncs = @{$typefuncs{$parttype}};
		my $partheader_func = $partfuncs[0];
		my $partbody_func = $partfuncs[1];
		my ($parttype_header, $partcontext) = $partheader_func->($parttype);

		my @partheaders;
		push @partheaders, $parttype_header unless $parttype eq "";
		push @partheaders, "Content-transfer-encoding: $partencoding" unless $partencoding eq "";
		push @partheaders, "Mime-version: 1.0" if int(rand(2)) == 0;
		push @partheaders, "Content-disposition: inline" if int(rand(2)) == 0;
		@partheaders = shuffle(@partheaders);
		unshift @partheaders, "\r\n--$sep";
		push @partheaders, "";
		my $partheader = join("\r\n", @partheaders) . "\r\n";
		$data .= $partheader;
		push @rawsections, $partheader;
		if ($format =~ /binarymime/) {
			if ($parttype =~ /multipart/ || $parttype =~ /message/) {
				$partheader =~ s/(content-transfer-encoding): binary/$1: 7bit/i;
			} else {
				$partheader =~ s/(content-transfer-encoding): binary/$1: base64/i;
			}
		}
		$clean .= $partheader;
		push @cleansections, $partheader;
		push @fetchable, undef;

		my ($partfull, $partclean, undef, undef, undef) = $partbody_func->($format, $partencoding, $partcontext);
		if ($partno < $nparts && int(rand(2)) == 0 && $partfull eq $partclean) {
			# make sure sections not ending with linebreaks work
			# but only if the clean hasn't already been folded into base64
			if (substr($partfull, -2) eq "\r\n") {
				$partfull =~ s/\r\n\z//;
				$partclean =~ s/\r\n\z//;
			} elsif (substr($partfull, -1) eq "\n") {
				$partfull =~ s/\n\z//;
				$partclean =~ s/\n\z//;
			}
		}
		$data .= $partfull;
		$clean .= $partclean;
		push @rawsections, $partfull;		# don't need divided subsections
		push @cleansections, $partclean;	# don't need divided subsections
		push @fetchable, $partfull;		# don't need divided subsections
	}
	$data .= "\r\n--$sep--\r\n";
	$clean .= "\r\n--$sep--\r\n";
	push @rawsections, "\r\n--$sep--\r\n";
	push @cleansections, "\r\n--$sep--\r\n";
	push @fetchable, undef;

	# epilogue
	$data .= "epilogue\r\n";
	$clean .= "epilogue\r\n";
	push @rawsections, "epilogue\r\n";
	push @cleansections, "epilogue\r\n";
	push @fetchable, undef;

	return ($data, $clean, \@rawsections, \@cleansections, \@fetchable);
}

sub type_encoding_allowed
{
	my $type = shift;
	my $encoding = shift;

	if ($type =~ m,message/, || $type =~ m,multipart/,) {
		return $encoding ne "base64" && $encoding ne "quoted-printable";
	} elsif ($type =~ m,application/,) {
		return $encoding ne "" && $encoding ne "7bit";
	}
	return 1;
}

sub sub_encoding_allowed
{
	my $sub_encoding = shift;

	return 1 if !defined($top_encoding);
	if ($sub_encoding eq "8bit") {
		return $top_encoding eq "8bit" || $top_encoding eq "binary";
	} elsif ($sub_encoding eq "binary") {
		return $top_encoding eq "binary";
	}
	return 1;
}

sub clean_7bit
{
	my $format = shift;
	my $data = shift;
	return $data;
}

sub clean_8bit
{
	my $format = shift;
	my $data = shift;
	return $data;
}

sub clean_binary
{
	my $format = shift;
	my $data = shift;
	return ($format =~ /binarymime/) ? clean_base64($format, $data) : $data;
}

sub clean_base64
{
	my $format = shift;
	my $raw = shift;
	my $b64 = APR::Base64::encode($raw);
	$b64 =~ s/(.{76})(?=.)/$1\r\n/g;
	return $b64;
}

sub clean_qp
{
	my $format = shift;
	my $raw = shift;
	my $qp = encode_qp($raw, "\r\n");
	return $qp;
}

sub message_fuzzy_equal
{
	my $actual = shift;
	my $expected = shift;

	# SMTP adds/modifies headers; perform fuzzy match
	$actual =~ s/\*\*\*JUNK MAIL\*\*\* //i;
	$actual =~ s/^(Date|Return-Path|Delivered-To|Received|X-Virus-Scanned|X-Amavis-Alert|X-Spam-[a-z]+): [^\n]+(\n\s[^\n]+)*\n//mgi;

	# during delivery of non-8bit-conforming gibberish, NUL becomes 0x80 and CRLF is enforced
	$actual =~   s/\0/\200/g;
	$expected =~ s/\0/\200/g;
	$actual =~   s/\r{2,}\n/\r\n/g;
	$expected =~ s/\r{2,}\n/\r\n/g;
	1 while $actual =~ s/(\A|[^\r])\n/$1\r\n/g;
	1 while $actual =~ s/\r([^\n]|\z)/\r\n$1/g;
	1 while $expected =~ s/(\A|[^\r])\n/$1\r\n/g;
	1 while $expected =~ s/\r([^\n]|\z)/\r\n$1/g;

	# the Content-Transfer-Encoding header(s) may be reordered but must still match
	my @actual_encodings;
	my @expected_encodings;
	while ($actual =~   s/^Content-Transfer-Encoding: ([^\n]+(\n\s[^\n]+)*)\n//mi) {
		my $cte = $1;
		$cte =~ s/\r//g;
		push @actual_encodings, $cte;
	}
	while ($expected =~ s/^Content-Transfer-Encoding: ([^\n]+(\n\s[^\n]+)*)\n//mi) {
		my $cte = $1;
		$cte =~ s/\r//g;
		push @expected_encodings, $cte;
	}
	my $actual_encodings = join(",", @actual_encodings);
	my $expected_encodings = join(",", @expected_encodings);

	print "=== EDITED ACTUAL (Content-Transfer-Encodings: $actual_encodings) ===\n$actual\n" .
	      "=== EDITED EXPECTED (Content-Transfer-Encodings: $expected_encodings) ===\n$expected\n" .
	      "=== END ===\n" if $opts{debug};
	return 1 if $actual_encodings eq $expected_encodings &&
		    ($actual eq $expected || $actual eq "$expected\r\n");
	return 0;
}

sub imap_flush
{
	imap_send_data(undef);
}

sub imap_send_data
{
	my $data = shift;

	state $bufsiz = undef;
	state $buf = "";

	my $flush;
	if (defined($data)) {
		if (!defined($bufsiz)) {
			$bufsiz = $opts{bufsiz};
			if (!defined($bufsiz)) {
				my $r = int(rand(3));
				if ($r == 0) {
					$bufsiz = 0;
				} elsif ($r == 1) {
					$bufsiz = int(rand(64)) + 1;
				} else {
					$bufsiz = int(rand(4096)) + 1;
				}
			}
		}

		$buf .= $data;
		$flush = length($buf) >= $bufsiz;
	} else {
		$flush = 1;
	}

	if ($flush && length($buf)) {
		imap_printC($buf) if $opts{verbose};
		$to_imap->print($buf);

		undef $bufsiz;
		$buf = "";
	}
}

sub smtp_flush
{
	smtp_send_data(undef);
}

sub smtp_send_data
{
	my $data = shift;

	state $bufsiz = undef;
	state $buf = "";

	my $flush;
	if (defined($data)) {
		if (!defined($bufsiz)) {
			$bufsiz = $opts{bufsiz};
			if (!defined($bufsiz)) {
				my $r = int(rand(3));
				if ($r == 0) {
					$bufsiz = 0;
				} elsif ($r == 1) {
					$bufsiz = int(rand(64)) + 1;
				} else {
					$bufsiz = int(rand(4096)) + 1;
				}
			}
		}

		$buf .= $data;
		$flush = length($buf) >= $bufsiz;
	} else {
		$flush = 1;
	}

	if ($flush && length($buf)) {
		smtp_printC($buf) if $opts{verbose};
		$to_smtp->print($buf);

		undef $bufsiz;
		$buf = "";
	}
}

sub imap_printC
{
	my $msg = shift;
	imap_printX("C", $msg);
	print "~FLUSH~" if $opts{buftag};
}

sub imap_printS
{
	imap_printX("S", @_);
}

sub imap_printX
{
	my $tag = shift;
	my $msg = shift;

	state $lastdir = "";
	state $lastmsg = "\n";

	if ($tag eq "C") {
		if ($lastdir ne "C") {
			print "~NO LINE TERMINATOR~\n" if $lastmsg !~ /\n$/;
			print ">"x72 . "\n";
			$lastdir = "C";
		}
	} else {
		if ($lastdir ne "S") {
			print "~NO LINE TERMINATOR~\n" if $lastmsg !~ /\n$/;
			print "<"x72 . "\n";
			$lastdir = "S";
		}
	}
	print $msg;
	$lastmsg = $msg;
}

sub smtp_printC
{
	my $msg = shift;
	smtp_printX("C", $msg);
	print "~FLUSH~" if $opts{buftag};
}

sub smtp_printS
{
	smtp_printX("S", @_);
}

sub smtp_printX
{
	my $tag = shift;
	my $msg = shift;

	state $lastdir = "";
	state $lastmsg = "\n";

	if ($tag eq "C") {
		if ($lastdir ne "C") {
			print "~NO LINE TERMINATOR~\n" if $lastmsg !~ /\n$/;
			print ">"x72 . "\n";
			$lastdir = "C";
		}
	} else {
		if ($lastdir ne "S") {
			print "~NO LINE TERMINATOR~\n" if $lastmsg !~ /\n$/;
			print "<"x72 . "\n";
			$lastdir = "S";
		}
	}
	print $msg;
	$lastmsg = $msg;
}

sub failif
{
	my $what = shift;
	my $why = shift;

	if ($what && $expect_OK) {
		$expect_OK = 0;
		$explanation = $why;
	}
}
