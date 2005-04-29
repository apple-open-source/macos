package Mail::Audit::KillDups;
use Mail::Audit;
use vars qw(@VERSION $dupfile $cache_bytes);
$VERSION = '1.9';
$dupfile = ".msgid-cache";
$cache_bytes = 10000;
1;

package Mail::Audit;
use strict;
use Fcntl;

sub killdups {
    my $self = shift;
    my $mid = shift || $self->get("Message-Id");
    my $end_of_ring = 0;
    my $current_pos;

    unless (sysopen MSGID, $Mail::Audit::KillDups::dupfile, O_RDWR | O_CREAT) {
        _log(1, "Error opening $Mail::Audit::KillDups::dupfile: $!");
        return 1;
    }

    chomp $mid;
    while (<MSGID>) {
        chomp;
        if ($_ eq $mid) {
            _log(1, "Duplicate, ignoring");
            $self->ignore;
            return 1; # Just in case.
        }

        $current_pos = tell MSGID;
        if ($current_pos > $Mail::Audit::KillDups::cache_bytes && $end_of_ring == 0) {
            # we've gotten too big, write this mid back at the top of the file

            last;
        } elsif ($_ eq "" && $end_of_ring == 0 && $current_pos > 0) {
            # Found the end of the ring buffer, so save position.
            $end_of_ring = $current_pos - 1;
        }
    }

    # Didn't find mid, so write it to the end of the ring buffer
    unless (seek MSGID, $end_of_ring, 0) {
        _log(1, "seek to position $end_of_ring failed: $!");
        close MSGID;
        return 1;
    }

    print MSGID "$mid\n\n";
    close MSGID;
        
    return 0;
}


1;
__END__

=pod

=head1 NAME

Mail::Audit::KillDups - Mail::Audit plugin for duplicate suppression

=head1 SYNOPSIS

    use Mail::Audit qw(KillDups);
    $Mail::Audit::KillDups::dupfile = "/home/simon/.msgid-cache";
        my $mail = Mail::Audit->new;
    $mail->killdups;

=head1 DESCRIPTION

This is a Mail::Audit plugin which provides a method for checking
and supressing duplicate messages; that is, mails with message-ids which
have been previously seen.

=head2 METHODS

=over 4

=item C<killdups>

Checks the incoming message against a file of previously seen message
ids, ignores it if it's already seen, and adds it if it hasn't been.
C<$Mail::Audit::KillDups::dupfile> contains the name of the file used;
if you don't set this, it will be F<.msgid-cache> in the current
directory. (Probably your home directory.)

The data in C<$Mail::Audit::KillDups::dupfile> will be treated as a
ring buffer, where the end of the buffer will be delimited by two
newline characters.  When the file size exceeds
C<$Mail::Audit::KillDups::cache_bytes> bytes, the message id will be
written at the beginning of the file.  Old message ids in the file
will be overwritten.

=head1 AUTHOR

Simon Cozens <simon@cpan.org>

=head1 SEE ALSO

L<Mail::Audit>
