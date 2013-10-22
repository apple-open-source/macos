package IO::KQueue;

use strict;
use vars qw($VERSION @ISA @EXPORT $AUTOLOAD $MAX_EVENTS);

use DynaLoader ();
use Exporter ();

use Errno;

BEGIN {
$VERSION = '0.34';

$MAX_EVENTS = 1000;

@ISA = qw(Exporter DynaLoader);
@EXPORT = qw(
    EV_ADD
    EV_DELETE
    EV_ENABLE
    EV_DISABLE
    EV_ONESHOT
    EV_CLEAR
    EV_EOF
    EV_ERROR
    EVFILT_READ
    EVFILT_WRITE
    EVFILT_VNODE
    EVFILT_PROC
    EVFILT_SIGNAL
    EVFILT_TIMER
    EVFILT_FS
    NOTE_LOWAT
    NOTE_DELETE
    NOTE_WRITE
    NOTE_EXTEND
    NOTE_ATTRIB
    NOTE_LINK
    NOTE_RENAME
    NOTE_REVOKE
    NOTE_EXIT
    NOTE_FORK
    NOTE_EXEC
    NOTE_PCTRLMASK
    NOTE_PDATAMASK
    NOTE_TRACK
    NOTE_TRACKERR
    NOTE_CHILD
    KQ_IDENT
    KQ_FILTER
    KQ_FLAGS
    KQ_FFLAGS
    KQ_DATA
    KQ_UDATA
);

bootstrap IO::KQueue $VERSION;
}

use constant EV_ADD => (constant('EV_ADD'))[1];
use constant EV_DELETE => (constant('EV_DELETE'))[1];
use constant EV_ENABLE => (constant('EV_ENABLE'))[1];
use constant EV_DISABLE => (constant('EV_DISABLE'))[1];
use constant EV_ONESHOT => (constant('EV_ONESHOT'))[1];
use constant EV_CLEAR => (constant('EV_CLEAR'))[1];
use constant EV_EOF => (constant('EV_EOF'))[1];
use constant EV_ERROR => (constant('EV_ERROR'))[1];
use constant EVFILT_READ => (constant('EVFILT_READ'))[1];
use constant EVFILT_WRITE => (constant('EVFILT_WRITE'))[1];
use constant EVFILT_VNODE => (constant('EVFILT_VNODE'))[1];
use constant EVFILT_PROC => (constant('EVFILT_PROC'))[1];
use constant EVFILT_SIGNAL => (constant('EVFILT_SIGNAL'))[1];
use constant EVFILT_TIMER => (constant('EVFILT_TIMER'))[1];
use constant EVFILT_FS => (constant('EVFILT_FS'))[1];
use constant NOTE_LOWAT => (constant('NOTE_LOWAT'))[1];
use constant NOTE_DELETE => (constant('NOTE_DELETE'))[1];
use constant NOTE_WRITE => (constant('NOTE_WRITE'))[1];
use constant NOTE_EXTEND => (constant('NOTE_EXTEND'))[1];
use constant NOTE_ATTRIB => (constant('NOTE_ATTRIB'))[1];
use constant NOTE_LINK => (constant('NOTE_LINK'))[1];
use constant NOTE_RENAME => (constant('NOTE_RENAME'))[1];
use constant NOTE_REVOKE => (constant('NOTE_REVOKE'))[1];
use constant NOTE_EXIT => (constant('NOTE_EXIT'))[1];
use constant NOTE_FORK => (constant('NOTE_FORK'))[1];
use constant NOTE_EXEC => (constant('NOTE_EXEC'))[1];
use constant NOTE_PCTRLMASK => (constant('NOTE_PCTRLMASK'))[1];
use constant NOTE_PDATAMASK => (constant('NOTE_PDATAMASK'))[1];
use constant NOTE_TRACK => (constant('NOTE_TRACK'))[1];
use constant NOTE_TRACKERR => (constant('NOTE_TRACKERR'))[1];
use constant NOTE_CHILD => (constant('NOTE_CHILD'))[1];

use constant KQ_IDENT => 0;
use constant KQ_FILTER => 1;
use constant KQ_FLAGS => 2;
use constant KQ_FFLAGS => 3;
use constant KQ_DATA => 4;
use constant KQ_UDATA => 5;

sub DESTROY {
}

sub AUTOLOAD {
    my $sub = $AUTOLOAD;
    (my $constname = $sub) =~ s/.*:://;
    my ($err, $val) = constant($constname);
    if (defined($err)) {
        die $err;
    }
    eval "sub $sub () { $val }";
    goto &$sub;
}

1;

__END__

=head1 NAME

IO::KQueue - perl interface to the BSD kqueue system call

=head1 SYNOPSIS

    my $kq = IO::KQueue->new();
    $kq->EV_SET($fd, EVFILT_READ, EV_ADD, 0, 5);
    
    my %results = $kq->kevent($timeout);

=head1 DESCRIPTION

This module provides a fairly low level interface to the BSD kqueue() system
call, allowing you to monitor for changes on sockets, files, processes and
signals.

Usage is very similar to the kqueue system calls, so you will need to have
read and understood the kqueue man page. This document may seem fairly light on
details but that is merely because the details are in the man page, and so I
don't wish to replicate them here.

=head1 API

=head2 C<< IO::KQueue->new() >>

Construct a new KQueue object (maps to the C<kqueue()> system call).

=head2 C<< $kq->EV_SET($ident, $filt, $flags, $fflags, $data, $udata) >>

e.g.:

  $kq->EV_SET(fileno($server), EVFILT_READ, EV_ADD, 0, 5);

Equivalent to the EV_SET macro followed immediately by a call to kevent() to
set the event up.

Note that to watch for both I<read> and I<write> events you need to call this
method twice - once with EVFILT_READ and once with EVFILT_WRITE - unlike
C<epoll()> and C<poll()> these "filters" are not a bitmask.

Returns nothing. Throws an exception on failure.

The C<$fflags>, C<$data> and C<$udata> params are optional.

=head2 C<< $kq->kevent($timeout) >>

Poll for events on the kqueue. Timeout is in milliseconds. If timeout is
ommitted then we wait forever until there are events to read. If timeout is
zero then we return immediately.

Returns a list of arrayrefs which contain the kevent. The contents of the kevent
are:

=over 4

=item * C<< $kevent->[KQ_IDENT] >>

=item * C<< $kevent->[KQ_FILTER] >>

=item * C<< $kevent->[KQ_FLAGS] >>

=item * C<< $kevent->[KQ_FFLAGS] >>

=item * C<< $kevent->[KQ_DATA] >>

=item * C<< $kevent->[KQ_UDATA] >>

See the included F<tail.pl> and F<chat.pl> scripts for example usage, and see
the kqueue man pages for full details.

=head1 CONSTANTS

For a list of exported constants see the source of F<Makefile.PL>, or the
kqueue man page. In addition the C<KQ_*> entries of the kevent are also
exported - see the list above.

=head1 LICENSE

This is free software. You may use it and distribute it under the same terms
as Perl itself - i.e. either the Perl Artistic License, or the GPL version 2.0.

=head1 AUTHOR

Matt Sergeant, <matt@sergeant.org>

Copyright 2005 MessageLabs Ltd.

=head1 SEE ALSO

L<IO::Poll>

=cut
