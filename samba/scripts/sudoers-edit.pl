#! /usr/bin/perl

# Copyright (C) 2007 Apple Inc. All rights reserved.
# sudoers-quota.pl: automated interface to editing /etc/sudoers

use strict;
use File::Basename;
use Getopt::Long;

my $fname;
my $user = $ENV{SUDO_USER};
my $grantprivs = 1;
my @envlist;

$0 = basename($0);

sub usage
{
    print STDERR <<"EOF";
Usage: $0 [OPTIONS] FILE
Grant password-free sudo privileges to a user.

Recognized environment variables:
    SUDO_USER (mandatory, set by sudo)

Options:
    --environment=LIST  Allow the listed environment variables.
    --remove            Remove user privileges.
    --verbose           Be verbose.
    --help              Print this message.
EOF
}

sub rewind
{
    my $fh = shift;
    seek $fh, 0, $IO::Seekable::SEEK_SET;
}

GetOptions( 'verbose' => sub { $ENV{DEBUG} = "y" },
    'help' => sub { usage(); exit 0 },
    'remove' => sub { $grantprivs = 0 },
    'environment=s' => \@envlist
);

# Allow multiple comma-separated values in @envlist.
@envlist = split(/,/, join(',', @envlist));

# Make sure we always add $EDITOR to the allowed environment
push(@envlist, 'EDITOR') unless (grep { $_ eq 'EDITOR'} @envlist);

unless ($fname = shift and -w $fname and $user) {
	usage();
	exit 1;
}

open my $sudoers, "+<$fname" or die "failed to open $fname: $!";

my @lines = <$sudoers>;

# Remove any of our lines. We can only have one user configured at a time.
@lines = grep {! /\Q$0\E/ } @lines;

rewind $sudoers;
truncate $sudoers, 0;
for my $line (@lines) {
	print $sudoers $line;
}

if ($grantprivs) {
    print $sudoers <<"EOF";
# $0: Sudo privileges granted to $user by the Samba test suite.
User_Alias SAMBAQA = $user # $0
SAMBAQA ALL=(ALL) NOPASSWD: ALL # $0
Defaults:SAMBAQA env_reset # $0
EOF

    foreach my $var (@envlist) {
	print $sudoers <<"EOF"
Defaults:SAMBAQA env_keep += \"$var\" # $0
EOF
    }

}

# Dump the file we wrote to make sure we did the right thing.
if (defined($ENV{DEBUG})) {
    rewind $sudoers;
    while (my $line = <$sudoers>) {
	print STDOUT $line;
    }
}

close $sudoers;

exit 0
