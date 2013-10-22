############################################################
#
#   $Id: Unix.pm 185 2010-07-15 19:25:30Z trevor $
#   Sys::Filesystem - Retrieve list of filesystems and their properties
#
#   Copyright 2004,2005,2006 Nicola Worthington
#   Copyright 2009           Jens Rehsack
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
############################################################

package Sys::Filesystem::Unix;

# vim:ts=4:sw=4:tw=78

use strict;
use Carp qw(croak);
use Fcntl qw(:flock);
use IO::File;

use vars qw($VERSION);
$VERSION = '1.30';

sub version()
{
    return $VERSION;
}

# Default fstab and mtab layout
my @keys = qw(fs_spec fs_file fs_vfstype fs_mntops fs_freq fs_passno);
my %special_fs = (
                   swap => 1,
                   proc => 1
                 );

sub new
{
    ref( my $class = shift ) && croak 'Class name required';
    my %args = @_;
    my $self = bless( {}, $class );

    # Defaults
    $args{fstab} ||= '/etc/fstab';
    $args{mtab}  ||= '/etc/mtab';

    # $args{xtab}  ||= '/etc/lib/nfs/xtab';

    $self->readFsTab( $args{fstab}, \@keys, [ 0, 1, 2 ], \%special_fs );
    $self->readMntTab( $args{mtab}, \@keys, [ 0, 1, 2 ], \%special_fs );

    $self;
}

sub readFsTab($\@\@\%)
{
    my ( $self, $fstabPath, $fstabKeys, $pridx, $special_fs ) = @_;

    # Read the fstab
    local $/ = "\n";
    if ( my $fstab = IO::File->new( $fstabPath, 'r' ) )
    {
        while (<$fstab>)
        {
            next if ( /^\s*#/ || /^\s*$/ );

            # $_ =~ s/#.*$//;
            # next if( /^\s*$/ );

            my @vals = split( ' ', $_ );
            $self->{ $vals[ $pridx->[1] ] }->{mount_point} = $vals[ $pridx->[1] ];
            $self->{ $vals[ $pridx->[1] ] }->{device}      = $vals[ $pridx->[0] ];
            $self->{ $vals[ $pridx->[1] ] }->{unmounted}   = 1
              unless ( defined( $self->{ $vals[ $pridx->[1] ] }->{mounted} ) );

            if ( defined( $pridx->[2] ) )
            {
                my $vfs_type = $self->{ $vals[ $pridx->[1] ] }->{fs_vfstype} = $vals[ $pridx->[2] ];
                $self->{ $vals[ $pridx->[1] ] }->{special} = 1 if ( defined( $special_fs->{$vfs_type} ) );
            }
            else
            {
                $self->{ $vals[ $pridx->[1] ] }->{special} = 0
                  unless ( defined( $self->{ $vals[ $pridx->[1] ] }->{special} ) );
            }

            for ( my $i = 0; $i < @{$fstabKeys}; ++$i )
            {
                $self->{ $vals[ $pridx->[1] ] }->{ $fstabKeys->[$i] } = defined( $vals[$i] ) ? $vals[$i] : '';
            }
        }
        $fstab->close();
        1;
    }
    else
    {
        0;
    }
}

sub readMntTab($\@\@\%)
{
    my ( $self, $mnttabPath, $mnttabKeys, $pridx, $special_fs ) = @_;

    # Read the mtab
    local $/ = "\n";
    my $mtab;
    if ( ( $mtab = IO::File->new( $mnttabPath, 'r' ) ) && flock( $mtab, LOCK_SH | LOCK_NB ) )
    {
        while (<$mtab>)
        {
            next if ( /^\s*#/ || /^\s*$/ );

            # $_ =~ s/#.*$//;
            # next if( /^\s*$/ );

            my @vals = split( /\s+/, $_ );
            delete $self->{ $vals[ $pridx->[1] ] }->{unmounted}
              if ( exists( $self->{ $vals[ $pridx->[1] ] }->{unmounted} ) );
            $self->{ $vals[ $pridx->[1] ] }->{mounted}     = 1;
            $self->{ $vals[ $pridx->[1] ] }->{mount_point} = $vals[ $pridx->[1] ];
            $self->{ $vals[ $pridx->[1] ] }->{device}      = $vals[ $pridx->[0] ];

            if ( defined( $pridx->[2] ) )
            {
                my $vfs_type = $self->{ $vals[ $pridx->[1] ] }->{fs_vfstype} = $vals[ $pridx->[2] ];
                $self->{ $vals[ $pridx->[1] ] }->{special} = 1 if ( defined( $special_fs->{$vfs_type} ) );
            }
            else
            {
                $self->{ $vals[ $pridx->[1] ] }->{special} = 0
                  unless ( defined( $self->{ $vals[ $pridx->[1] ] }->{special} ) );
            }

            for ( my $i = 0; $i < @{$mnttabKeys}; ++$i )
            {
                $self->{ $vals[ $pridx->[1] ] }->{ $mnttabKeys->[$i] } = defined( $vals[$i] ) ? $vals[$i] : '';
            }
        }
        $mtab->close();
        1;
    }
    else
    {
        0;
    }
}

sub readMounts
{
    my ( $self, $mount_rx, $pridx, $keys, $special, @lines ) = @_;

    foreach my $line (@lines)
    {
        if ( my @vals = $line =~ $mount_rx )
        {
            $self->{ $vals[ $pridx->[1] ] }->{mount_point} = $vals[ $pridx->[1] ];
            $self->{ $vals[ $pridx->[1] ] }->{device}      = $vals[ $pridx->[0] ];
            $self->{ $vals[ $pridx->[1] ] }->{mounted}     = 1;
            delete $self->{ $vals[ $pridx->[1] ] }->{unmounted}
              if ( exists( $self->{ $vals[ $pridx->[1] ] }->{unmounted} ) );

            if ( defined( $pridx->[2] ) )
            {
                my $vfs_type = $self->{ $vals[ $pridx->[1] ] }->{fs_vfstype} = $vals[ $pridx->[2] ];
                $self->{ $vals[ $pridx->[1] ] }->{special} = 1 if ( defined( $special->{$vfs_type} ) );
            }
            elsif ( !defined( $self->{ $vals[ $pridx->[1] ] }->{special} ) )
            {
                $self->{ $vals[ $pridx->[1] ] }->{special} = 0;
            }

            for ( my $i = 0; $i < @{$keys}; ++$i )
            {
                $self->{ $vals[ $pridx->[1] ] }->{ $keys->[$i] } = defined( $vals[$i] ) ? $vals[$i] : '';
            }
        }
    }

    $self;
}

sub readSwap
{
    my ( $self, $swap_rx, @lines ) = @_;
    foreach my $line (@lines)
    {
        if ( my ($dev) = $line =~ $swap_rx )
        {
            $self->{none}->{mount_point} ||= 'none';
            $self->{none}->{device}     = $dev;
            $self->{none}->{fs_vfstype} = 'swap';
            $self->{none}->{mounted}    = 1;
            $self->{none}->{special}    = 1;
            delete $self->{none}->{unmounted};
        }
    }
    $self;
}

1;

=pod

=head1 NAME

Sys::Filesystem::Unix - Return generic Unix filesystem information to Sys::Filesystem

=head1 SYNOPSIS

See L<Sys::Filesystem>.

=head1 INHERITANCE

  Sys::Filesystem::Unix
  ISA UNIVERSAL

=head1 METHODS

=over 4

=item version()

Return the version of the (sub)module.

=item readFsTab

This method provides the capability to parse a standard unix fstab file.

It expects following arguments:

=over 8

=item fstabPath

Full qualified path to the fstab file to read.

=item fstabKeys

The column names for the fstab file through an array reference.

=item special_fs

Hash reference containing the names of all special file systems having a true
value as key.

=back

This method return true in case the specified file could be opened for reading,
false otherwise.

=item readMntTab

This method provides the capability to read abd parse a standard unix
mount-tab file. The file is locked using flock after opening it.

It expects following arguments:

=over 8

=item mnttabPath

Full qualified path to the mnttab file to read.

=item mnttabKeys

The column names for the mnttab file through an array reference.

=item $special_fs

Hash reference containing the names of all special file systems having a true
value as key.

=back

This method return true in case the specified file could be opened for reading
and locked, false otherwise.

=item readMounts

This method is called to parse the information got from C<mount> system command.
It expects following arguments:

=over 8

=item mount_rx

Regular expression to extract the information from each mount line.

=item pridx

Array reference containing the index for primary keys of interest in match
in following order: device, mount_point, type.

=item keys

Array reference of the columns of the match - in order of paranteses in
regular expression.

=item special

Array reference containing the names of the special file system types.

=item lines

Array containing the lines to parse.

=back

=item readSwap

This method is called to parse the information from the swap status.
It expects following arguments:

=over 8

=item swap_rx

Regular expression to extract the information from each swap status line.
This regular expression should have exact one pair of parantheses to
identify the swap device.

=item lines

Array containing the lines to parse.

=back

=back

=head1 VERSION

$Id: Unix.pm 185 2010-07-15 19:25:30Z trevor $

=head1 AUTHOR

Nicola Worthington <nicolaw@cpan.org> - L<http://perlgirl.org.uk>

Jens Rehsack <rehsack@cpan.org> - L<http://www.rehsack.de/>

=head1 COPYRIGHT

Copyright 2004,2005,2006 Nicola Worthington.
Copyright 2008-2010 Jens Rehsack.

This software is licensed under The Apache Software License, Version 2.0.

L<http://www.apache.org/licenses/LICENSE-2.0>

=cut

