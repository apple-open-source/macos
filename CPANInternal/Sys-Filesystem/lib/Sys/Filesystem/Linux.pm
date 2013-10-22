############################################################
#
#   $Id: Linux.pm 185 2010-07-15 19:25:30Z trevor $
#   Sys::Filesystem - Retrieve list of filesystems and their properties
#
#   Copyright 2004,2005,2006 Nicola Worthington
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

package Sys::Filesystem::Linux;

# vim:ts=4:sw=4:tw=78

use strict;
use warnings;
use vars qw($VERSION @ISA);

use Carp qw(croak);
require IO::File;
require Sys::Filesystem::Unix;

$VERSION = '1.30';
@ISA     = qw(Sys::Filesystem::Unix);

sub version()
{
    return $VERSION;
}

# Default fstab and mtab layout
my @keys = qw(fs_spec fs_file fs_vfstype fs_mntops fs_freq fs_passno);
my %special_fs = (
                   swap                    => 1,
                   proc                    => 1,
                   devpts                  => 1,
                   tmpfs                   => 1,
                   sysfs                   => 1,
                   securityfs              => 1,
                   debugfs                 => 1,
                   procbususb              => 1,
                   udev                    => 1,
                   nfsd                    => 1,
                   binfmt_misc             => 1,
                   fusectl                 => 1,
                   'fuse.gvfs-fuse-daemon' => 1,
                 );

sub new
{
    ref( my $class = shift ) && croak 'Class name required';
    my %args = @_;
    my $self = bless( {}, $class );

    # Defaults
    $args{fstab} ||= '/etc/fstab';
    $args{mtab} ||= -r '/proc/self/mounts' ? '/proc/self/mounts' : '/etc/mtab';
    #$args{xtab}  ||= '/etc/lib/nfs/xtab';

    local $/ = "\n";

    # Read the fstab
    if ( my $fstab = IO::File->new( $args{fstab}, 'r' ) )
    {
        while (<$fstab>)
        {
            next if ( /^\s*#/ || /^\s*$/ );
            my @vals = split( ' ', $_ );
            if ( $vals[0] =~ /^\s*LABEL=(.+)\s*$/ )
            {
                $self->{ $vals[1] }->{label} = $1;
            }
            $self->{ $vals[1] }->{mount_point} = $vals[1];
            $self->{ $vals[1] }->{device}      = $vals[0];
            $self->{ $vals[1] }->{unmounted}   = 1;
            $self->{ $vals[1] }->{special}     = 1 if ( defined( $special_fs{ $vals[2] } ) );
            for ( my $i = 0; $i < @keys; ++$i )
            {
                $self->{ $vals[1] }->{ $keys[$i] } = $vals[$i];
            }
        }
        $fstab->close();
    }
    else
    {
        croak "Unable to open fstab file ($args{fstab})\n";
    }

    # Read the mtab
    unless ( $self->readMntTab( $args{mtab}, \@keys, [ 0, 1, 2 ], \%special_fs ) )
    {
        croak "Unable to open fstab file ($args{mtab})\n";
    }

    $self;
}

1;

=pod

=head1 NAME

Sys::Filesystem::Linux - Return Linux filesystem information to Sys::Filesystem

=head1 SYNOPSIS

See L<Sys::Filesystem>.

=head1 INHERITANCE

  Sys::Filesystem::Linux
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

Dscribes the block special device or remote filesystem to be mounted.

For  ordinary  mounts  it  will hold (a link to) a block special device
node (as created by L<mknod(8)>)  for  the  device  to  be  mounted,  like
/dev/cdrom’   or   ‘/dev/sdb7’.    For   NFS   mounts  one  will  have
<host>:<dir>, e.g., ‘knuth.aeb.nl:/’.  For procfs, use ‘proc’.

Instead of giving the device explicitly, one may indicate the (ext2  or
xfs)  filesystem that is to be mounted by its UUID or volume label (cf.
L<e2label(8)> or  L<xfs_admin(8)>),  writing  LABEL=<label>  or  UUID=<uuid>,
e.g.,   ‘LABEL=Boot’   or  ‘UUID=3e6be9de-8139-11d1-9106-a43f08d823a6’.
This will make the system more robust: adding or removing a  SCSI  disk
changes the disk device name but not the filesystem volume label.

=item fs_file

Describes the mount point for the filesystem. For swap partitions,
this field should be specified as‘none. If the name of the mount
point contains spaces these can be escaped as‘\040.

=item fs_vfstype

Dscribes the type  of  the  filesystem.
Linux  supports  lots  of filesystem types, such as adfs, affs, autofs,
coda, coherent, cramfs, devpts, efs, ext2, ext3,  hfs,  hpfs,  iso9660,
jfs,  minix,  msdos,  ncpfs,  nfs,  ntfs,  proc, qnx4, reiserfs, romfs,
smbfs, sysv, tmpfs, udf, ufs, umsdos, vfat, xenix,  xfs,  and  possibly
others.  For more details, see L<mount(8)>.  For the filesystems currently
supported by the running kernel, see /proc/filesystems.  An entry  swap
denotes a file or partition to be used for swapping, cf. L<swapon(8)>.  An
entry ignore causes the line to be ignored.  This  is  useful  to  show
disk partitions which are currently unused.

=item fs_mntops

Describes the mount options associated with the filesystem.

It is formatted as a comma separated list of options.  It  contains  at
least  the type of mount plus any additional options appropriate to the
filesystem type.  For documentation on the available options  for  non-
nfs  file systems, see L<mount(8)>.  For documentation on all nfs-specific
options have a look at L<nfs(5)>.  Common for all types of file system are
the options ‘‘noauto’’ (do not mount when 'mount -a' is given, e.g., at
boot time), ‘‘user’’ (allow a user  to  mount),  and  ‘‘owner’’  (allow
device  owner to mount), and ‘‘_netdev’’ (device requires network to be
available).  The ‘‘owner’’ and ‘‘_netdev’’ options are  Linux-specific.
For more details, see L<mount(8)>.

=item fs_freq

Used  for  these filesystems by the
L<dump(8)> command to determine which filesystems need to be  dumped.   If
the  fifth  field  is not present, a value of zero is returned and dump
will assume that the filesystem does not need to be dumped.

=item fs_passno

Used by the L<fsck(8)> program to  determine the order in which filesystem
checks are done at reboot time.  The
root filesystem should be specified with a fs_passno of  1,  and  other
filesystems  should  have a fs_passno of 2.  Filesystems within a drive
will be checked sequentially, but filesystems on different drives  will
be  checked  at  the  same time to utilize parallelism available in the
hardware.  If the sixth field is not present or zero, a value  of  zero
is  returned  and fsck will assume that the filesystem does not need to
be checked.

=back

=head1 SEE ALSO

L<Sys::Filesystem>, L<Sys::Filesystem::Unix>, L<fstab(5)>

=head1 VERSION

$Id: Linux.pm 185 2010-07-15 19:25:30Z trevor $

=head1 AUTHOR

Nicola Worthington <nicolaw@cpan.org> - L<http://perlgirl.org.uk>

Jens Rehsack <rehsack@cpan.org> - L<http://www.rehsack.de/>

=head1 COPYRIGHT

Copyright 2004,2005,2006 Nicola Worthington.

Copyright 2009,2010 Jens Rehsack.

This software is licensed under The Apache Software License, Version 2.0.

L<http://www.apache.org/licenses/LICENSE-2.0>

=cut

