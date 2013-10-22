############################################################
#
#   $Id: Solaris.pm 185 2010-07-15 19:25:30Z trevor $
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

package Sys::Filesystem::Solaris;

# vim:ts=4:sw=4:tw=78

use strict;
use warnings;
use vars qw($VERSION @ISA);

use Carp qw(croak);
use Data::Dumper;
require Sys::Filesystem::Unix;

$VERSION = '1.30';
@ISA     = qw(Sys::Filesystem::Unix);

sub version()
{
    return $VERSION;
}

my @fstab_keys = qw(device device_to_fsck mount_point fs_vfstype fs_freq mount_at_boot fs_mntops);
my @mtab_keys  = qw(device mount_point fs_vfstype fs_mntops time);
my %special_fs = (
                   swap    => 1,
                   proc    => 1,
                   procfs  => 1,
                   tmpfs   => 1,
                   mntfs   => 1,
                   autofs  => 1,
                   lofs    => 1,
                   fd      => 1,
                   ctfs    => 1,
                   devfs   => 1,
                   dev     => 1,
                   objfs   => 1,
                   cachefs => 1,
                 );

sub new
{
    ref( my $class = shift ) && croak 'Class name required';
    my %args = @_;
    my $self = bless( {}, $class );

    $args{fstab} ||= '/etc/vfstab';
    $args{mtab}  ||= '/etc/mnttab';

    unless ( $self->readFsTab( $args{fstab}, \@fstab_keys, [ 0, 2, 3 ], \%special_fs ) )
    {
        croak "Unable to open fstab file ($args{fstab})\n";
    }
    print( STDERR Dumper( \$self ) );

    unless ( $self->readMntTab( $args{mtab}, \@mtab_keys, [ 0, 1, 2 ], \%special_fs ) )
    {
        croak "Unable to open mtab file ($args{mtab})\n";
    }
    print( STDERR Dumper( \$self ) );

    $self;
}

1;

=pod

=head1 NAME

Sys::Filesystem::Solaris - Return Solaris filesystem information to Sys::Filesystem

=head1 SYNOPSIS

See L<Sys::Filesystem>.

=head1 INHERITANCE

  Sys::Filesystem::Solaris
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

=item device

Resource name.

=item device_to_fsck

The raw device to fsck.

=item mount_point

The default mount directory.

=item fs_vfstype

The  name of the file system type.

=item fs_freq

The number used by fsck to decide whether to check the file system
automatically.

=item mount_at_boot

Whether the file system should be mounted automatically by mountall.

=item fs_mntops

The file system mount options.

=item time

The time at which the file system was mounted.

=back

=head1 SEE ALSO

L<Solaris::DeviceTree>

=head1 VERSION

$Id: Solaris.pm 185 2010-07-15 19:25:30Z trevor $

=head1 AUTHOR

Nicola Worthington <nicolaw@cpan.org> - L<http://perlgirl.org.uk>

Jens Rehsack <rehsack@cpan.org> - L<http://www.rehsack.de/>

=head1 COPYRIGHT

Copyright 2004,2005,2006 Nicola Worthington.
Copyright 2009,2010 Jens Rehsack.

This software is licensed under The Apache Software License, Version 2.0.

L<http://www.apache.org/licenses/LICENSE-2.0>

=cut

