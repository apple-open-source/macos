#!/usr/bin/perl -w

use IO::KQueue;
use Getopt::Std;

my $START = 10;

my %opts;

getopts('h', \%opts);
help() if $opts{h};

my $file = shift(@ARGV) || "-";
open(my $fh, $file) || die "$0: open($file): $!";

my @buf;
while (<$fh>) {
    $buf[$. % $START] = $_;
}
my @tail = (@buf[ ($. % $START + 1) .. $#buf ], 
			@buf[  0 .. $. % $START ]);
for (@tail) {
    print if $_; # @tail may begin or end with undef
}

my $kq = IO::KQueue->new();

$kq->EV_SET(fileno($fh), EVFILT_READ, EV_ADD, 0, 0, \&read_file);
$kq->EV_SET(fileno($fh), EVFILT_VNODE, EV_ADD, NOTE_DELETE | NOTE_RENAME, 0, \&reopen_file);

while (1) {
    my @events = $kq->kevent();
    
    foreach my $kevent (@events) {
        my $sub = $kevent->[KQ_UDATA];
        $sub->($kevent) if ref($sub) eq 'CODE';
    }
}

sub read_file {
    my $kev = shift;
    
    if ($kev->[KQ_DATA] < 0) {
        print "$file has shrunk\n";
        seek($fh, 0, 2);
        return;
    }
    
    while (<$fh>) {
        print;
    }
}

sub reopen_file {
    # renamed or deleted...
    close($fh);
    open($fh, $file) || die "$0: $file went away\n";
    $kq->EV_SET(fileno($fh), EVFILT_READ, EV_ADD, 0, 0, \&read_file);
    $kq->EV_SET(fileno($fh), EVFILT_VNODE, EV_ADD, NOTE_DELETE | NOTE_RENAME, 0, \&reopen_file);
}

sub help {
    print <<EOT;
$0 [-h] [file]

Tail a file forever, like tail -F.
EOT
    exit(0);
}
