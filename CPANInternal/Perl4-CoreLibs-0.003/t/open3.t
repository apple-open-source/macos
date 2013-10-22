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
use Test::More tests => 23;

require_ok "open3.pl";

sub cmd_line {
	if ($^O eq 'MSWin32' || $^O eq 'NetWare') {
		my $cmd = shift;
		$cmd =~ tr/\r\n//d;
		$cmd =~ s/"/\\"/g;
		return qq/"$cmd"/;
	}
	else {
		return $_[0];
	}
}

my ($pid, $reaped_pid);
STDOUT->autoflush;
STDERR->autoflush;

# basic
$pid = &open3('WRITE', 'READ', 'ERROR', $^X, '-e', cmd_line(<<'EOF'));
	$| = 1;
	print scalar <STDIN>;
	print STDERR "hi error\n";
EOF
ok $pid;
print WRITE "hi kid\n";
like scalar(<READ>), qr/\Ahi kid\r?\n\z/;
like scalar(<ERROR>), qr/\Ahi error\r?\n\z/;
ok close(WRITE);
ok close(READ);
ok close(ERROR);
$reaped_pid = waitpid $pid, 0;
is $reaped_pid, $pid;
is $?, 0;

# read and error together, both named
$pid = &open3('WRITE', 'READ', 'READ', $^X, '-e', cmd_line(<<'EOF'));
	$| = 1;
	print scalar <STDIN>;
	print STDERR scalar <STDIN>;
EOF
print WRITE "wibble\n";
like scalar(<READ>), qr/\Awibble\r?\n\z/;
print WRITE "wobble\n";
like scalar(<READ>), qr/\Awobble\r?\n\z/;
waitpid $pid, 0;

# read and error together, error empty
$pid = &open3('WRITE', 'READ', '', $^X, '-e', cmd_line(<<'EOF'));
	$| = 1;
	print scalar <STDIN>;
	print STDERR scalar <STDIN>;
EOF
print WRITE "wibble\n";
like scalar(<READ>), qr/\Awibble\r?\n\z/;
print WRITE "wobble\n";
like scalar(<READ>), qr/\Awobble\r?\n\z/;
waitpid $pid, 0;

# dup writer
ok pipe(PIPE_READ, PIPE_WRITE);
$pid = &open3('<&PIPE_READ', 'READ', '', $^X, '-e', 'print scalar <STDIN>');
close PIPE_READ;
print PIPE_WRITE "wibble\n";
close PIPE_WRITE;
like scalar(<READ>), qr/\Awibble\r?\n\z/;
waitpid $pid, 0;

# dup reader
$pid = &open3('WRITE', 'READ', 'ERROR', $^X, '-e', cmd_line(<<'EOF'));
	$| = 1;
	sub cmd_line {
		$^O eq 'MSWin32' || $^O eq 'NetWare' ? qq/"$_[0]"/ : $_[0];
	}
	require "open3.pl";
	$pid = &open3('WRITE', '>&STDOUT', 'ERROR', $^X, '-e',
		cmd_line('print scalar <STDIN>'));
	print WRITE "wibble\n";
	waitpid $pid, 0;
EOF
like scalar(<READ>), qr/\Awibble\r?\n\z/;
waitpid $pid, 0;

# dup error:  This particular case, duping stderr onto the existing
# stdout but putting stdout somewhere else, is a good case because it
# used not to work.
$pid = &open3('WRITE', 'READ', 'ERROR', $^X, '-e', cmd_line(<<'EOF'));
	$| = 1;
	sub cmd_line {
		$^O eq 'MSWin32' || $^O eq 'NetWare' ? qq/"$_[0]"/ : $_[0];
	}
	require "open3.pl";
	$pid = &open3('WRITE', 'READ', '>&STDOUT', $^X, '-e',
		cmd_line('print STDERR scalar <STDIN>'));
	print WRITE "wibble\n";
	waitpid $pid, 0;
EOF
like scalar(<READ>), qr/\Awibble\r?\n\z/;
waitpid $pid, 0;

# dup reader and error together, both named
$pid = &open3('WRITE', 'READ', 'ERROR', $^X, '-e', cmd_line(<<'EOF'));
	$| = 1;
	sub cmd_line {
		$^O eq 'MSWin32' || $^O eq 'NetWare' ? qq/"$_[0]"/ : $_[0];
	}
	require "open3.pl";
	$pid = &open3('WRITE', '>&STDOUT', '>&STDOUT', $^X, '-e',
		cmd_line('$|=1; print STDOUT scalar <STDIN>; print STDERR scalar <STDIN>'));
	print WRITE "wibble\n";
	print WRITE "wobble\n";
	waitpid $pid, 0;
EOF
like scalar(<READ>), qr/\Awibble\r?\n\z/;
like scalar(<READ>), qr/\Awobble\r?\n\z/;
waitpid $pid, 0;

# dup reader and error together, error empty
$pid = &open3('WRITE', 'READ', 'ERROR', $^X, '-e', cmd_line(<<'EOF'));
	$| = 1;
	sub cmd_line {
		$^O eq 'MSWin32' || $^O eq 'NetWare' ? qq/"$_[0]"/ : $_[0];
	}
	require "open3.pl";
	$pid = &open3('WRITE', '>&STDOUT', '', $^X, '-e',
		cmd_line('$|=1; print STDOUT scalar <STDIN>; print STDERR scalar <STDIN>'));
	print WRITE "wibble\n";
	print WRITE "wobble\n";
	waitpid $pid, 0;
EOF
like scalar(<READ>), qr/\Awibble\r?\n\z/;
like scalar(<READ>), qr/\Awobble\r?\n\z/;
waitpid $pid, 0;

# command line in single parameter variant of open3
# for understanding of Config{'sh'} test see exec description in camel book
my $cmd = 'print(scalar(<STDIN>))';
$cmd = $Config{'sh'} =~ /sh/ ? "'$cmd'" : cmd_line($cmd);
eval{$pid = &open3('WRITE', 'READ', 'ERROR', "$^X -e " . $cmd); };
is $@, "";
print WRITE "wibble\n";
like scalar(<READ>), qr/\Awibble\r?\n\z/;
waitpid $pid, 0;

1;
