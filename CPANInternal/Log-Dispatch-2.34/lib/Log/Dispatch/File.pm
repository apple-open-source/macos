package Log::Dispatch::File;
{
  $Log::Dispatch::File::VERSION = '2.34';
}

use strict;
use warnings;

use Log::Dispatch::Output;

use base qw( Log::Dispatch::Output );

use Params::Validate qw(validate SCALAR BOOLEAN);
Params::Validate::validation_options( allow_extra => 1 );

use Scalar::Util qw( openhandle );

# Prevents death later on if IO::File can't export this constant.
*O_APPEND = \&APPEND unless defined &O_APPEND;

sub APPEND { 0 }

sub new {
    my $proto = shift;
    my $class = ref $proto || $proto;

    my %p = @_;

    my $self = bless {}, $class;

    $self->_basic_init(%p);
    $self->_make_handle(%p);

    return $self;
}

sub _make_handle {
    my $self = shift;

    my %p = validate(
        @_,
        {
            filename => { type => SCALAR },
            mode     => {
                type    => SCALAR,
                default => '>'
            },
            binmode => {
                type    => SCALAR,
                default => undef
            },
            autoflush => {
                type    => BOOLEAN,
                default => 1
            },
            close_after_write => {
                type    => BOOLEAN,
                default => 0
            },
            permissions => {
                type     => SCALAR,
                optional => 1
            },
        }
    );

    $self->{filename}    = $p{filename};
    $self->{close}       = $p{close_after_write};
    $self->{permissions} = $p{permissions};
    $self->{binmode}     = $p{binmode};
    $self->{syswrite}    = $p{syswrite};

    if ( $self->{close} ) {
        $self->{mode} = '>>';
    }
    elsif (
           exists $p{mode}
        && defined $p{mode}
        && (
            $p{mode} =~ /^(?:>>|append)$/
            || (   $p{mode} =~ /^\d+$/
                && $p{mode} == O_APPEND() )
        )
        ) {
        $self->{mode} = '>>';
    }
    else {
        $self->{mode} = '>';
    }

    $self->{autoflush} = $p{autoflush};

    $self->_open_file() unless $p{close_after_write};

}

sub _open_file {
    my $self = shift;

    open my $fh, $self->{mode}, $self->{filename}
        or die "Cannot write to '$self->{filename}': $!";

    if ( $self->{autoflush} ) {
        my $oldfh = select $fh;
        $| = 1;
        select $oldfh;
    }

    if ( $self->{permissions}
        && !$self->{chmodded} ) {
        my $current_mode = ( stat $self->{filename} )[2] & 07777;
        if ( $current_mode ne $self->{permissions} ) {
            chmod $self->{permissions}, $self->{filename}
                or die
                "Cannot chmod $self->{filename} to $self->{permissions}: $!";
        }

        $self->{chmodded} = 1;
    }

    if ( $self->{binmode} ) {
        binmode $fh, $self->{binmode};
    }

    $self->{fh} = $fh;
}

sub log_message {
    my $self = shift;
    my %p    = @_;

    if ( $self->{close} ) {
        $self->_open_file;
    }

    my $fh = $self->{fh};

    if ( $self->{syswrite} ) {
        defined syswrite( $fh, $p{message} )
            or die "Cannot write to '$self->{filename}': $!";
    }
    else {
        print $fh $p{message}
            or die "Cannot write to '$self->{filename}': $!";
    }

    if ( $self->{close} ) {
        close $fh
            or die "Cannot close '$self->{filename}': $!";
    }
}

sub DESTROY {
    my $self = shift;

    if ( $self->{fh} ) {
        my $fh = $self->{fh};
        close $fh if openhandle($fh);
    }
}

1;

# ABSTRACT: Object for logging to files

__END__

=pod

=head1 NAME

Log::Dispatch::File - Object for logging to files

=head1 VERSION

version 2.34

=head1 SYNOPSIS

  use Log::Dispatch;

  my $log = Log::Dispatch->new(
      outputs => [
          [
              'File',
              min_level => 'info',
              filename  => 'Somefile.log',
              mode      => '>>',
              newline   => 1
          ]
      ],
  );

  $log->emerg("I've fallen and I can't get up");

=head1 DESCRIPTION

This module provides a simple object for logging to files under the
Log::Dispatch::* system.

Note that a newline will I<not> be added automatically at the end of a message
by default.  To do that, pass C<< newline => 1 >>.

=head1 CONSTRUCTOR

The constructor takes the following parameters in addition to the standard
parameters documented in L<Log::Dispatch::Output>:

=over 4

=item * filename ($)

The filename to be opened for writing.

=item * mode ($)

The mode the file should be opened with.  Valid options are 'write',
'>', 'append', '>>', or the relevant constants from Fcntl.  The
default is 'write'.

=item * binmode ($)

A layer name to be passed to binmode, like ":encoding(UTF-8)" or ":raw".

=item * close_after_write ($)

Whether or not the file should be closed after each write.  This
defaults to false.

If this is true, then the mode will always be append, so that the file is not
re-written for each new message.

=item * autoflush ($)

Whether or not the file should be autoflushed.  This defaults to true.

=item * syswrite ($)

Whether or not to perform the write using L<perlfunc/syswrite>(),
as opposed to L<perlfunc/print>().  This defaults to false.
The usual caveats and warnings as documented in L<perlfunc/syswrite> apply.

=item * permissions ($)

If the file does not already exist, the permissions that it should
be created with.  Optional.  The argument passed must be a valid
octal value, such as 0600 or the constants available from Fcntl, like
S_IRUSR|S_IWUSR.

See L<perlfunc/chmod> for more on potential traps when passing octal
values around.  Most importantly, remember that if you pass a string
that looks like an octal value, like this:

 my $mode = '0644';

Then the resulting file will end up with permissions like this:

 --w----r-T

which is probably not what you want.

=back

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>

=head1 COPYRIGHT AND LICENSE

This software is Copyright (c) 2011 by Dave Rolsky.

This is free software, licensed under:

  The Artistic License 2.0 (GPL Compatible)

=cut
