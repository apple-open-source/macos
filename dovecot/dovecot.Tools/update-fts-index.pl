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

my $queue_dir = "/private/var/db/dovecot.fts.update";

$ENV{PATH} = "/usr/bin:/bin:/usr/sbin:/sbin";
delete $ENV{CDPATH};

my $imappid;
my $to_imap;
my $from_imap;
local $SIG{__DIE__} = \&imap_cleanup;

# if fts is disabled, don't bother doing anything
my $conf = `/usr/bin/doveconf -h mail_plugins`;
chomp $conf;
my $noop = 0;
unless (grep { $_ eq "fts" } split(/\s+/, $conf)) {
	myinfo("Full-text search capability disabled; not doing anything.")
		if $opts{verbose};

	# Need to remove the queue files if applicable, which involves
	# untainting and sanity-checking the file names.
	$noop = 1;
}

my $ok = 1;
if (defined($opts{queued})) {
	opendir(DIR, $queue_dir) or myfatal("$queue_dir: $!");
	my @entries = readdir(DIR);
	closedir(DIR);

	# slurp and unescape the list of files in the directory
	my %work;
	for (@entries) {
		next if $_ eq "." or $_ eq "..";

		# untaint: fts creates files with only alpha, num, %, and .
		# we rename files to .files during processing
		if (!/^(\.?([a-zA-Z0-9%]+)\.([a-zA-Z0-9%]+))$/) {
			mywarn("$queue_dir/$_: malformed or unsafe name");
			next;
		}
		my $name = $1;
		my $user = $2;
		my $mailbox = $3;

		next unless defined $user and defined $mailbox;
		$user =~ s/%([a-fA-F0-9]{2})/chr(hex($1))/ge;
		$mailbox =~ s/%([a-fA-F0-9]{2})/chr(hex($1))/ge;

		push @{$work{$user}->{mailboxes}}, $mailbox;
		$work{$user}->{queuefiles}->{$mailbox} = $name;
		$work{$user}->{order} = rand;
	}

	# process the users in random order so nobody gets preference
	my @order = sort { $work{$a}->{order} <=> $work{$b}->{order} }
			 keys %work;
	for my $user (@order) {
		if ($noop ||
		    update_fts_with_retries($user, \&preserve_queuefile_for,
					    \&delete_queuefile_for,
					    $work{$user},
					    @{$work{$user}->{mailboxes}}) <= 0) {
			$ok = 0;

			# delete all queuefiles on error or FTS disabled
			for (keys %{$work{$user}->{queuefiles}}) {
				my $queuefile = $work{$user}->{queuefiles}->{$_};
				if (!unlink("$queue_dir/$queuefile")) {
					mywarn("$queue_dir/$queuefile: $!")
					    unless $!{ENOENT};
				}
			}
		}
	}
}

if ($noop) {
	exit 0;
}

if (!defined($opts{queued})) {
	for (@ARGV) {
		my @mailboxes;
		if (defined($opts{mailbox})) {
			@mailboxes = @{$opts{mailbox}};
		} else {
			@mailboxes = ();
		}

		# untaint all usernames on command line
		/(.+)/;
		my $user = $1;

		if (update_fts($user, undef, undef, undef, @mailboxes) <= 0) {
			$ok = 0;
		}
	}
}

if (!$opts{quiet}) {
	my $disp = $ok ? "Done" : "Failed";
	myinfo($disp);
}
exit !$ok;

sub update_fts_with_retries
{
	my @args = @_;

	for (my $tries = 3; --$tries >= 0; ) {
		my $r = update_fts(@args);
		return $r if $r >= 0;

		if ($tries > 0) {
			# maybe dovecot isn't running yet (during boot)
			myinfo("Will retry in a minute");
			sleep(60);
		} else {
			myinfo("Giving up");
		}
	}

	return 0;
}

sub update_fts
{
	my $user = shift;
	my $preupdate_func = shift;
	my $postupdate_func = shift;
	my $func_context = shift;
	my @mailboxes = @_;

	# rewrite specified mailboxes with LITERAL+ so they're safe
	my @literal_mailboxes = @mailboxes;
	for (@literal_mailboxes) {
		my $size = length;
		$_ = "{$size+}\r\n$_";
	}

	if (!$opts{quiet}) {
		myinfo("Updating search indexes for user $user");
	}

	# start dovecot imap as the user
	my @imapargv = ("/usr/libexec/dovecot/imap", "-u", $user);
	$imappid = open3(\*TO_IMAP, \*FROM_IMAP, \*FROM_IMAP, @imapargv);
	if (!defined($imappid)) {
		mywarn("$imapargv[0]: $!");
		return -1;
	}
	$to_imap = IO::Handle->new_from_fd(*TO_IMAP, "w");
	$from_imap = IO::Handle->new_from_fd(*FROM_IMAP, "r");
	if (!defined($to_imap) || !defined($from_imap)) {
		mywarn("IO::Handle.new_from_fd: $!");
		imap_cleanup();
		return -1;
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
			return -1;
		}
	}
	if (read_error($reply)) {
		imap_cleanup();
		return -1;
	}
	my $capability = $reply;

	# at this point we're logged in so failures should return 0 not -1

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
		@literal_mailboxes = @mailboxes;
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
		my $literal_mailbox = $literal_mailboxes[$boxi];

		&$preupdate_func($func_context, $mailbox)
		    if defined $preupdate_func;

		# EXAMINE don't SELECT the mailbox so we don't reset \Recent
		++$tag;
		$cmd = qq($tag EXAMINE $literal_mailbox\r\n);
		printC($cmd) if $opts{verbose};
		$to_imap->print($cmd);
		while ($reply = $from_imap->getline()) {
			printS($reply) if $opts{verbose};
			$reply =~ s/[\r\n]+$//;
			if ($reply =~ /^$tag /) {
				if ($reply !~ /$tag OK (\[.*\])?/) {
					mywarn("EXAMINE failed: <$reply>");
					&$postupdate_func($func_context,
							  $mailbox)
					    if defined $postupdate_func;
					next BOX;
				}
				last;
			}
		}
		if (read_error($reply)) {
			imap_cleanup();
			&$postupdate_func($func_context, $mailbox)
			    if defined $postupdate_func;
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
			&$postupdate_func($func_context, $mailbox)
			    if defined $postupdate_func;
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
			&$postupdate_func($func_context, $mailbox)
			    if defined $postupdate_func;
			return 0;
		}

		&$postupdate_func($func_context, $mailbox)
		    if defined $postupdate_func;
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

# rename queuefile to .queuefile before updating a mailbox's index so that:
# - if the system reboots in the middle we will resume
# - if another update request comes in during indexing (queuefile is recreated)
#   we will be run again
sub preserve_queuefile_for
{
	my $userref = shift;
	my $mailbox = shift;

	my $queuefile = $userref->{queuefiles}->{$mailbox};
	if (defined($queuefile) && $queuefile !~ /^\./ &&
	    !rename("$queue_dir/$queuefile", "$queue_dir/.$queuefile")) {
		# maybe another process got it first
		mywarn("rename $queue_dir/$queuefile -> $queue_dir/.$queuefile: $!")
		    unless $!{ENOENT};
	}
}
			
# delete .queuefile after updating a mailbox's index
sub delete_queuefile_for
{
	my $userref = shift;
	my $mailbox = shift;

	my $queuefile = $userref->{queuefiles}->{$mailbox};
	$queuefile = ".$queuefile" unless $queuefile =~ /^\./;
	if (defined($queuefile) && !unlink("$queue_dir/$queuefile")) {
		# maybe another process got it first
		mywarn("$queue_dir/$queuefile: $!") unless $!{ENOENT};
	}
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
