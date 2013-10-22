use warnings;
use strict;

use Config;
BEGIN {
	# open2/3 supported on win32, but not Borland due to CRT bugs
	if(!$Config{d_fork} &&
			(($^O ne 'MSWin32' && $^O ne 'NetWare') ||
			 $Config{cc} =~ /^bcc/i)) {
		require Test::More;
		Test::More->import(skip_all =>
			"open2/3 not available with MSWin32+Netware+cc=bcc");
	}
}

BEGIN {
	# make warnings fatal
	$SIG{__WARN__} = sub { die @_ };
}

use IO::Handle;
use Test::More tests => 8;

require_ok "open2.pl";

my $perl = $^X;

sub cmd_line {
	if ($^O eq 'MSWin32' || $^O eq 'NetWare') {
		return qq/"$_[0]"/;
	}
	else {
		return $_[0];
	}
}

my ($pid, $reaped_pid);
STDOUT->autoflush;
STDERR->autoflush;

$pid = &open2('READ', 'WRITE', $^X, '-e', cmd_line('print scalar <STDIN>'));
ok $pid;
ok print(WRITE "hi kid\n");
like scalar(<READ>), qr/\Ahi kid\r?\n\z/;
ok close(WRITE);
ok close(READ);
$reaped_pid = waitpid $pid, 0;
is $reaped_pid, $pid;
is $?, 0;

1;
