############################################################
#
#   $Id: Netbsd.pm 185 2010-07-15 19:25:30Z trevor $
#   Sys::Filesystem - Retrieve list of filesystems and their properties
#
#   Copyright 2009 Jens Rehsack
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

package Sys::Filesystem::Netbsd;

# vim:ts=4:sw=4:tw=78

use strict;
use vars qw(@ISA $VERSION);

require Sys::Filesystem::Unix;
use Carp qw(croak);

$VERSION = '1.30';
@ISA     = qw(Sys::Filesystem::Unix);

sub version()
{
    return $VERSION;
}

# Default fstab and mtab layout
my @keys = qw(fs_spec fs_file fs_vfstype fs_mntops fs_freq fs_passno);
my %special_fs = (
                   swap   => 1,
                   procfs => 1,
                   kernfs => 1,
                   ptyfs  => 1,
                   tmpfs  => 1,
                 );

my $mount_rx = qr|^([/:\w]+)\s+on\s+([/\w]+)\s+type\s+(\w+)|;
my $swap_rx  = qr|^(/[/\w]+)\s+|;

sub new
{
    ref( my $class = shift ) && croak 'Class name required';
    my %args = @_;
    my $self = bless( {}, $class );

    # Defaults
    $args{fstab} ||= '/etc/fstab';

    my @mounts = qx( /sbin/mount );
    $self->readMounts( $mount_rx, [ 0, 1, 2 ], [qw(fs_spec fs_file fs_vfstype fs_mntops)], \%special_fs, @mounts );
    $self->readSwap( $swap_rx, qx( /sbin/swapctl -l ) );
    unless ( $self->readFsTab( $args{fstab}, \@keys, [ 0, 1, 2 ], \%special_fs ) )
    {
        croak "Unable to open fstab file ($args{fstab})\n";
    }

    return $self;
}

1;

=pod

=head1 NAME

Sys::Filesystem::Netbsd - Return NetBSD filesystem information to Sys::Filesystem

=head1 SYNOPSIS

See L<Sys::Filesystem>.

=head1 INHERITANCE

  Sys::Filesystem::Netbsd
  ISA Sys::Filesystem::Unix
    ISA UNIVERSAL

=head1 METHODS

=over 4

=item version ()

Return the version of the (sub)module.

=back

=head1 ATTRIBUTES

The following is a list of filesystem properties which may
be queried as methods through the parent L<Sys::Filesystem> object.

=over 4

=item fs_spec

Describes the block special device or remote filesystem to be mounted.

=item fs_file

Describes the mount point for the filesystem. For swap partitions,
this field should be specified as none. If the name of the mount
point contains spaces these can be escaped as \040.

=item fs_vfstype

Dscribes the type  of  the  filesystem.

=item fs_mntops

Describes the mount options associated with the filesystem.

=item fs_freq

Used  for  these filesystems by the
L<dump(8)> command to determine which filesystems need to be  dumped.

=item fs_passno

Used by the L<fsck(8)> program to  determine the order in which filesystem
checks are done at reboot time. 

=back

=head1 VERSION

$Id: Netbsd.pm 185 2010-07-15 19:25:30Z trevor $

=head1 AUTHOR

Jens Rehsack <rehsack@cpan.org> - L<http://www.rehsack.de/>

=head1 COPYRIGHT

Copyright 2009,2010 Jens Rehsack.

This software is licensed under The Apache Software License, Version 2.0.

L<http://www.apache.org/licenses/LICENSE-2.0>

=cut

