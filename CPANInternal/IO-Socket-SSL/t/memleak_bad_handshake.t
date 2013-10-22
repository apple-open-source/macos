#!perl -w
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl t/nonblock.t'


use Net::SSLeay;
use Socket;
use IO::Socket::SSL;
use IO::Select;
use Errno qw(EAGAIN EINPROGRESS );
use strict;

if ( grep { $^O =~m{$_}i } qw( MacOS VOS vmesa riscos amigaos mswin32) ) {
    print "1..0 # Skipped: ps not implemented on this platform\n";
    exit
}

if ( $^O =~m{aix}i ) {
    print "1..0 # Skipped: might hang, see https://rt.cpan.org/Ticket/Display.html?id=72170\n";
    exit
}


$|=1;
use vars qw( $SSL_SERVER_ADDR );
do "t/ssl_settings.req" || do "ssl_settings.req";

if ( ! getsize($$) ) {
	print "1..0 # Skipped: no usable ps\n";
	exit;
}

my $server = IO::Socket::SSL->new(
	LocalAddr => $SSL_SERVER_ADDR,
	Listen => 200,
	ReuseAddr => 1,
);
my $addr = $SSL_SERVER_ADDR.':'.$server->sockport;

defined( my $pid = fork()) or do {
	print "1..0 # Skipped: fork failed\n";
	goto done;
};

if ( $pid == 0 ) {
	# server
	while (1) {
		# socket accept, client handshake and client close 
		$server->accept;
	}
	goto done;
}


close($server);
# plain non-SSL connect and close w/o sending data
for(1..100) {
	IO::Socket::INET->new( $addr ) or next;
}
my $size100 = getsize($pid);
if ( ! $size100 ) {
	print "1..0 # Skipped: cannot get size of child process\n";
	goto done;
}

for(100..200) {
	IO::Socket::INET->new( $addr ) or next;
}
my $size200 = getsize($pid);

for(200..300) {
	IO::Socket::INET->new( $addr ) or next;
}
my $size300 = getsize($pid);
if ($size100>$size200 or $size200<$size300) {;
	print "1..0 # skipped  - do we measure the right thing?\n";
	goto done;
}

print "1..1\n";
print "not " if $size100 < $size200 and $size200 < $size300;
print "ok # check memleak failed handshake ($size100,$size200,$size300)\n";

done:
kill(9,$pid);
wait;
exit;


sub getsize {
	my $pid = shift;
	open( my $ps,'-|',"ps -o vsize -p $pid 2>/dev/null" ) or return;
	$ps && <$ps> or return; # header
	return int(<$ps>); # size
}

