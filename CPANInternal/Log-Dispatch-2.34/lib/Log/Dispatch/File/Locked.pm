package Log::Dispatch::File::Locked;
{
  $Log::Dispatch::File::Locked::VERSION = '2.34';
}

use strict;
use warnings;

use base qw( Log::Dispatch::File );

use Fcntl qw(:DEFAULT :flock);

sub _open_file {
    my $self = shift;

    $self->SUPER::_open_file();

    my $fh = $self->{fh};

    flock( $fh, LOCK_EX )
        or die "Cannot lock '$self->{filename}' for writing: $!";

    # just in case there was an append while we waited for the lock
    seek( $fh, 0, 2 )
        or die "Cannot seek to end of '$self->{filename}': $!";
}

1;

# ABSTRACT: Subclass of Log::Dispatch::File to facilitate locking

__END__

=pod

=head1 NAME

Log::Dispatch::File::Locked - Subclass of Log::Dispatch::File to facilitate locking

=head1 VERSION

version 2.34

=head1 SYNOPSIS

  use Log::Dispatch;

  my $log = Log::Dispatch->new(
      outputs => [
          [
              'File::Locked',
              min_level => 'info',
              filename  => 'Somefile.log',
              mode      => '>>',
              newline   => 1
          ]
      ],
  );

  $log->emerg("I've fallen and I can't get up");

=head1 DESCRIPTION

This module acts exactly like L<Log::Dispatch::File> except that it
obtains an exclusive lock on the file before writing to it.

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>

=head1 COPYRIGHT AND LICENSE

This software is Copyright (c) 2011 by Dave Rolsky.

This is free software, licensed under:

  The Artistic License 2.0 (GPL Compatible)

=cut
