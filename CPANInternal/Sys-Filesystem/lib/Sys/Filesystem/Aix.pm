############################################################
#
#   $Id: Aix.pm 185 2010-07-15 19:25:30Z trevor $
#   Sys::Filesystem - Retrieve list of filesystems and their properties
#
#   Copyright 2004,2005,2006 Nicola Worthington
#   Copyright 2008,2009      Jens Rehsack
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

package Sys::Filesystem::Aix;

# vim:ts=4:sw=4:tw=78

use strict;
use warnings;
use vars qw($VERSION);

use Carp qw(croak);
use IO::File;

$VERSION = '1.30';

sub version()
{
    return $VERSION;
}

my @fstab_keys = qw(account boot check dev mount nodename size type vfs vol log);
my %special_fs = (
                   swap   => 1,
                   procfs => 1,
                   proc   => 1,
                   tmpfs  => 1,
                   mntfs  => 1,
                   autofs => 1,
                 );

sub new
{
    ref( my $class = shift ) && croak 'Class name required';
    my %args = @_;
    my $self = bless( {}, $class );

    $args{fstab} ||= '/etc/filesystems';
    local $/ = "\n";

    my %curr_mountz = map {
        my $path = $_ =~ m/^\s/ ? (split)[1] : (split)[2];
        ( $path => 1 );
    } qx( /usr/sbin/mount );

    my %fs_info = map {
        my ( $path, $device, $vfs, $nodename, $type, $size, $options, $mount, $account ) = split( m/:/, $_ );

        ( $path => [ $device, $vfs, $nodename, $type, $size, $options, $mount, $account ] )
      }
      grep { m/^[^#]/ } qx( /usr/sbin/lsfs -c );

    foreach my $current_filesystem ( keys %fs_info )
    {
        $self->{$current_filesystem}->{filesystem} = $current_filesystem;

        my ( $device, $vfs, $nodename, $type, $size, $options, $mount, $account ) = @{ $fs_info{$current_filesystem} };

        $self->{$current_filesystem}->{dev}      = $device;
        $self->{$current_filesystem}->{vfs}      = $vfs;
        $self->{$current_filesystem}->{options}  = $options;
        $self->{$current_filesystem}->{nodename} = $nodename;
        $self->{$current_filesystem}->{type}     = $type;
        $self->{$current_filesystem}->{size}     = $size;
        $self->{$current_filesystem}->{mount}    = $mount;
        $self->{$current_filesystem}->{account}  = $account;
        $self->{$current_filesystem}->{special}  = 1 if ( defined($vfs) && defined( $special_fs{$vfs} ) );

        # the filesystem is either currently mounted or is not,
        # this does not need to be checked for each individual
        # attribute.
        my $state = defined( $curr_mountz{$current_filesystem} ) ? 'mounted' : 'unmounted';
        $self->{$current_filesystem}->{$state} = 1;
    }

    %fs_info = map {
        my ( $lvname, $type, $lps, $pps, $pvs, $lvstate, $path ) = split( m/\s+/, $_ );

        ( $path => [ $lvname, $type, $lps, $pps, $pvs, $lvstate ] )
      }
      grep { $_ !~ m/^\w+:$/ }
      grep { $_ !~ m/^LV\sNAME\s+/ }
      grep { $_ !~ m(N/A$) } qx( /usr/sbin/lsvg -Ll `/usr/sbin/lsvg -Lo` );

    foreach my $current_filesystem ( keys %fs_info )
    {
        $self->{$current_filesystem}->{filesystem} = $current_filesystem;

        my ( $lvname, $type, $lps, $pps, $pvs, $lvstate ) = @{ $fs_info{$current_filesystem} };

        $self->{$current_filesystem}->{dev}     = $lvname;
        $self->{$current_filesystem}->{vfs}     = $type;
        $self->{$current_filesystem}->{LPs}     = $lps;
        $self->{$current_filesystem}->{PPs}     = $pps;
        $self->{$current_filesystem}->{PVs}     = $pvs;
        $self->{$current_filesystem}->{lvstate} = $lvstate;
        $self->{$current_filesystem}->{special} = 1 if ( defined($type) && defined( $special_fs{$type} ) );

        # the filesystem is either currently mounted or is not,
        # this does not need to be checked for each individual
        # attribute.
        my $state = defined( $curr_mountz{$current_filesystem} ) ? 'mounted' : 'unmounted';
        $self->{$current_filesystem}->{$state} = 1;
    }

    # Read the fstab
    if ( my $fstab = IO::File->new( $args{fstab}, 'r' ) )
    {
        my $current_filesystem = '*UNDEFINED*';
        while (<$fstab>)
        {

            # skip comments and blank lines.
            next if m{^ [*] }x || m{^ \s* $}x;

            # Found a new filesystem group
            if (/^\s*(.+?):\s*$/)
            {
                $current_filesystem = $1;
                $self->{$current_filesystem}->{filesystem} = $1;

                # the filesystem is either currently mounted or is not,
                # this does not need to be checked for each individual
                # attribute.
                my $state = defined( $curr_mountz{$current_filesystem} ) ? 'mounted' : 'unmounted';
                $self->{$current_filesystem}{$state} = 1;

                # This matches a filesystem attribute
            }
            elsif ( my ( $key, $value ) = $_ =~ /^\s*([a-z]{3,8})\s+=\s+"?(.+)"?\s*$/ )
            {
                unless ( defined( $self->{$current_filesystem}->{$key} ) )
                {

                    # do not overwrite already known data
                    $self->{$current_filesystem}->{$key} = $value;
                    if ( ( $key eq 'vfs' ) && defined( $special_fs{$value} ) )
                    {
                        $self->{$current_filesystem}->{special} = 1;
                    }
                }
            }
        }
        $fstab->close();
    }
    else
    {
        croak "Unable to open fstab file ($args{fstab})\n";
    }

    $self;
}

1;

=pod

=head1 NAME

Sys::Filesystem::Aix - Return AIX filesystem information to Sys::Filesystem

=head1 SYNOPSIS

See L<Sys::Filesystem>.

=head1 INHERITANCE

  Sys::Filesystem::Aix
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

=item account

Used by the dodisk command to determine the filesystems to be
processed by the accounting system.

=item boot

Used by the mkfs command to initialize the boot block of a new
filesystem.

=item check

Used by the fsck command to determine the default filesystems
to be checked.

=item dev

Identifies, for local mounts, either the block special file
where the filesystem resides or the file or directory to be
mounted. 

=item free

This value can be either true or false. (Obsolete and ignored).

=item mount

Used by the mount command to determine whether this file
system should be mounted by default.

=item nodename

Used by the mount command to determine which node contains
the remote filesystem.

=item size

Used by the mkfs command for reference and to build the file
system.

=item type

Used to group related mounts.

=item vfs

Specifies the type of mount. For example, vfs=nfs specifies
the virtual filesystem being mounted is an NFS filesystem.

=item vol

Used by the mkfs command when initializing the label on a new
filesystem. The value is a volume or pack label using a
maximum of 6 characters.

=item log

The LVName must be the full path name of the filesystem logging
logical volume name to which log data is written as this file
system is modified. This is only valid for journaled filesystems.

=back

=head1 SEE ALSO

L<Sys::Filesystem>

=head2 Example /etc/filesystems


	* @(#)filesystems @(#)29	1.22  src/bos/etc/filesystems/filesystems, cmdfs, bos530 9/8/00 13:57:45
	* IBM_PROLOG_BEGIN_TAG 
	* This is an automatically generated prolog. 
	*  
	* <snip>
	*  
	* This version of /etc/filesystems assumes that only the root file system
	* is created and ready.  As new file systems are added, change the check,
	* mount, free, log, vol and vfs entries for the appropriate stanza.

	/:
		dev       = /dev/hd4
		vol       = "root"
		mount     = automatic
		check     = false
		free      = true
		vfs       = jfs2
		log       = /dev/hd8
		type      = bootfs

	/proc:
		dev       = /proc
		vol       = "/proc"
		mount     = true
		check     = false
		free      = false
		vfs       = procfs

	/scratch:
		dev       = /dev/fslv02
		vfs       = jfs2
		log       = INLINE
		mount     = true
		account   = false


=head2 Example /usr/sbin/mount output


	  node       mounted        mounted over    vfs       date        options      
	-------- ---------------  ---------------  ------ ------------ --------------- 
			 /dev/hd4         /                jfs2   Mar 24 12:14 rw,log=/dev/hd8 
			 /proc            /proc            procfs Mar 24 12:15 rw              
			 /dev/fslv02      /scratch         jfs2   Mar 24 12:15 rw,log=INLINE   


=head2 filesystems(4)

Manpage includes all known options, describes the format
and comment char's.

=head1 VERSION

$Id: Aix.pm 185 2010-07-15 19:25:30Z trevor $

=head1 AUTHOR

Nicola Worthington <nicolaw@cpan.org> - L<http://perlgirl.org.uk>

Jens Rehsack <rehsack@cpan.org> - L<http://www.rehsack.de/>

=head1 COPYRIGHT

Copyright 2004,2005,2006 Nicola Worthington.

Copyright 2008-2010 Jens Rehsack.

This software is licensed under The Apache Software License, Version 2.0.

L<http://www.apache.org/licenses/LICENSE-2.0>

=cut

