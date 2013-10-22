#!/usr/bin/perl -w

use blib;
use IO::KQueue;
use IO::Socket;

my $server = IO::Socket::INET->new(
    LocalAddr => "127.0.0.1:3069",
    Listen => 10,
    Blocking => 0,
    Reuse => 1,
    );

IO::Handle::blocking($server, 0);

my $kq = IO::KQueue->new();

$kq->EV_SET(fileno($server), EVFILT_READ, EV_ADD, 0, 5);

my %users;
while (1) {
    my @ret = $kq->kevent();
    if (!@ret) {
        die "No kevents: $!";
    }
    
    foreach my $kevent (@ret) {
        my $fd = $kevent->[KQ_IDENT];
        
        if ($fd == fileno($server)) {
            my $client = $server->accept();
            IO::Handle::blocking($client, 0);
            if (!$client) {
                warn("accept() failed: $!");
                next;
            }
            
            $kq->EV_SET(fileno($client), EVFILT_READ, EV_ADD);
            
            $users{fileno($client)} = $client;
        }
        else {
            my $client = $users{$fd} || die "Unknown fd: $fd";
            my $buf;
            my $blen = $client->read($buf, 8096);
            
            if (!$blen) {
                # eof
                delete $users{$fd};
                $client->close;
            }
            elsif ($blen == -1) {
                # error
                die "read error on $fd: $!";
            }
            else {
                foreach my $f (keys %users) {
                    next if $f == $fd;
                    $users{$f}->print($buf);
                }
            }
        }
    }
}