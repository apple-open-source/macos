#!/usr/bin/perl -Tw

# Copyright (c) 2010-2011 Apple Inc. All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without  
# modification, are permitted provided that the following conditions  
# are met:
# 
# 1.  Redistributions of source code must retain the above copyright  
# notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above  
# copyright notice, this list of conditions and the following  
# disclaimer in the documentation and/or other materials provided  
# with the distribution.
# 3.  Neither the name of Apple Inc. ("Apple") nor the names of its  
# contributors may be used to endorse or promote products derived  
# from this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND  
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,  
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A  
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS  
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,  
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT  
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF  
# USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND  
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,  
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT  
# OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF  
# SUCH DAMAGE.

# Update users' fts search indexes (fts plugin for dovecot).

use strict;
use feature 'state';
use Getopt::Long;
use File::Temp qw(tempfile);
use IPC::Open3;
use IO::Handle;
use Sys::Syslog qw(:standard :macros);
use Errno;

sub usage
{
	die <<EOT;
Usage: $0 [options] username ...
   or: $0 [options] --queued
Options:
	--mailbox name		update only this mailbox, not all mailboxes;
				multiple --mailbox arguments allowed
	--quiet
	--syslog		log to syslog not stdout/stderr
	--verbose
EOT
}

my %opts;
GetOptions(\%opts,
    'mailbox=s@',
    'queued',
    'quiet',
    'syslog',
    'verbose',
) || usage();

if ((@ARGV == 0 && !defined($opts{queued})) ||
    (@ARGV > 0 && defined($opts{queued}))) {
	usage();
}

if ($opts{syslog}) {
	my $ident = $0;
	$ident =~ s,.*/,,;
	openlog($ident, "pid", LOG_LOCAL6) or die("openlog: $!\n");
}

if ($> != 0) {
	myfatal("must run as root");
}

$ENV{PATH} = "/usr/bin:/bin:/usr/sbin:/sbin";
delete $ENV{CDPATH};

my $imappid;
my $to_imap;
my $from_imap;
local $SIG{__DIE__} = \&imap_cleanup;

my %work;
my @order;
if (defined($opts{queued})) {
	my $queue_dir = "/var/db/dovecot.fts.update";

	opendir(DIR, $queue_dir) or myfatal("$queue_dir: $!");
	my @entries = readdir(DIR);
	closedir(DIR);

	# slurp and unescape the list of files in the directory
	for (@entries) {
		next if /^\./;

		# untaint: fts creates files with only alpha, num, %, and .
		if (!/^(([a-zA-Z0-9%]+)\.([a-zA-Z0-9%]+))$/) {
			mywarn("$queue_dir/$_: malformed or unsafe name");
			next;
		}
		my $name = $1;
		my $user = $2;
		my $mailbox = $3;

		if (!unlink("$queue_dir/$name")) {
			# maybe another process got it first
			mywarn("$queue_dir/$name: $!") unless $!{ENOENT};
			next;
		}

		next unless defined $user and defined $mailbox;
		$user =~ s/%([a-fA-F0-9]{2})/chr(hex($1))/ge;
		$mailbox =~ s/%([a-fA-F0-9]{2})/chr(hex($1))/ge;

		push @{$work{$user}->{mailboxes}}, $mailbox;
		$work{$user}->{order} = rand;
	}

	# process the users in random order so nobody gets preference
	@order = sort { $work{$a}->{order} <=> $work{$b}->{order} } keys %work;
} else {
	for (@ARGV) {
		if (defined($opts{mailbox})) {
			@{$work{$_}->{mailboxes}} = @{$opts{mailbox}};
		} else {
			@{$work{$_}->{mailboxes}} = ();
		}

		# untaint all usernames on command line
		/(.+)/;
		push @order, $1;
	}
}

# rewrite specified mailboxes with LITERAL+ so they're safe
for (keys %work) {
	for (@{$work{$_}->{mailboxes}}) {
		my $size = length;
		$_ = "{$size+}\r\n$_";
	}
}

# if fts is disabled, don't bother doing anything
my $conf = `/usr/bin/doveconf -h mail_plugins`;
chomp $conf;
unless (grep { $_ eq "fts" } split(/\s+/, $conf)) {
	myinfo("Full-text search capability disabled; not doing anything.")
		if $opts{verbose};
	exit 0;
}

my $ok = 1;
for my $user (@order) {
	if (!update_fts($user, @{$work{$user}->{mailboxes}})) {
		$ok = 0;
	}
}
if (!$opts{quiet}) {
	my $disp = $ok ? "Done" : "Failed";
	myinfo($disp);
}
exit !$ok;

sub update_fts
{
	my $user = shift;
	my @mailboxes = @_;

	if (!$opts{quiet}) {
		myinfo("Updating search indexes for user $user");
	}

	# start dovecot imap as the user
	my @imapargv = ("/usr/libexec/dovecot/imap", "-u", $user);
	$imappid = open3(\*TO_IMAP, \*FROM_IMAP, \*FROM_IMAP, @imapargv);
	if (!defined($imappid)) {
		mywarn("$imapargv[0]: $!");
		return 0;
	}
	$to_imap = IO::Handle->new_from_fd(*TO_IMAP, "w");
	$from_imap = IO::Handle->new_from_fd(*FROM_IMAP, "r");
	if (!defined($to_imap) || !defined($from_imap)) {
		mywarn("IO::Handle.new_from_fd: $!");
		imap_cleanup();
		return 0;
	}
	$to_imap->autoflush(1);

	# verify greeting
	my $reply;
	while ($reply = $from_imap->getline()) {
		printS($reply) if $opts{verbose};
		$reply =~ s/[\r\n]+$//;
		if ($reply =~ /DEBUG/i || $reply =~ /Growing/) {
			# skip debugging messages
		} elsif ($reply =~ /^\* PREAUTH /) {
			last;
		} else {
			mywarn("bad greeting from IMAP server: $reply");
			imap_cleanup();
			return 0;
		}
	}
	if (read_error($reply)) {
		imap_cleanup();
		return 0;
	}
	my $capability = $reply;

	my $tag = "a";
	my $cmd;

	if (@mailboxes == 0) {
		# list all the mailboxes
		$cmd = qq($tag LIST "" "*"\r\n);
		printC($cmd) if $opts{verbose};
		$to_imap->print($cmd);
		while ($reply = $from_imap->getline()) {
			printS($reply) if $opts{verbose};
			$reply =~ s/[\r\n]+$//;
			if ($reply =~ /^$tag /) {
				if ($reply !~ /$tag OK (\[.*\])?/) {
					mywarn("LIST failed: <$reply>");
					imap_cleanup();
					return 0;
				}
				last;
			} elsif ($reply =~ /^\* LIST \([^\)]*\) (?:"."|NIL) (.*)/) {
				my $mailbox = $1;
				if ($mailbox =~ /^{(\d+)}/) {
					# FIXME: ignore literal size,
					# assume it's one line
					my $size = $1;
					my $mailbox = $from_imap->getline();
					if (read_error($mailbox)) {
						imap_cleanup();
						return 0;
					}
					printS($mailbox) if $opts{verbose};
					$mailbox =~ s/[\r\n]+$//;
					# we'll send mailbox back as LITERAL+
					$mailbox = "{$size+}\r\n$mailbox";
				}

				# keep quotes (if any) and literal (if any) in mailbox name
				push @mailboxes, $mailbox if defined $mailbox;
			}
		}
		if (read_error($reply)) {
			imap_cleanup();
			return 0;
		}
	}

	# rebuild index in every mailbox
	BOX: for my $boxi (0..$#mailboxes) {
		if (!$opts{quiet}) {
			myinfo("Updating search index for user $user" .
				" mailbox " . ($boxi + 1) .
				" of " . scalar(@mailboxes) .
			        " in IMAP process $imappid");
		}
		my $mailbox = $mailboxes[$boxi];

		# EXAMINE don't SELECT the mailbox so we don't reset RECENT
		++$tag;
		$cmd = qq($tag EXAMINE $mailbox\r\n);
		printC($cmd) if $opts{verbose};
		$to_imap->print($cmd);
		while ($reply = $from_imap->getline()) {
			printS($reply) if $opts{verbose};
			$reply =~ s/[\r\n]+$//;
			if ($reply =~ /^$tag /) {
				if ($reply !~ /$tag OK (\[.*\])?/) {
					mywarn("EXAMINE failed: <$reply>");
					next BOX;
				}
				last;
			}
		}
		if (read_error($reply)) {
			imap_cleanup();
			return 0;
		}

		# SEARCH the mailbox for some string; this updates the index
		++$tag;
		$cmd = qq($tag SEARCH BODY XYZZY\r\n);
		printC($cmd) if $opts{verbose};
		$to_imap->print($cmd);
		while ($reply = $from_imap->getline()) {
			printS($reply) if $opts{verbose};
			$reply =~ s/[\r\n]+$//;
			if ($reply =~ /^$tag /) {
				if ($reply !~ /$tag OK (\[.*\])?/) {
					mywarn("SEARCH failed: <$reply>");
				}
				last;
			} elsif ($reply =~ /^\* OK (Indexed.*\%.*)/) {
				if (!$opts{quiet}) {
					myinfo("$1 [$user]");
				}
			} elsif ($reply =~ /Info:/) {
				if (!$opts{quiet}) {
					myinfo($reply);
				}
			} elsif ($reply =~ /(Warning|Error|Fatal|Panic):/ ||
				 $reply =~ /fts_sk/) {
				mywarn($reply);
			}
		}
		if (read_error($reply)) {
			imap_cleanup();
			return 0;
		}

		# perform any deferred expunges
		next unless $capability =~ /\WX-FTS-COMPACT(\W|$)/;
		if (!$opts{quiet}) {
			myinfo("Compacting search index for user $user" .
				" mailbox " . ($boxi + 1) .
				" of " . scalar(@mailboxes) .
			        " in IMAP process $imappid");
		}
		++$tag;
		$cmd = qq($tag X-FTS-COMPACT\r\n);
		printC($cmd) if $opts{verbose};
		$to_imap->print($cmd);
		while ($reply = $from_imap->getline()) {
			printS($reply) if $opts{verbose};
			$reply =~ s/[\r\n]+$//;
			if ($reply =~ /^$tag /) {
				if ($reply !~ /$tag OK (\[.*\])?/) {
					mywarn("X-FTS-COMPACT failed: <$reply>");
				}
				last;
			}
		}
		if (read_error($reply)) {
			imap_cleanup();
			return 0;
		}
	}

	# log out
	++$tag;
	$cmd = qq($tag LOGOUT\r\n);
	printC($cmd) if $opts{verbose};
	$to_imap->print($cmd);
	while ($reply = $from_imap->getline()) {
		printS($reply) if $opts{verbose};
		$reply =~ s/[\r\n]+$//;
		if ($reply =~ /^$tag /) {
			if ($reply !~ /$tag OK (\[.*\])?/) {
				mywarn("LOGOUT failed: <$reply>");
				imap_cleanup();
				return 0;
			}
			last;
		}
	}
	if (read_error($reply)) {
		imap_cleanup();
		return 0;
	}

	# done
	$to_imap->close();
	undef $to_imap;
	$from_imap->close();
	undef $from_imap;
	waitpid($imappid, 0);
	undef $imappid;

	return 1;
}

sub printC
{
	my $msg = shift;
	printX("C", $msg);
}

sub printS
{
	printX("S", @_);
}

sub printX
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

sub read_error
{
	my $input = shift;
	my $error;

	if ($from_imap->error) {
		$error = $!;
	} elsif (!defined($input)) {
		$error = "unexpected EOF";
	}

	if (defined($error)) {
		mywarn("error communicating with imap process $imappid: $error");
		return 1;
	}

	return 0;
}

sub imap_cleanup
{
	if (defined($to_imap)) {
		$to_imap->close();
		undef $to_imap;

		sleep 2;	# maybe it will exit cleanly
	}

	if (defined($from_imap)) {
		$from_imap->close();
		undef $from_imap;
	}

	if (defined($imappid)) {
		kill(9, $imappid);
		waitpid($imappid, 0);
		undef $imappid;
	}
}

sub myfatal
{
	my $msg = shift;

	if ($opts{syslog}) {
		syslog(LOG_ERR, $msg);
	}
	die("$msg\n");
}

sub mywarn
{
	my $msg = shift;

	if ($opts{syslog}) {
		syslog(LOG_WARNING, $msg);
	} else {
		warn(scalar(localtime) . ": $msg\n");
	}
}

sub myinfo
{
	my $msg = shift;

	if ($opts{syslog}) {
		syslog(LOG_INFO, $msg);
	} else {
		print scalar(localtime) . ": $msg\n";
	}
}
