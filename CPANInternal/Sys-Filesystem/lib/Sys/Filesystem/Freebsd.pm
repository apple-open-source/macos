############################################################
#
#   $Id: Freebsd.pm 185 2010-07-15 19:25:30Z trevor $
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

package Sys::Filesystem::Freebsd;

# vim:ts=4:sw=4:tw=78

use strict;
use warnings;
use vars qw(@ISA $VERSION);

require Sys::Filesystem::Unix;
use Carp qw(croak);

$VERSION = '1.30';
@ISA     = qw(Sys::Filesystem::Unix);

sub version()
{
    return $VERSION;
}

my @keys = qw(fs_spec fs_file fs_vfstype fs_mntops fs_freq fs_passno);
my %special_fs = (
                   swap   => 1,
                   proc   => 1,
                   devpts => 1,
                   tmpfs  => 1,
                 );

my $mount_rx = qr|^([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)|;
my $swap_rx  = qr|^(/[/\w]+)\s+|;

sub new
{
    ref( my $class = shift ) && croak 'Class name required';
    my %args = @_;
    my $self = bless( {}, $class );

    $args{fstab} ||= $ENV{PATH_FSTAB} || '/etc/fstab';

    my @mounts = map { $_ =~ s/[\cI]+/ /g; chomp; $_ } qx( /sbin/mount -p );
    $self->readMounts( $mount_rx, [ 0, 1, 2 ], \@keys, \%special_fs, @mounts );
    $self->readSwap( $swap_rx, qx( /sbin/swapctl -l ) );
    unless ( $self->readFsTab( $args{fstab}, \@keys, [ 0, 1, 2 ], \%special_fs ) )
    {
        croak "Unable to open fstab file ($args{fstab})\n";
    }

    $self;
}

1;

# See the fstab(5) manual page for important information on automatic mounts
# of network filesystems before modifying this file.
#
# Device                Mountpoint      FStype  Options         Dump    Pass#
#/dev/da0s1b             none            swap    sw              0       0
#/dev/da1s1b             none            swap    sw              0       0
#/dev/da0s1a             /               ufs     rw              1       1
#/dev/da1s1e             /home           ufs     rw              2       2
#/dev/da0s1e             /usr            ufs     rw              2       2
#/dev/da1s1f             /var            ufs     rw              2       2
#/dev/acd0c              /cdrom          cd9660  ro,noauto       0       0
#/var/tmp                /tmp            null    rw              0       0
#proc                    /proc           procfs  rw              0       0
#/etc/portal.conf        /p              portal  rw              0       0

###############################################################################
# POD

=pod

=head1 NAME

Sys::Filesystem::Freebsd - Return Freebsd filesystem information to Sys::Filesystem

=head1 SYNOPSIS

See L<Sys::Filesystem>.

=head1 INHERITANCE

  Sys::Filesystem::Freebsd
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

=head1 SEE ALSO

L<Sys::Filesystem>, L<Sys::Filesystem::Unix>, L<fstab(5)>

=head1 VERSION

$Id: Freebsd.pm 185 2010-07-15 19:25:30Z trevor $

=head1 AUTHOR

Nicola Worthington <nicolaw@cpan.org> - L<http://perlgirl.org.uk>

Jens Rehsack <rehsack@cpan.org> - L<http://www.rehsack.de>

=head1 COPYRIGHT

Copyright 2004,2005,2006 Nicola Worthington.

Copyright 2009,2010 Jens Rehsack.

This software is licensed under The Apache Software License, Version 2.0.

L<http://www.apache.org/licenses/LICENSE-2.0>

=cut

