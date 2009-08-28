#!perl -w
# vim:sw=4:ts=8

use strict;

use Test::More tests => 21;

## ----------------------------------------------------------------------------
## 09trace.t
## ----------------------------------------------------------------------------
#
## ----------------------------------------------------------------------------

BEGIN {
    use_ok( 'DBI' );
}

$|=1;

package PerlIO::via::TraceDBI;

our $logline;

sub OPEN {
	return 1;
}

sub PUSHED
{
	my ($class,$mode,$fh) = @_;
	# When writing we buffer the data
	my $buf = '';
	return bless \$buf,$class;
}

sub FILL
{
	my ($obj,$fh) = @_;
	return $logline;
}

sub READLINE
{
	my ($obj,$fh) = @_;
	return $logline;
}

sub WRITE
{
	my ($obj,$buf,$fh) = @_;
#	print "\n*** WRITING $buf\n";
	$logline = $buf;
	return length($buf);
}

sub FLUSH
{
	my ($obj,$fh) = @_;
	return 0;
}

sub CLOSE {
#	print "\n*** CLOSING!!!\n";
	$logline = "**** CERRADO! ***";
	return -1;
}

1;

package PerlIO::via::MyFancyLogLayer;

sub OPEN {
	my ($obj, $path, $mode, $fh) = @_;
	$$obj = $path;
	return 1;
}

sub PUSHED
{
	my ($class,$mode,$fh) = @_;
	# When writing we buffer the data
	my $logger;
	return bless \$logger,$class;
}

sub WRITE
{
	my ($obj,$buf,$fh) = @_;
	$$obj->log($buf);
	return length($buf);
}

sub FLUSH
{
	my ($obj,$fh) = @_;
	return 0;
}

sub CLOSE {
	my $self = shift;
	$$self->close();
	return 0;
}

1;

package MyFancyLogger;

use Symbol qw(gensym);

sub new
{
	my $self = {};
	my $fh = gensym();
	open $fh, '>', 'fancylog.log';
	$self->{_fh} = $fh;
	$self->{_buf} = '';
	return bless $self, shift;
}

sub log
{
	my $self = shift;
	my $fh = $self->{_fh};
	$self->{_buf} .= shift;
	print $fh "At ", scalar localtime(), ':', $self->{_buf}, "\n" and
	$self->{_buf} = ''
		if $self->{_buf}=~tr/\n//;
}

sub close {
	my $self = shift;
	return unless exists $self->{_fh};
	my $fh = $self->{_fh};
	print $fh "At ", scalar localtime(), ':', $self->{_buf}, "\n" and
	$self->{_buf} = ''
		if $self->{_buf};
	close $fh;
	delete $self->{_fh};
}

1;

package main;

## ----------------------------------------------------------------------------
# Connect to the example driver.

my $dbh = DBI->connect('dbi:ExampleP:dummy', '', '',
                           { PrintError => 0,
                             RaiseError => 1,
                             PrintWarn => 1,
                           });
isa_ok( $dbh, 'DBI::db' );

# Clean up when we're done.
END { $dbh->disconnect if $dbh };

## ----------------------------------------------------------------------------
# Check the database handle attributes.

cmp_ok($dbh->{TraceLevel}, '==', $DBI::dbi_debug & 0xF, '... checking TraceLevel attribute');

my $trace_file = "dbitrace.log";

1 while unlink $trace_file;

my $tracefd;
## ----------------------------------------------------------------------------
# First use regular filehandle
open $tracefd, '>>', $trace_file;

my $oldfd = select($tracefd);
$| = 1;
select $oldfd;

ok(-f $trace_file, '... regular fh: trace file successfully created');

$dbh->trace(2, $tracefd);
ok( 1, '... regular fh:  filehandle successfully set');

#
#	read current size of file
#
my $filesz = (stat $tracefd)[7];
$dbh->trace_msg("First logline\n", 1);
#
#	read new file size and verify its different
#
my $newfsz = (stat $tracefd)[7];
SKIP: {
    skip 'on VMS autoflush using select does not work', 1 if $^O eq 'VMS';
    ok(($filesz != $newfsz), '... regular fh: trace_msg');
}

$dbh->trace(undef, "STDOUT");	# close $trace_file
ok(-f $trace_file, '... regular fh: file successfully changed');

$filesz = (stat $tracefd)[7];
$dbh->trace_msg("Next logline\n");
#
#	read new file size and verify its same
#
$newfsz = (stat $tracefd)[7];
ok(($filesz == $newfsz), '... regular fh: trace_msg after changing trace output');

#1 while unlink $trace_file;

$dbh->trace(0);	# disable trace

SKIP: {
	eval { require 5.008; };
	skip "Layered I/O not available in Perl $^V", 13
		if $@;
## ----------------------------------------------------------------------------
# Then use layered filehandle
#
open TRACEFD, '+>:via(TraceDBI)', 'layeredtrace.out';
print TRACEFD "*** Test our layer\n";
my $result = <TRACEFD>;
is $result, "*** Test our layer\n",	"... layered fh: file is layered: $result\n";

$dbh->trace(1, \*TRACEFD);
ok( 1, '... layered fh:  filehandle successfully set');

$dbh->trace_msg("Layered logline\n", 1);

$result = <TRACEFD>;
is $result, "Layered logline\n", "... layered fh: trace_msg: $result\n";

$dbh->trace(1, "STDOUT");	# close $trace_file
$result = <TRACEFD>;
is $result,	"Layered logline\n", "... layered fh: close doesn't close: $result\n";

$dbh->trace_msg("Next logline\n", 1);
$result = <TRACEFD>;
is $result, "Layered logline\n", "... layered fh: trace_msg after change trace output: $result\n";

## ----------------------------------------------------------------------------
# Then use scalar filehandle
#
my $tracestr;
open TRACEFD, '+>:scalar', \$tracestr;
print TRACEFD "*** Test our layer\n";
ok 1,	"... scalar trace: file is layered: $tracestr\n";

$dbh->trace(1, \*TRACEFD);
ok 1, '... scalar trace: filehandle successfully set';

$dbh->trace_msg("Layered logline\n", 1);
ok 1, "... scalar trace: $tracestr\n";

$dbh->trace(1, "STDOUT");	# close $trace_file
ok 1, "... scalar trace: close doesn't close: $tracestr\n";

$dbh->trace_msg("Next logline\n", 1);
ok 1, "... scalar trace: after change trace output: $tracestr\n";

## ----------------------------------------------------------------------------
# Then use fancy logger
#
open my $fh, '>:via(MyFancyLogLayer)', MyFancyLogger->new();

$dbh->trace('SQL', $fh);

$dbh->trace_msg("Layered logline\n", 1);
ok 1, "... logger: trace_msg\n";

$dbh->trace(1, "STDOUT");	# close $trace_file
ok 1, "... logger: close doesn't close\n";

$dbh->trace_msg("Next logline\n", 1);
ok 1, "... logger: trace_msg after change trace output\n";

close $fh;

1 while unlink 'fancylog.log';

}

1;

# end
