package File::ExtAttr;

=head1 NAME

File::ExtAttr - Perl extension for accessing extended attributes of files

=head1 SYNOPSIS

  use File::ExtAttr ':all';
  use IO::File;
  
  # Manipulate the extended attributes of files.
  setfattr('foo.txt', 'colour', 'red') || die;
  my $colour = getfattr('bar.txt', 'colour');
  if (defined($colour))
  {
      print $colour;
      delfattr('bar.txt', 'colour');
  }
  
  # Manipulate the extended attributes of a file via a file handle.
  my $fh = new IO::File('<foo.txt') || die;
  setfattr($fh, 'colour', 'red') || die;
  
  $fh = new IO::File('<bar.txt') || die;
  $colour = getfattr($fh, 'colour');
  if (defined($colour))
  {
      print $colour;
      delfattr($fh, 'colour');
  }

  # List attributes in the default namespace.
  print "Attributes of bar.txt:\n";
  foreach (listfattr($fh))
  {
    print "\t$_\n";
  }

  # Examine attributes in a namespace-aware manner.
  my @namespaces = listfattrns($fh);

  foreach my $ns (@namespaces)
  {
    print "Attributes in namespace '$ns': ";
    my @attrs = listfattr($fh, { namespace => $ns });
    print join(',', @attrs)."\n";
  }

=head1 DESCRIPTION

File::ExtAttr is a Perl module providing access to the extended attributes
of files.

Extended attributes are metadata associated with a file.
Examples are access control lists (ACLs) and other security parameters.
But users can add their own key=value pairs.

Extended attributes may not be supported by your operating system.
This module is aimed at Linux, Unix or Unix-like operating systems
(e.g.: Mac OS X, FreeBSD, NetBSD, Solaris).

Extended attributes may also not be supported by your filesystem
or require special options to be enabled for a particular filesystem.
E.g.:

  mount -o user_xattr /dev/hda1 /some/path

=head2 Supported OSes

=over 4

=item Linux

=item Mac OS X

=item FreeBSD 5.0 and later

=item NetBSD 4.0 and later

=item Solaris 10 and later

=back

=head2 Unsupported OSes

=over 4

=item OpenBSD

=back

=head2 Namespaces

Some implementations of extended attributes support namespacing.
In those implementations, the attribute is referred to by namespace
and attribute name.

=over 4

=item Linux

The primary namespaces are C<user> for user programs;
C<security>, C<system> and C<trusted> for file security/access-control.
See L<http://www.die.net/doc/linux/man/man5/attr.5.html>
for more details.

Namespaces on Linux are described by a string, but only certain values
are supported by filesystems. In general C<user>, C<security>, C<system>
and C<trusted> are supported, by others may be supported --
e.g.: C<os2> on JFS. File::Extattr will be able to access any of these.

=item FreeBSD, NetBSD

*BSD have two namespaces: C<user> and C<system>.

Namespaces on *BSD are described by an integer. File::ExtAttr will only
be able to access attributes in C<user> and C<system>.

=item Mac OS X

OS X has no support for namespaces.

=item Solaris

Solaris has no support for namespaces.

=back

=head2 Flags

The functions take a hash reference as their final parameter,
which can specify flags to modify the behaviour of the functions.
The flags specific to a function are documented in the function's
description.

All functions support a C<namespace> flag. E.g.:

  use File::ExtAttr ':all';
  use IO::File;
  
  # Manipulate the extended attributes of files.
  setfattr('foo.txt', 'colour', 'red') || die;
  my $colour = getfattr('bar.txt', 'colour', { namespace => 'user');

If no namespace is specified, the default namespace will be used.
On Linux and *BSD the default namespace will be C<user>.

=cut

use strict;
use warnings;
use Carp;
use Scalar::Util;

require Exporter;
use AutoLoader;

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use File::ExtAttr ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
  getfattr
  setfattr
  delfattr
  listfattr
  listfattrns
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
);

our $VERSION = '1.09';

#this is used by getxattr(), needs documentation
$File::ExtAttr::MAX_INITIAL_VALUELEN = 255;

require XSLoader;
XSLoader::load('File::ExtAttr', $VERSION);

# Preloaded methods go here.

=head1 METHODS

=over 4

=cut

sub _is_fh
{
    my $file = shift;
    my $is_fh = 0;

    eval
    {
        # TODO: Does this work with Perl 5.005, 5.6.x?
        # Relies on autovivification of filehandles?
        $is_fh = 1 if ($file->isa('IO::Handle'));

        # TODO: Does this work with Perl 5.005, 5.6.x?
        # Better solution for detecting a file handle?
        $is_fh = 1 if (openhandle($file));
    };

    return $is_fh;
}

=item getfattr([$filename | $filehandle], $attrname, [\%flags])

Return the value of the attribute named C<$attrname>
for the file named C<$filename> or referenced by the open filehandle
C<$filehandle> (which should be an IO::Handle or subclass thereof).

If no attribute is found, returns C<undef>. Otherwise gives a warning.

=cut

sub getfattr
{
    my $file = shift;

    return _is_fh($file)
        # File handle
        ? _fgetfattr($file->fileno(), @_)
        # Filename
        : _getfattr($file, @_);
}

=item setfattr([$filename | $filehandle], $attrname, $attrval, [\%flags])

Set the attribute named C<$attrname> with the value C<$attrval>
for the file named C<$filename> or referenced by the open filehandle
C<$filehandle> (which should be an IO::Handle or subclass thereof).

C<%flags> allows control of whether the attribute should be created
or should replace an existing attribute's value. If the key C<create>
is true, setfattr will fail if the attribute already exists. If the key
C<replace> is true, setfattr will fail if the attribute
does not already exist. If neither is specified, then the attribute
will be created (if necessary) or silently replaced.

If the attribute could not be set, a warning is issued.

Note that C<create> cannot be implemented in a race-free manner on *BSD.
If your code relies on the C<create> behaviour, it may be insecure on *BSD.

=cut

sub setfattr
{
    my ($file, $attrname, $attrval, $flagsref) = @_;

    die "Only one of the 'create' and 'replace' options can be passed to setfattr"
      if ($flagsref->{create} && $flagsref->{replace});

    return _is_fh($file)
        # File handle
        ? _fsetfattr($file->fileno(), $attrname, $attrval, $flagsref)
        # Filename
        : _setfattr($file, $attrname, $attrval, $flagsref);
}

=item delfattr([$filename | $filehandle], $attrname, [\%flags])

Delete the attribute named C<$attrname> for the file named C<$filename>
or referenced by the open filehandle C<$filehandle>
(which should be an IO::Handle or subclass thereof).

Returns true on success, otherwise false and a warning is issued.

=cut

sub delfattr
{
    my $file = shift;

    return _is_fh($file)
        # File handle
        ? _fdelfattr($file->fileno(), @_)
        # Filename
        : _delfattr($file, @_);
}

=item listfattr([$filename | $filehandle], [\%flags])

Return an array of the attributes on the file named C<$filename>
or referenced by the open filehandle C<$filehandle> (which should be
an IO::Handle or subclass thereof).

Returns undef on failure and $! will be set.

=cut

sub listfattr
{
    my $file = shift;

    return _is_fh($file)
        # File handle
        ? _listfattr('', $file->fileno(), @_)
        # Filename
        : _listfattr($file, -1, @_);
}

=item listfattrns([$filename | $filehandle], [\%flags])

Return an array containing the namespaces of attributes on the file named
C<$filename> or referenced by the open filehandle C<$filehandle>
(which should be an IO::Handle or subclass thereof).

Returns undef on failure and $! will be set.

=cut

sub listfattrns
{
    my $file = shift;

    return _is_fh($file)
        # File handle
        ? _listfattrns('', $file->fileno(), @_)
        # Filename
        : _listfattrns($file, -1, @_);
}

=back

=cut

# TODO: l* functions

=head1 EXPORT

None by default.

You can request that C<getfattr>, C<setfattr>, C<delfattr>
and C<listfattr> be exported using the tag ":all".

=head2 Exportable constants

None

=head1 BUGS

You cannot set empty attributes on Mac OS X 10.4 and earlier.
This is a bug in Darwin, rather than File::ExtAttr.

=head1 SEE ALSO

The latest version of this software should be available from its
home page: L<http://sourceforge.net/projects/file-extattr/>

L<OS2::ExtAttr> provides access to extended attributes on OS/2.

Eiciel, L<http://rofi.pinchito.com/eiciel/>, is an access control list (ACL)
editor for GNOME; the ACLs are stored in extended attributes.

Various low-level APIs exist for manipulating extended attributes:

=over 4

=item Linux

getattr(2), attr(5)

L<http://www.die.net/doc/linux/man/man2/getxattr.2.html>

L<http://www.die.net/doc/linux/man/man5/attr.5.html>

=item OpenBSD

OpenBSD 3.7 supported extended attributes, although support was never
built into the default GENERIC kernel. Its support was documented
in the C<extattr> man page:

L<http://www.openbsd.org/cgi-bin/man.cgi?query=extattr_get_file&apropos=0&sektion=0&manpath=OpenBSD+Current&arch=i386&format=html>

Support was removed in OpenBSD 3.8 -- see the CVS history
for the include file C<sys/extattr.h>.

L<http://www.openbsd.org/cgi-bin/cvsweb/src/sys/sys/Attic/extattr.h>

=item FreeBSD

FreeBSD >= 5.0 supports extended attributes.

extattr(2)

L<http://www.freebsd.org/cgi/man.cgi?query=extattr&sektion=2&apropos=0&manpath=FreeBSD+6.0-RELEASE+and+Ports>

=item NetBSD

NetBSD >= 3.0 supports extended attributes, but you'll need to use
NetBSD >= 4.0 to get UFS filesystem support for them.

L<http://netbsd.gw.com/cgi-bin/man-cgi?extattr_get_file+2+NetBSD-current>

L<http://www.netbsd.org/Changes/changes-4.0.html#ufs>

=item Mac OS X

getxattr(2)

L<http://developer.apple.com/documentation/Darwin/Reference/ManPages/man2/getxattr.2.html>

L<http://arstechnica.com/reviews/os/macosx-10.4.ars/7>

=item Solaris

attropen(3C), fsattr(5)

L<http://docsun.cites.uiuc.edu/sun_docs/C/solaris_9/SUNWaman/hman3c/attropen.3c.html>

L<http://docsun.cites.uiuc.edu/sun_docs/C/solaris_9/SUNWaman/hman5/fsattr.5.html>

Solaris also has extensible system attributes, which are used
by Solaris's CIFS support on ZFS, and have a confusingly similar
name to extended file attributes. These system attributes are stored
in extended file attributes called SUNWattr_ro and SUNWattr_rw.
See PSARC 2007/315 for more details:

L<http://opensolaris.org/os/community/arc/caselog/2007/315/spec-final-txt/>

=back

=head1 AUTHOR

Kevin M. Goess, E<lt>kgoess@ensenda.comE<gt>

Richard Dawe, E<lt>richdawe@cpan.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2005 by Kevin M. Goess

Copyright (C) 2005, 2006, 2007, 2008 by Richard Dawe

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.5 or,
at your option, any later version of Perl 5 you may have available.

=cut

1;
__END__
