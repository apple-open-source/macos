#! /usr/bin/perl
# Copyright (c) 2007 Apple Inc. All rights reserved.

use File::Basename;

# NOTE: This script assumes that it is being run from the root directory of a
# SimonSays test hierarchy. The commandline passed in @ARGV should be a command
# relative to the TestSuiteName directory.
#
#   TestSuiteName/	=> our cwd
#   TestSuiteName/Tools => where we will create XILog wrappers

# Convert a script name like "foo-bar-baz.sh" into FooBarBaz
sub script_to_xilog
{
    my $scriptname = shift;
    my $xiname = "";
    my @components;

    # Strip a trailing file extension;
    $scriptname =~ s/\.[a-z]{2,3}$//;

    foreach my $part (split /[_-]/, $scriptname) {
	$xiname .= "\u$part";
    }

    return $xiname;
}

sub add_script_symlink
{
    use Cwd;

    my $progname = shift;
    my $xiname = shift;

    my $cwd = getcwd;
    print "cwd=$cwd\n" if ($debug);

    unless (-e $progname) {
	warn "checking for $progname: ", $!;
    }

    chdir "Tools/$xiname" || \
	die "chdir Tools/$xiname: ", $!;

    print "symlink ../../$progname $progname\n" if ($debug);
    symlink "../../$progname", basename($progname) || die $!;

    # We don't want any tool we run to make it's own XILog calls, so we use
    # XILOG_PATH to redirect this somewhere harmless.
    symlink 'null.log', '/dev/null';
    chdir $cwd;
}

my @CMDLINE = @ARGV;
my $CMD = basename $CMDLINE[0];
my $XICMD = script_to_xilog($CMD);

# Should we do extra work to parse smbtorture4 test results?
my $smbtorture = defined($ENV{SMBTORTURE}) ? 1 : 0;

# Should we bother to wrap this in XILog at all?
my $xilogwrap = defined($ENV{NOXILOG}) ? 0 : 1;

my $debug = defined($ENV{DEBUG}) ? 1 : 0;

print "generating XILog tool $XICMD for $CMD\n";

rmdir "Tools/$XICMD";
mkdir "Tools/$XICMD" or die "mkdir Tools/$XICMD: ", $!;

# SimonSays executes Tools/FooScript/FooScript from the Tools/FooScript
# directory, so we need to create a symlink from there back to the original
# script.
add_script_symlink($CMDLINE[0], $XICMD);
$CMDLINE[0] = "./$CMD";

open XILOG, ">Tools/$XICMD/$XICMD" or die "open Tools/$XICMD/$XICMD: ", $!;
print XILOG <<"EOF";
#! /usr/bin/perl
# Copyright (c) 2007 Apple Inc. All rights reserved.

use strict;
use lib "/AppleInternal/Library/Perl";
use XILog qw(:api);

# Preserve environment variables we need
\$ENV{SCRIPTBASE} = '$ENV{SCRIPTBASE}';

my \$debug = defined(\$ENV{DEBUG});

##############################################################################

sub search_for_test
{
    my \$name;

    while (my \$line = <TESTCASE>) {
	chomp \$line;
	if ((\$name) = (\$line =~ m/Running smbtorture test ([\\w-]+)/)) {
	    print "found test \$name\\n" if (\$debug);
	    return \$name;
	}
    }

    return
}

sub process_test
{
    my \$name = shift;
    my \$lines = [];
    my \$status = 'unknown';

    while (my \$line = <TESTCASE>) {
	chomp \$line;

	push \@{\$lines}, \$line;
	if ((\$status) = (\$line =~ m/Finished smbtorture test \\Q\$name\\E: (\\w+)/)) {
	    print "found test \$name\\n" if (\$debug);
	    return (\$status, \$lines);
	}
    }

    return (\$status, \$lines);
}

sub parse_smb_torture_results
{
    my \$xilog = shift;

    while (1) {
	my (\$testname, \$lines, \$status);

	\$testname = search_for_test();
	last unless \$testname;

    	XILogBeginTestCase(\$xilog, \$testname, \$testname);
	(\$status, \$lines) = process_test(\$testname);

	# Push all the output to the log.
	foreach my \$l (\@{\$lines}) {
	    print "XILogMsg(\$l)\\n";
	    XILogMsg(\$l);
	}

	# Fail the test.
	if (\$status ne 'OK') {
	    XILogErr("smbtorture \$testname: \$status");
	}

    	XILogEndTestCase(\$xilog);
    }

    # Note that we do not need to check the exit status because every
    # smbtorture sub-test already prints a status.
    close TESTCASE;
}

sub parse_simple_results
{
    my \$xilog = shift;

    XILogBeginTestCase(\$xilog, "$XICMD", "$XICMD");

    while (my \$line = <TESTCASE>) {
	chomp \$line;

	if (\$line =~ m/FAILED/) {
	    XILogErr(\$line);
	} else {
	    XILogMsg(\$line);
	}
    }


    #  Need to check exit status in case something went wildly wrong
    #  without a FAILED message.
    close TESTCASE
	or \$! ?  XILogErr("pipe error: \$!")
		: XILogErr("failed with exit status: " . (\$? >> 8));

    XILogEndTestCase(\$xilog);
}

##############################################################################

my \$log;
my \$logpath = "Logs/$XICMD.log";

my \$smbtorture = $smbtorture;
my \$xilogwrap = $xilogwrap;

sub failtest
{
    # NB. can't use XILogErr here because we are not in a test case.
    print STDERR \@_, "\\n";
    XILogCloseLog(\$log) if (\$xilogwrap);
    exit 1;
}

# Stick the BigBrother credentials in the environment for scripts that don't
# look at the command line.
\$ENV{USERNAME} = \$ENV{BB_USER};
\$ENV{PASSWORD} = \$ENV{BB_PASS};

if (\$xilogwrap) {
    \$log = XILogOpenLogFromPath(\$kXILogStyleXML, 1, \$logpath)
	or die "opening \$logpath";
    \$ENV{XILOG_PATH} = './null.log';
}

open(TESTCASE, "@CMDLINE 2>&1 |") or failtest "running $CMDLINE[0]: ", \$!;

if (\$xilogwrap) {
    if (\$smbtorture) {
	parse_smb_torture_results(\$log);
    } else {
	parse_simple_results(\$log);
    }

    XILogCloseLog(\$log);
} else {
    # Just echo the tool's output
    print <TESTCASE>;
}

exit 0;

EOF

chmod 0755, "Tools/$XICMD/$XICMD";
exit 0;
