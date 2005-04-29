#! perl -w
# $Revision: 1.1 $

# Copyright (c) 2000-2002 Ned Konz. All rights reserved.  This program is free
# software; you can redistribute it and/or modify it under the same terms as
# Perl itself.

# ----------------------------------------------------------------------
# class Archive::Zip
# Note that the package Archive::Zip exists only for exporting and
# sharing constants. Everything else is in another package
# in this file.
# Creation of a new Archive::Zip object actually creates a new object
# of class Archive::Zip::Archive.
# ----------------------------------------------------------------------

package Archive::Zip;
require 5.003_96;
use strict;

use Carp();
use IO::File();
use IO::Seekable();
use Compress::Zlib();
use File::Spec 0.8 ();
use File::Temp();

# use sigtrap qw(die normal-signals);	# is this needed?

use vars qw( @ISA @EXPORT_OK %EXPORT_TAGS $VERSION $ChunkSize $ErrorHandler );

# This is the size we'll try to read, write, and (de)compress.
# You could set it to something different if you had lots of memory
# and needed more speed.
$ChunkSize = 32768;

$ErrorHandler = \&Carp::carp;

# BEGIN block is necessary here so that other modules can use the constants.
BEGIN
{
	require Exporter;

	$VERSION = "1.14";
	@ISA = qw( Exporter );

	my @ConstantNames = qw( FA_MSDOS FA_UNIX GPBF_ENCRYPTED_MASK
	  GPBF_DEFLATING_COMPRESSION_MASK GPBF_HAS_DATA_DESCRIPTOR_MASK
	  COMPRESSION_STORED COMPRESSION_DEFLATED COMPRESSION_LEVEL_NONE
	  COMPRESSION_LEVEL_DEFAULT COMPRESSION_LEVEL_FASTEST
	  COMPRESSION_LEVEL_BEST_COMPRESSION IFA_TEXT_FILE_MASK IFA_TEXT_FILE
	  IFA_BINARY_FILE );

	my @MiscConstantNames = qw( FA_AMIGA FA_VAX_VMS FA_VM_CMS FA_ATARI_ST
	  FA_OS2_HPFS FA_MACINTOSH FA_Z_SYSTEM FA_CPM FA_TOPS20
	  FA_WINDOWS_NTFS FA_QDOS FA_ACORN FA_VFAT FA_MVS FA_BEOS FA_TANDEM
	  FA_THEOS GPBF_IMPLODING_8K_SLIDING_DICTIONARY_MASK
	  GPBF_IMPLODING_3_SHANNON_FANO_TREES_MASK
	  GPBF_IS_COMPRESSED_PATCHED_DATA_MASK COMPRESSION_SHRUNK
	  DEFLATING_COMPRESSION_NORMAL DEFLATING_COMPRESSION_MAXIMUM
	  DEFLATING_COMPRESSION_FAST DEFLATING_COMPRESSION_SUPER_FAST
	  COMPRESSION_REDUCED_1 COMPRESSION_REDUCED_2 COMPRESSION_REDUCED_3
	  COMPRESSION_REDUCED_4 COMPRESSION_IMPLODED COMPRESSION_TOKENIZED
	  COMPRESSION_DEFLATED_ENHANCED
	  COMPRESSION_PKWARE_DATA_COMPRESSION_LIBRARY_IMPLODED );

	my @ErrorCodeNames = qw( AZ_OK AZ_STREAM_END AZ_ERROR AZ_FORMAT_ERROR
	  AZ_IO_ERROR );

	my @PKZipConstantNames = qw( SIGNATURE_FORMAT SIGNATURE_LENGTH
	  LOCAL_FILE_HEADER_SIGNATURE LOCAL_FILE_HEADER_FORMAT
	  LOCAL_FILE_HEADER_LENGTH CENTRAL_DIRECTORY_FILE_HEADER_SIGNATURE
	  DATA_DESCRIPTOR_FORMAT DATA_DESCRIPTOR_LENGTH DATA_DESCRIPTOR_SIGNATURE
	  DATA_DESCRIPTOR_FORMAT_NO_SIG DATA_DESCRIPTOR_LENGTH_NO_SIG
	  CENTRAL_DIRECTORY_FILE_HEADER_FORMAT CENTRAL_DIRECTORY_FILE_HEADER_LENGTH
	  END_OF_CENTRAL_DIRECTORY_SIGNATURE END_OF_CENTRAL_DIRECTORY_SIGNATURE_STRING
	  END_OF_CENTRAL_DIRECTORY_FORMAT END_OF_CENTRAL_DIRECTORY_LENGTH );

	my @UtilityMethodNames = qw( _error _printError _ioError _formatError
	  _subclassResponsibility _binmode _isSeekable _newFileHandle _readSignature
	  _asZipDirName);

	@EXPORT_OK   = ('computeCRC32');
	%EXPORT_TAGS = (
		'CONSTANTS'      => \@ConstantNames,
		'MISC_CONSTANTS' => \@MiscConstantNames,
		'ERROR_CODES'    => \@ErrorCodeNames,

		# The following two sets are for internal use only
		'PKZIP_CONSTANTS' => \@PKZipConstantNames,
		'UTILITY_METHODS' => \@UtilityMethodNames
	);

	# Add all the constant names and error code names to @EXPORT_OK
	Exporter::export_ok_tags(
		'CONSTANTS',       'ERROR_CODES',
		'PKZIP_CONSTANTS', 'UTILITY_METHODS',
		'MISC_CONSTANTS'
	);
}

# ------------------------- begin exportable error codes -------------------

use constant AZ_OK           => 0;
use constant AZ_STREAM_END   => 1;
use constant AZ_ERROR        => 2;
use constant AZ_FORMAT_ERROR => 3;
use constant AZ_IO_ERROR     => 4;

# ------------------------- end exportable error codes ---------------------
# ------------------------- begin exportable constants ---------------------

# File types
# Values of Archive::Zip::Member->fileAttributeFormat()

use constant FA_MSDOS        => 0;
use constant FA_AMIGA        => 1;
use constant FA_VAX_VMS      => 2;
use constant FA_UNIX         => 3;
use constant FA_VM_CMS       => 4;
use constant FA_ATARI_ST     => 5;
use constant FA_OS2_HPFS     => 6;
use constant FA_MACINTOSH    => 7;
use constant FA_Z_SYSTEM     => 8;
use constant FA_CPM          => 9;
use constant FA_TOPS20       => 10;
use constant FA_WINDOWS_NTFS => 11;
use constant FA_QDOS         => 12;
use constant FA_ACORN        => 13;
use constant FA_VFAT         => 14;
use constant FA_MVS          => 15;
use constant FA_BEOS         => 16;
use constant FA_TANDEM       => 17;
use constant FA_THEOS        => 18;

# general-purpose bit flag masks
# Found in Archive::Zip::Member->bitFlag()

use constant GPBF_ENCRYPTED_MASK             => 1 << 0;
use constant GPBF_DEFLATING_COMPRESSION_MASK => 3 << 1;
use constant GPBF_HAS_DATA_DESCRIPTOR_MASK   => 1 << 3;

# deflating compression types, if compressionMethod == COMPRESSION_DEFLATED
# ( Archive::Zip::Member->bitFlag() & GPBF_DEFLATING_COMPRESSION_MASK )

use constant DEFLATING_COMPRESSION_NORMAL     => 0 << 1;
use constant DEFLATING_COMPRESSION_MAXIMUM    => 1 << 1;
use constant DEFLATING_COMPRESSION_FAST       => 2 << 1;
use constant DEFLATING_COMPRESSION_SUPER_FAST => 3 << 1;

# compression method

# these two are the only ones supported in this module
use constant COMPRESSION_STORED   => 0;    # file is stored (no compression)
use constant COMPRESSION_DEFLATED => 8;    # file is Deflated

use constant COMPRESSION_LEVEL_NONE             => 0;
use constant COMPRESSION_LEVEL_DEFAULT          => -1;
use constant COMPRESSION_LEVEL_FASTEST          => 1;
use constant COMPRESSION_LEVEL_BEST_COMPRESSION => 9;

# internal file attribute bits
# Found in Archive::Zip::Member::internalFileAttributes()

use constant IFA_TEXT_FILE_MASK => 1;
use constant IFA_TEXT_FILE      => 1;      # file is apparently text
use constant IFA_BINARY_FILE    => 0;

# PKZIP file format miscellaneous constants (for internal use only)
use constant SIGNATURE_FORMAT => "V";
use constant SIGNATURE_LENGTH => 4;

# these lengths are without the signature.
use constant LOCAL_FILE_HEADER_SIGNATURE => 0x04034b50;
use constant LOCAL_FILE_HEADER_FORMAT    => "v3 V4 v2";
use constant LOCAL_FILE_HEADER_LENGTH    => 26;

# PKZIP docs don't mention the signature, but Info-Zip writes it.
use constant DATA_DESCRIPTOR_SIGNATURE => 0x08074b50;
use constant DATA_DESCRIPTOR_FORMAT    => "V3";
use constant DATA_DESCRIPTOR_LENGTH    => 12;

# but the signature is apparently optional.
use constant DATA_DESCRIPTOR_FORMAT_NO_SIG => "V2";
use constant DATA_DESCRIPTOR_LENGTH_NO_SIG => 8;

use constant CENTRAL_DIRECTORY_FILE_HEADER_SIGNATURE => 0x02014b50;
use constant CENTRAL_DIRECTORY_FILE_HEADER_FORMAT    => "C2 v3 V4 v5 V2";
use constant CENTRAL_DIRECTORY_FILE_HEADER_LENGTH    => 42;

use constant END_OF_CENTRAL_DIRECTORY_SIGNATURE        => 0x06054b50;
use constant END_OF_CENTRAL_DIRECTORY_SIGNATURE_STRING =>
  pack( "V", END_OF_CENTRAL_DIRECTORY_SIGNATURE );
use constant END_OF_CENTRAL_DIRECTORY_FORMAT => "v4 V2 v";
use constant END_OF_CENTRAL_DIRECTORY_LENGTH => 18;

use constant GPBF_IMPLODING_8K_SLIDING_DICTIONARY_MASK => 1 << 1;
use constant GPBF_IMPLODING_3_SHANNON_FANO_TREES_MASK  => 1 << 2;
use constant GPBF_IS_COMPRESSED_PATCHED_DATA_MASK      => 1 << 5;

# the rest of these are not supported in this module
use constant COMPRESSION_SHRUNK    => 1;    # file is Shrunk
use constant COMPRESSION_REDUCED_1 => 2;    # file is Reduced CF=1
use constant COMPRESSION_REDUCED_2 => 3;    # file is Reduced CF=2
use constant COMPRESSION_REDUCED_3 => 4;    # file is Reduced CF=3
use constant COMPRESSION_REDUCED_4 => 5;    # file is Reduced CF=4
use constant COMPRESSION_IMPLODED  => 6;    # file is Imploded
use constant COMPRESSION_TOKENIZED => 7;    # reserved for Tokenizing compr.
use constant COMPRESSION_DEFLATED_ENHANCED => 9;   # reserved for enh. Deflating
use constant COMPRESSION_PKWARE_DATA_COMPRESSION_LIBRARY_IMPLODED => 10;

# ------------------------- end of exportable constants ---------------------

use constant ZIPARCHIVECLASS => 'Archive::Zip::Archive';
use constant ZIPMEMBERCLASS  => 'Archive::Zip::Member';

sub new    # Archive::Zip
{
	my $class = shift;
	return $class->ZIPARCHIVECLASS->new(@_);
}

sub computeCRC32    # Archive::Zip
{
	my $data = shift;
	$data = shift if ref($data);    # allow calling as an obj method
	my $crc = shift;
	return Compress::Zlib::crc32( $data, $crc );
}

# Report or change chunk size used for reading and writing.
# Also sets Zlib's default buffer size (eventually).
sub setChunkSize    # Archive::Zip
{
	my $chunkSize = shift;
	$chunkSize = shift if ref($chunkSize);    # object method on zip?
	my $oldChunkSize = $Archive::Zip::ChunkSize;
	$Archive::Zip::ChunkSize = $chunkSize if ($chunkSize);
	return $oldChunkSize;
}

sub chunkSize    # Archive::Zip
{
	return $Archive::Zip::ChunkSize;
}

sub setErrorHandler (&)    # Archive::Zip
{
	my $errorHandler = shift;
	$errorHandler = \&Carp::carp unless defined($errorHandler);
	my $oldErrorHandler = $Archive::Zip::ErrorHandler;
	$Archive::Zip::ErrorHandler = $errorHandler;
	return $oldErrorHandler;
}

# ----------------------------------------------------------------------
# Private utility functions (not methods).
# ----------------------------------------------------------------------

sub _printError    # Archive::Zip
{
	my $string = join ( ' ', @_, "\n" );
	my $oldCarpLevel = $Carp::CarpLevel;
	$Carp::CarpLevel += 2;
	&{$ErrorHandler} ($string);
	$Carp::CarpLevel = $oldCarpLevel;
}

# This is called on format errors.
sub _formatError    # Archive::Zip
{
	shift if ref( $_[0] );
	_printError( 'format error:', @_ );
	return AZ_FORMAT_ERROR;
}

# This is called on IO errors.
sub _ioError    # Archive::Zip
{
	shift if ref( $_[0] );
	_printError( 'IO error:', @_, ':', $! );
	return AZ_IO_ERROR;
}

# This is called on generic errors.
sub _error    # Archive::Zip
{
	shift if ref( $_[0] );
	_printError( 'error:', @_ );
	return AZ_ERROR;
}

# Called when a subclass should have implemented
# something but didn't
sub _subclassResponsibility    # Archive::Zip
{
	Carp::croak("subclass Responsibility\n");
}

# Try to set the given file handle or object into binary mode.
sub _binmode    # Archive::Zip
{
	my $fh = shift;
	return UNIVERSAL::can( $fh, 'binmode' ) ? $fh->binmode() : binmode($fh);
}

# Attempt to guess whether file handle is seekable.
# Because of problems with Windoze, this only returns true when
# the file handle is a real file.
sub _isSeekable    # Archive::Zip
{
	my $fh = shift;

	if ( UNIVERSAL::isa( $fh, 'IO::Scalar' ) )
	{
		return 0;
	}
	elsif ( UNIVERSAL::isa( $fh, 'IO::String' ) )
	{
		return 1;
	}
	elsif ( UNIVERSAL::can( $fh, 'stat' ) )
	{
		return -f $fh;
	}
	return UNIVERSAL::can( $fh, 'seek' );
}

# Return an opened IO::Handle
# my ( $status, fh ) = _newFileHandle( 'fileName', 'w' );
# Can take a filename, file handle, or ref to GLOB
# Or, if given something that is a ref but not an IO::Handle,
# passes back the same thing.
sub _newFileHandle    # Archive::Zip
{
	my $fd     = shift;
	my $status = 1;
	my $handle;

	if ( ref($fd) )
	{
		if ( UNIVERSAL::isa( $fd, 'IO::Scalar' )
			or UNIVERSAL::isa( $fd, 'IO::String' ) )
		{
			$handle = $fd;
		}
		elsif ( UNIVERSAL::isa( $fd, 'IO::Handle' )
			or UNIVERSAL::isa( $fd, 'GLOB' ) )
		{
			$handle = IO::File->new();
			$status = $handle->fdopen( $fd, @_ );
		}
		else
		{
			$handle = $fd;
		}
	}
	else
	{
		$handle = IO::File->new();
		$status = $handle->open( $fd, @_ );
	}

	return ( $status, $handle );
}

# Returns next signature from given file handle, leaves
# file handle positioned afterwards.
# In list context, returns ($status, $signature)
# ( $status, $signature) = _readSignature( $fh, $fileName );

sub _readSignature    # Archive::Zip
{
	my $fh                = shift;
	my $fileName          = shift;
	my $expectedSignature = shift;    # optional

	my $signatureData;
	my $bytesRead = $fh->read( $signatureData, SIGNATURE_LENGTH );
	return _ioError("reading header signature")
	  if $bytesRead != SIGNATURE_LENGTH;
	my $signature = unpack( SIGNATURE_FORMAT, $signatureData );
	my $status    = AZ_OK;

	# compare with expected signature, if any, or any known signature.
	if ( ( defined($expectedSignature) && $signature != $expectedSignature )
		|| ( !defined($expectedSignature)
			&& $signature != CENTRAL_DIRECTORY_FILE_HEADER_SIGNATURE
			&& $signature != LOCAL_FILE_HEADER_SIGNATURE
			&& $signature != END_OF_CENTRAL_DIRECTORY_SIGNATURE
			&& $signature != DATA_DESCRIPTOR_SIGNATURE ) )
	{
		my $errmsg = sprintf( "bad signature: 0x%08x", $signature );
		if ( _isSeekable($fh) )
		{
			$errmsg .=
			  sprintf( " at offset %d", $fh->tell() - SIGNATURE_LENGTH );
		}

		$status = _formatError("$errmsg in file $fileName");
	}

	return ( $status, $signature );
}

# Utility method to make and open a temp file.
# Will create $temp_dir if it doesn't exist.
# Returns file handle and name:
#
# my ($fh, $name) = Archive::Zip::tempFile();
# my ($fh, $name) = Archive::Zip::tempFile('mytempdir');
#

sub tempFile    # Archive::Zip
{
	my $dir = shift;
	my ( $fh, $filename ) = File::Temp::tempfile(
		SUFFIX => '.zip',
		UNLINK => 0,        # we will delete it!
		$dir ? ( DIR => $dir ) : ()
	);
	return ( undef, undef ) unless $fh;
	my ( $status, $newfh ) = _newFileHandle( $fh, 'w+' );
	return ( $newfh, $filename );
}

# Return the normalized directory name as used in a zip file (path
# separators become slashes, etc.). 
# Will translate internal slashes in path components (i.e. on Macs) to
# underscores.  Discards volume names.
# When $forceDir is set, returns paths with trailing slashes (or arrays
# with trailing blank members).
#
# If third argument is a reference, returns volume information there.
#
# input         output
# .				('.')	'.'
# ./a			('a')	a
# ./a/b			('a','b')	a/b
# ./a/b/		('a','b')	a/b
# a/b/			('a','b')	a/b
# /a/b/			('','a','b')	/a/b
# c:\a\b\c.doc	('','a','b','c.doc')	/a/b/c.doc		# on Windoze
# "i/o maps:whatever"	('i_o maps', 'whatever')  "i_o maps/whatever"	# on Macs
sub _asZipDirName    # Archive::Zip
{
	my $name      = shift;
	my $forceDir  = shift;
	my $volReturn = shift;
	my ( $volume, $directories, $file ) =
	  File::Spec->splitpath( File::Spec->canonpath($name), $forceDir );
	$$volReturn = $volume if ( ref($volReturn) );
	my @dirs = map { $_ =~ s{/}{_}g; $_ } File::Spec->splitdir($directories);
	if ( @dirs > 0 ) { pop (@dirs) unless $dirs[-1] }   # remove empty component
	push ( @dirs, $file || '' );
	return wantarray ? @dirs : join ( '/', @dirs );
}

# Return an absolute local name for a zip name.
# Assume a directory if zip name has trailing slash.
# Takes an optional volume name in FS format (like 'a:').
#
sub _asLocalName    # Archive::Zip
{
	my $name   = shift;    # zip format
	my $volume = shift;
	$volume = '' unless defined($volume);    # local FS format

	my @paths = split ( /\//, $name );
	my $filename = pop (@paths);
	$filename = '' unless defined($filename);
	my $localDirs = File::Spec->catdir(@paths);
	my $localName = File::Spec->catpath( $volume, $localDirs, $filename );
	$localName = File::Spec->rel2abs($localName) unless $volume;
	return $localName;
}

# ----------------------------------------------------------------------
# class Archive::Zip::Archive (concrete)
# Generic ZIP archive.
# ----------------------------------------------------------------------
package Archive::Zip::Archive;
use File::Path;
use File::Find();
use File::Spec();
use File::Copy();
use File::Basename;
use Cwd;

use vars qw( @ISA );
@ISA = qw( Archive::Zip );

BEGIN
{
	use Archive::Zip qw( :CONSTANTS :ERROR_CODES :PKZIP_CONSTANTS
	  :UTILITY_METHODS );
}

# Note that this returns undef on read errors, else new zip object.

sub new    # Archive::Zip::Archive
{
	my $class = shift;
	my $self = bless( {
		  'diskNumber'                            => 0,
		  'diskNumberWithStartOfCentralDirectory' => 0,
		  'numberOfCentralDirectoriesOnThisDisk'  => 0,   # shld be # of members
		  'numberOfCentralDirectories'            => 0,   # shld be # of members
		  'centralDirectorySize' => 0,    # must re-compute on write
		  'centralDirectoryOffsetWRTStartingDiskNumber' => 0,  # must re-compute
		  'writeEOCDOffset'             => 0,
		  'writeCentralDirectoryOffset' => 0,
		  'zipfileComment'              => '',
		  'eocdOffset'                  => 0,
		  'fileName'                    => ''
	  },
	  $class
	);
	$self->{'members'} = [];
	if (@_)
	{
		my $status = $self->read(@_);
		return $status == AZ_OK ? $self : undef;
	}
	return $self;
}

sub members    # Archive::Zip::Archive
{
	@{ shift->{'members'} };
}

sub numberOfMembers    # Archive::Zip::Archive
{
	scalar( shift->members() );
}

sub memberNames    # Archive::Zip::Archive
{
	my $self = shift;
	return map { $_->fileName() } $self->members();
}

# return ref to member with given name or undef
sub memberNamed    # Archive::Zip::Archive
{
	my ( $self, $fileName ) = @_;
	foreach my $member ( $self->members() )
	{
		return $member if $member->fileName() eq $fileName;
	}
	return undef;
}

sub membersMatching    # Archive::Zip::Archive
{
	my ( $self, $pattern ) = @_;
	return grep { $_->fileName() =~ /$pattern/ } $self->members();
}

sub diskNumber    # Archive::Zip::Archive
{
	shift->{'diskNumber'};
}

sub diskNumberWithStartOfCentralDirectory    # Archive::Zip::Archive
{
	shift->{'diskNumberWithStartOfCentralDirectory'};
}

sub numberOfCentralDirectoriesOnThisDisk    # Archive::Zip::Archive
{
	shift->{'numberOfCentralDirectoriesOnThisDisk'};
}

sub numberOfCentralDirectories    # Archive::Zip::Archive
{
	shift->{'numberOfCentralDirectories'};
}

sub centralDirectorySize    # Archive::Zip::Archive
{
	shift->{'centralDirectorySize'};
}

sub centralDirectoryOffsetWRTStartingDiskNumber    # Archive::Zip::Archive
{
	shift->{'centralDirectoryOffsetWRTStartingDiskNumber'};
}

sub zipfileComment    # Archive::Zip::Archive
{
	my $self    = shift;
	my $comment = $self->{'zipfileComment'};
	if (@_)
	{
		$self->{'zipfileComment'} = pack( 'C0a*', shift () );    # avoid unicode
	}
	return $comment;
}

sub eocdOffset    # Archive::Zip::Archive
{
	shift->{'eocdOffset'};
}

# Return the name of the file last read.
sub fileName    # Archive::Zip::Archive
{
	shift->{'fileName'};
}

sub removeMember    # Archive::Zip::Archive
{
	my ( $self, $member ) = @_;
	$member = $self->memberNamed($member) unless ref($member);
	return undef unless $member;
	my @newMembers = grep { $_ != $member } $self->members();
	$self->{'members'} = \@newMembers;
	return $member;
}

sub replaceMember    # Archive::Zip::Archive
{
	my ( $self, $oldMember, $newMember ) = @_;
	$oldMember = $self->memberNamed($oldMember) unless ref($oldMember);
	return undef unless $oldMember;
	return undef unless $newMember;
	my @newMembers =
	  map { ( $_ == $oldMember ) ? $newMember : $_ } $self->members();
	$self->{'members'} = \@newMembers;
	return $oldMember;
}

sub extractMember    # Archive::Zip::Archive
{
	my $self   = shift;
	my $member = shift;
	$member = $self->memberNamed($member) unless ref($member);
	return _error('member not found') unless $member;
	my $name = shift;    # local FS name if given
	my ( $volumeName, $dirName, $fileName );
	if ( defined($name) )
	{
		( $volumeName, $dirName, $fileName ) = File::Spec->splitpath($name);
		$dirName = File::Spec->catpath( $volumeName, $dirName, '' );
	}
	else
	{
		$name = $member->fileName();
		( $dirName = $name ) =~ s{[^/]*$}{};
		$dirName = Archive::Zip::_asLocalName($dirName);
		$name    = Archive::Zip::_asLocalName($name);
	}
	if ( $dirName && !-d $dirName )
	{
		mkpath($dirName);
		return _ioError("can't create dir $dirName") if ( !-d $dirName );
	}
	return $member->extractToFileNamed( $name, @_ );
}

sub extractMemberWithoutPaths    # Archive::Zip::Archive
{
	my $self   = shift;
	my $member = shift;
	$member = $self->memberNamed($member) unless ref($member);
	return _error('member not found') unless $member;
	return AZ_OK if $member->isDirectory();
	my $name = shift;
	unless ($name)
	{
		$name = $member->fileName();
		$name =~ s{.*/}{};    # strip off directories, if any
		$name = Archive::Zip::_asLocalName($name);
	}
	return $member->extractToFileNamed( $name, @_ );
}

sub addMember    # Archive::Zip::Archive
{
	my ( $self, $newMember ) = @_;
	push ( @{ $self->{'members'} }, $newMember ) if $newMember;
	return $newMember;
}

sub addFile    # Archive::Zip::Archive
{
	my $self      = shift;
	my $fileName  = shift;
	my $newName   = shift;
	my $newMember = $self->ZIPMEMBERCLASS->newFromFile( $fileName, $newName );
	$self->addMember($newMember) if defined($newMember);
	return $newMember;
}

sub addString    # Archive::Zip::Archive
{
	my $self      = shift;
	my $newMember = $self->ZIPMEMBERCLASS->newFromString(@_);
	return $self->addMember($newMember);
}

sub addDirectory    # Archive::Zip::Archive
{
	my ( $self, $name, $newName ) = @_;
	my $newMember = $self->ZIPMEMBERCLASS->newDirectoryNamed( $name, $newName );
	$self->addMember($newMember);
	return $newMember;
}

# add either a file or a directory.

sub addFileOrDirectory
{
	my ( $self, $name, $newName ) = @_;
	if ( -f $name )
	{
		( $newName =~ s{/$}{} ) if $newName;
		return $self->addFile( $name, $newName );
	}
	elsif ( -d $name )
	{
		( $newName =~ s{[^/]$}{&/} ) if $newName;
		return $self->addDirectory( $name, $newName );
	}
	else
	{
		return _error("$name is neither a file nor a directory");
	}
}

sub contents    # Archive::Zip::Archive
{
	my ( $self, $member, $newContents ) = @_;
	$member = $self->memberNamed($member) unless ref($member);
	return undef unless $member;
	return $member->contents($newContents);
}

sub writeToFileNamed    # Archive::Zip::Archive
{
	my $self     = shift;
	my $fileName = shift;    # local FS format
	foreach my $member ( $self->members() )
	{
		if ( $member->_usesFileNamed($fileName) )
		{
			return _error( "$fileName is needed by member "
				. $member->fileName()
				. "; consider using overwrite() or overwriteAs() instead." );
		}
	}
	my ( $status, $fh ) = _newFileHandle( $fileName, 'w' );
	return _ioError("Can't open $fileName for write") unless $status;
	my $retval = $self->writeToFileHandle( $fh, 1 );
	$fh->close();
	$fh = undef;

	return $retval;
}

# It is possible to write data to the FH before calling this,
# perhaps to make a self-extracting archive.
sub writeToFileHandle    # Archive::Zip::Archive
{
	my $self         = shift;
	my $fh           = shift;
	my $fhIsSeekable = @_ ? shift: _isSeekable($fh);
	_binmode($fh);

	# Find out where the current position is.
	my $offset = $fhIsSeekable ? $fh->tell() : 0;
	$offset = 0 if $offset < 0;

	foreach my $member ( $self->members() )
	{
		my $retval = $member->_writeToFileHandle( $fh, $fhIsSeekable, $offset );
		$member->endRead();
		return $retval if $retval != AZ_OK;
		$offset += $member->_localHeaderSize() + $member->_writeOffset();
		$offset += $member->hasDataDescriptor()
		  ? DATA_DESCRIPTOR_LENGTH + SIGNATURE_LENGTH
		  : 0;

		# changed this so it reflects the last successful position
		$self->{'writeCentralDirectoryOffset'} = $offset;
	}
	return $self->writeCentralDirectory($fh);
}

# Write zip back to the original file,
# as safely as possible.
# Returns AZ_OK if successful.
sub overwrite    # Archive::Zip::Archive
{
	my $self = shift;
	return $self->overwriteAs( $self->{'fileName'} );
}

# Write zip to the specified file,
# as safely as possible.
# Returns AZ_OK if successful.
sub overwriteAs    # Archive::Zip::Archive
{
	my $self    = shift;
	my $zipName = shift;
	return _error("no filename in overwriteAs()") unless defined($zipName);

	my ( $fh, $tempName ) = Archive::Zip::tempFile();
	return _error( "Can't open temp file", $! ) unless $fh;

	( my $backupName = $zipName ) =~ s{(\.[^.]*)?$}{.zbk};

	my $status = $self->writeToFileHandle($fh);
	$fh->close();
	$fh = undef;

	if ( $status != AZ_OK )
	{
		unlink($tempName);
		_printError("Can't write to $tempName");
		return $status;
	}

	my $err;

	# rename the zip
	if ( -f $zipName && !rename( $zipName, $backupName ) )
	{
		$err = $!;
		unlink($tempName);
		return _error( "Can't rename $zipName as $backupName", $err );
	}

	# move the temp to the original name (possibly copying)
	unless ( File::Copy::move( $tempName, $zipName ) )
	{
		$err = $!;
		rename( $backupName, $zipName );
		unlink($tempName);
		return _error( "Can't move $tempName to $zipName", $err );
	}

	# unlink the backup
	if ( -f $backupName && !unlink($backupName) )
	{
		$err = $!;
		return _error( "Can't unlink $backupName", $err );
	}

	return AZ_OK;
}

# Used only during writing
sub _writeCentralDirectoryOffset    # Archive::Zip::Archive
{
	shift->{'writeCentralDirectoryOffset'};
}

sub _writeEOCDOffset    # Archive::Zip::Archive
{
	shift->{'writeEOCDOffset'};
}

# Expects to have _writeEOCDOffset() set
sub _writeEndOfCentralDirectory    # Archive::Zip::Archive
{
	my ( $self, $fh ) = @_;

	$fh->print(END_OF_CENTRAL_DIRECTORY_SIGNATURE_STRING)
	  or return _ioError('writing EOCD Signature');
	my $zipfileCommentLength = length( $self->zipfileComment() );

	my $header = pack(
		END_OF_CENTRAL_DIRECTORY_FORMAT,
		0,                          # {'diskNumber'},
		0,                          # {'diskNumberWithStartOfCentralDirectory'},
		$self->numberOfMembers(),   # {'numberOfCentralDirectoriesOnThisDisk'},
		$self->numberOfMembers(),   # {'numberOfCentralDirectories'},
		$self->_writeEOCDOffset() - $self->_writeCentralDirectoryOffset(),
		$self->_writeCentralDirectoryOffset(),
		$zipfileCommentLength
	);
	$fh->print($header)
	  or return _ioError('writing EOCD header');
	if ($zipfileCommentLength)
	{
		$fh->print( $self->zipfileComment() )
		  or return _ioError('writing zipfile comment');
	}
	return AZ_OK;
}

# $offset can be specified to truncate a zip file.
sub writeCentralDirectory    # Archive::Zip::Archive
{
	my ( $self, $fh, $offset ) = @_;

	if ( defined($offset) )
	{
		$self->{'writeCentralDirectoryOffset'} = $offset;
		$fh->seek( $offset, IO::Seekable::SEEK_SET )
		  or return _ioError('seeking to write central directory');
	}
	else
	{
		$offset = $self->_writeCentralDirectoryOffset();
	}

	foreach my $member ( $self->members() )
	{
		my $status = $member->_writeCentralDirectoryFileHeader($fh);
		return $status if $status != AZ_OK;
		$offset += $member->_centralDirectoryHeaderSize();
		$self->{'writeEOCDOffset'} = $offset;
	}
	return $self->_writeEndOfCentralDirectory($fh);
}

sub read    # Archive::Zip::Archive
{
	my $self     = shift;
	my $fileName = shift;
	return _error('No filename given') unless $fileName;
	my ( $status, $fh ) = _newFileHandle( $fileName, 'r' );
	return _ioError("opening $fileName for read") unless $status;

	$status = $self->readFromFileHandle( $fh, $fileName );
	return $status if $status != AZ_OK;

	$fh->close();
	$self->{'fileName'} = $fileName;
	return AZ_OK;
}

sub readFromFileHandle    # Archive::Zip::Archive
{
	my $self     = shift;
	my $fh       = shift;
	my $fileName = shift;
	$fileName = $fh unless defined($fileName);
	return _error('No filehandle given')   unless $fh;
	return _ioError('filehandle not open') unless $fh->opened();

	_binmode($fh);
	$self->{'fileName'} = "$fh";

	# TODO: how to support non-seekable zips?
	return _error('file not seekable')
	  unless _isSeekable($fh);

	$fh->seek( 0, 0 );    # rewind the file

	my $status = $self->_findEndOfCentralDirectory($fh);
	return $status if $status != AZ_OK;

	my $eocdPosition = $fh->tell();

	$status = $self->_readEndOfCentralDirectory($fh);
	return $status if $status != AZ_OK;

	$fh->seek( $eocdPosition - $self->centralDirectorySize(),
		IO::Seekable::SEEK_SET )
	  or return _ioError("Can't seek $fileName");

	# Try to detect garbage at beginning of archives
	# This should be 0
	$self->{'eocdOffset'} = $eocdPosition - $self->centralDirectorySize() # here
	  - $self->centralDirectoryOffsetWRTStartingDiskNumber();

	for ( ; ; )
	{
		my $newMember =
		  $self->ZIPMEMBERCLASS->_newFromZipFile( $fh, $fileName,
			$self->eocdOffset() );
		my $signature;
		( $status, $signature ) = _readSignature( $fh, $fileName );
		return $status if $status != AZ_OK;
		last if $signature == END_OF_CENTRAL_DIRECTORY_SIGNATURE;
		$status = $newMember->_readCentralDirectoryFileHeader();
		return $status if $status != AZ_OK;
		$status = $newMember->endRead();
		return $status if $status != AZ_OK;
		$newMember->_becomeDirectoryIfNecessary();
		push ( @{ $self->{'members'} }, $newMember );
	}

	return AZ_OK;
}

# Read EOCD, starting from position before signature.
# Return AZ_OK on success.
sub _readEndOfCentralDirectory    # Archive::Zip::Archive
{
	my $self = shift;
	my $fh   = shift;

	# Skip past signature
	$fh->seek( SIGNATURE_LENGTH, IO::Seekable::SEEK_CUR )
	  or return _ioError("Can't seek past EOCD signature");

	my $header = '';
	my $bytesRead = $fh->read( $header, END_OF_CENTRAL_DIRECTORY_LENGTH );
	if ( $bytesRead != END_OF_CENTRAL_DIRECTORY_LENGTH )
	{
		return _ioError("reading end of central directory");
	}

	my $zipfileCommentLength;
	( $self->{'diskNumber'},
	  $self->{'diskNumberWithStartOfCentralDirectory'},
	  $self->{'numberOfCentralDirectoriesOnThisDisk'},
	  $self->{'numberOfCentralDirectories'},
	  $self->{'centralDirectorySize'},
	  $self->{'centralDirectoryOffsetWRTStartingDiskNumber'},
	  $zipfileCommentLength )
	  = unpack( END_OF_CENTRAL_DIRECTORY_FORMAT, $header );

	if ($zipfileCommentLength)
	{
		my $zipfileComment = '';
		$bytesRead = $fh->read( $zipfileComment, $zipfileCommentLength );
		if ( $bytesRead != $zipfileCommentLength )
		{
			return _ioError("reading zipfile comment");
		}
		$self->{'zipfileComment'} = $zipfileComment;
	}

	return AZ_OK;
}

# Seek in my file to the end, then read backwards until we find the
# signature of the central directory record. Leave the file positioned right
# before the signature. Returns AZ_OK if success.
sub _findEndOfCentralDirectory    # Archive::Zip::Archive
{
	my $self = shift;
	my $fh   = shift;
	my $data = '';
	$fh->seek( 0, IO::Seekable::SEEK_END )
	  or return _ioError("seeking to end");

	my $fileLength = $fh->tell();
	if ( $fileLength < END_OF_CENTRAL_DIRECTORY_LENGTH + 4 )
	{
		return _formatError("file is too short");
	}

	my $seekOffset = 0;
	my $pos        = -1;
	for ( ; ; )
	{
		$seekOffset += 512;
		$seekOffset = $fileLength if ( $seekOffset > $fileLength );
		$fh->seek( -$seekOffset, IO::Seekable::SEEK_END )
		  or return _ioError("seek failed");
		my $bytesRead = $fh->read( $data, $seekOffset );
		if ( $bytesRead != $seekOffset )
		{
			return _ioError("read failed");
		}
		$pos = rindex( $data, END_OF_CENTRAL_DIRECTORY_SIGNATURE_STRING );
		last
		  if ( $pos >= 0
			or $seekOffset == $fileLength
			or $seekOffset >= $Archive::Zip::ChunkSize );
	}

	if ( $pos >= 0 )
	{
		$fh->seek( $pos - $seekOffset, IO::Seekable::SEEK_CUR )
		  or return _ioError("seeking to EOCD");
		return AZ_OK;
	}
	else
	{
		return _formatError("can't find EOCD signature");
	}
}

# Used to avoid taint problems when chdir'ing.
# Not intended to increase security in any way; just intended to shut up the -T
# complaints.  If your Cwd module is giving you unreliable returns from cwd()
# you have bigger problems than this.
sub _untaintDir
{
	my $dir = shift;
	$dir =~ m/\A(.+)\z/s;
	return $1;
}

sub addTree    # Archive::Zip::Archive
{
	my $self = shift;
	my $root = shift or return _error("root arg missing in call to addTree()");
	my $dest = shift;
	$dest = '' unless defined($dest);
	my $pred = shift || sub { -r };
	my @files;
	my $startDir = _untaintDir( cwd() );

	return _error( 'undef returned by _untaintDir on cwd ', cwd() )
	  unless $startDir;

	# This avoids chdir'ing in Find, in a way compatible with older
	# versions of File::Find.
	my $wanted = sub {
		local $main::_ = $File::Find::name;
		my $dir = _untaintDir($File::Find::dir);
		chdir($startDir);
		push ( @files, $File::Find::name ) if (&$pred);
		chdir($dir);
	};

	File::Find::find( $wanted, $root );

	my $rootZipName = _asZipDirName( $root, 1 );    # with trailing slash
	my $pattern = $rootZipName eq './' ? '^' : "^\Q$rootZipName\E";

	$dest = _asZipDirName( $dest, 1 );              # with trailing slash

	foreach my $fileName (@files)
	{
		my $isDir = -d $fileName;

		# normalize, remove leading ./
		my $archiveName = _asZipDirName( $fileName, $isDir );
		if ( $archiveName eq $rootZipName ) { $archiveName = $dest }
		else { $archiveName =~ s{$pattern}{$dest} }
		next if $archiveName =~ m{^\.?/?$};    # skip current dir
		my $member =
		  $isDir 
		  ? $self->addDirectory( $fileName, $archiveName )
		  : $self->addFile( $fileName, $archiveName );
		return _error("add $fileName failed in addTree()") if !$member;
	}
	return AZ_OK;
}

sub addTreeMatching    # Archive::Zip::Archive
{
	my $self = shift;
	my $root = shift
	  or return _error("root arg missing in call to addTreeMatching()");
	my $dest = shift;
	$dest = '' unless defined($dest);
	my $pattern = shift
	  or return _error("pattern missing in call to addTreeMatching()");
	my $pred    = shift;
	my $matcher =
	  $pred ? sub { m{$pattern} && &$pred } : sub { m{$pattern} && -r };
	return $self->addTree( $root, $dest, $matcher );
}

# $zip->extractTree( $root, $dest [, $volume] );
#
# $root and $dest are Unix-style.
# $volume is in local FS format.
#
sub extractTree    # Archive::Zip::Archive
{
	my $self = shift;
	my $root = shift;    # Zip format
	$root = '' unless defined($root);
	my $dest = shift;    # Zip format
	$dest = './' unless defined($dest);
	my $volume  = shift;                              # optional
	my $pattern = "^\Q$root";
	my @members = $self->membersMatching($pattern);

	foreach my $member (@members)
	{
		my $fileName = $member->fileName();    # in Unix format
		$fileName =~ s{$pattern}{$dest};       # in Unix format
		                                       # convert to platform format:
		$fileName = Archive::Zip::_asLocalName( $fileName, $volume );
		my $status = $member->extractToFileNamed($fileName);
		return $status if $status != AZ_OK;
	}
	return AZ_OK;
}

# $zip->updateMember( $memberOrName, $fileName );
# Returns (possibly updated) member, if any; undef on errors.

sub updateMember    # Archive::Zip::Archive
{
	my $self      = shift;
	my $oldMember = shift;
	my $fileName  = shift;

	if ( !defined($fileName) )
	{
		_error("updateMember(): missing fileName argument");
		return undef;
	}

	my @newStat = stat($fileName);
	if ( !@newStat )
	{
		_ioError("Can't stat $fileName");
		return undef;
	}

	my $isDir = -d _;

	my $memberName;

	if ( ref($oldMember) )
	{
		$memberName = $oldMember->fileName();
	}
	else
	{
		$oldMember = $self->memberNamed( $memberName = $oldMember )
		  || $self->memberNamed( $memberName =
			_asZipDirName( $oldMember, $isDir ) );
	}

	unless ( defined($oldMember)
		&& $oldMember->lastModTime() == $newStat[9]
		&& $oldMember->isDirectory() == $isDir
		&& ( $isDir || ( $oldMember->uncompressedSize() == $newStat[7] ) ) )
	{

		# create the new member
		my $newMember = $isDir
		  ? $self->ZIPMEMBERCLASS->newDirectoryNamed( $fileName, $memberName )
		  : $self->ZIPMEMBERCLASS->newFromFile( $fileName, $memberName );

		unless ( defined($newMember) )
		{
			_error("creation of member $fileName failed in updateMember()");
			return undef;
		}

		# replace old member or append new one
		if ( defined($oldMember) )
		{
			$self->replaceMember( $oldMember, $newMember );
		}
		else { $self->addMember($newMember); }

		return $newMember;
	}

	return $oldMember;
}

# $zip->updateTree( $root, [ $dest, [ $pred [, $mirror]]] );
#
# This takes the same arguments as addTree, but first checks to see
# whether the file or directory already exists in the zip file.
#
# If the fourth argument $mirror is true, then delete all my members
# if corresponding files weren't found.

sub updateTree    # Archive::Zip::Archive
{
	my $self = shift;
	my $root = shift
	  or return _error("root arg missing in call to updateTree()");
	my $dest = shift;
	$dest = '' unless defined($dest);
	$dest = _asZipDirName( $dest, 1 );
	my $pred = shift || sub { -r };
	my $mirror = shift;

	my $rootZipName = _asZipDirName( $root, 1 );    # with trailing slash
	my $pattern = $rootZipName eq './' ? '^' : "^\Q$rootZipName\E";

	my @files;
	my $startDir = _untaintDir( cwd() );

	return _error( 'undef returned by _untaintDir on cwd ', cwd() )
	  unless $startDir;

	# This avoids chdir'ing in Find, in a way compatible with older
	# versions of File::Find.
	my $wanted = sub {
		local $main::_ = $File::Find::name;
		my $dir = _untaintDir($File::Find::dir);
		chdir($startDir);
		push ( @files, $File::Find::name ) if (&$pred);
		chdir($dir);
	};

	File::Find::find( $wanted, $root );

	# Now @files has all the files that I could potentially be adding to
	# the zip. Only add the ones that are necessary.
	# For each file (updated or not), add its member name to @done.
	my %done;
	foreach my $fileName (@files)
	{
		my @newStat = stat($fileName);
		my $isDir   = -d _;

		# normalize, remove leading ./
		my $memberName = _asZipDirName( $fileName, $isDir );
		if ( $memberName eq $rootZipName ) { $memberName = $dest }
		else { $memberName =~ s{$pattern}{$dest} }
		next if $memberName =~ m{^\.?/?$};    # skip current dir

		$done{$memberName} = 1;
		my $changedMember = $self->updateMember( $memberName, $fileName );
		return _error("updateTree failed to update $fileName")
		  unless ref($changedMember);
	}

	# @done now has the archive names corresponding to all the found files.
	# If we're mirroring, delete all those members that aren't in @done.
	if ($mirror)
	{
		foreach my $member ( $self->members() )
		{
			$self->removeMember($member)
			  unless $done{ $member->fileName() };
		}
	}

	return AZ_OK;
}

# ----------------------------------------------------------------------
# class Archive::Zip::Member
# A generic member of an archive ( abstract )
# ----------------------------------------------------------------------
package Archive::Zip::Member;
use vars qw( @ISA );
@ISA = qw ( Archive::Zip );

BEGIN
{
	use Archive::Zip qw( :CONSTANTS :MISC_CONSTANTS :ERROR_CODES
	  :PKZIP_CONSTANTS :UTILITY_METHODS );
}

use Time::Local();
use Compress::Zlib qw( Z_OK Z_STREAM_END MAX_WBITS );
use File::Path;
use File::Basename;

use constant ZIPFILEMEMBERCLASS   => 'Archive::Zip::ZipFileMember';
use constant NEWFILEMEMBERCLASS   => 'Archive::Zip::NewFileMember';
use constant STRINGMEMBERCLASS    => 'Archive::Zip::StringMember';
use constant DIRECTORYMEMBERCLASS => 'Archive::Zip::DirectoryMember';

# Unix perms for default creation of files/dirs.
use constant DEFAULT_DIRECTORY_PERMISSIONS => 040755;
use constant DEFAULT_FILE_PERMISSIONS      => 0100666;
use constant DIRECTORY_ATTRIB              => 040000;
use constant FILE_ATTRIB                   => 0100000;

# Returns self if successful, else undef
# Assumes that fh is positioned at beginning of central directory file header.
# Leaves fh positioned immediately after file header or EOCD signature.
sub _newFromZipFile    # Archive::Zip::Member
{
	my $class = shift;
	my $self  = $class->ZIPFILEMEMBERCLASS->_newFromZipFile(@_);
	return $self;
}

sub newFromString    # Archive::Zip::Member
{
	my $class = shift;
	my $self  = $class->STRINGMEMBERCLASS->_newFromString(@_);
	return $self;
}

sub newFromFile    # Archive::Zip::Member
{
	my $class = shift;
	my $self  = $class->NEWFILEMEMBERCLASS->_newFromFileNamed(@_);
	return $self;
}

sub newDirectoryNamed    # Archive::Zip::Member
{
	my $class = shift;
	my $self  = $class->DIRECTORYMEMBERCLASS->_newNamed(@_);
	return $self;
}

sub new    # Archive::Zip::Member
{
	my $class = shift;
	my $self  = {
		'lastModFileDateTime'      => 0,
		'fileAttributeFormat'      => FA_UNIX,
		'versionMadeBy'            => 20,
		'versionNeededToExtract'   => 20,
		'bitFlag'                  => 0,
		'compressionMethod'        => COMPRESSION_STORED,
		'desiredCompressionMethod' => COMPRESSION_STORED,
		'desiredCompressionLevel'  => COMPRESSION_LEVEL_NONE,
		'internalFileAttributes'   => 0,
		'externalFileAttributes'   => 0,                        # set later
		'fileName'                 => '',
		'cdExtraField'             => '',
		'localExtraField'          => '',
		'fileComment'              => '',
		'crc32'                    => 0,
		'compressedSize'           => 0,
		'uncompressedSize'         => 0,
		@_
	};
	bless( $self, $class );
	$self->unixFileAttributes( $self->DEFAULT_FILE_PERMISSIONS );
	return $self;
}

sub _becomeDirectoryIfNecessary    # Archive::Zip::Member
{
	my $self = shift;
	$self->_become(DIRECTORYMEMBERCLASS)
	  if $self->isDirectory();
	return $self;
}

# Morph into given class (do whatever cleanup I need to do)
sub _become    # Archive::Zip::Member
{
	return bless( $_[0], $_[1] );
}

sub versionMadeBy    # Archive::Zip::Member
{
	shift->{'versionMadeBy'};
}

sub fileAttributeFormat    # Archive::Zip::Member
{
	( $#_ > 0 ) 
	  ? ( $_[0]->{'fileAttributeFormat'} = $_[1] )
	  : $_[0]->{'fileAttributeFormat'};
}

sub versionNeededToExtract    # Archive::Zip::Member
{
	shift->{'versionNeededToExtract'};
}

sub bitFlag    # Archive::Zip::Member
{
	shift->{'bitFlag'};
}

sub compressionMethod    # Archive::Zip::Member
{
	shift->{'compressionMethod'};
}

sub desiredCompressionMethod    # Archive::Zip::Member
{
	my $self                        = shift;
	my $newDesiredCompressionMethod = shift;
	my $oldDesiredCompressionMethod = $self->{'desiredCompressionMethod'};
	if ( defined($newDesiredCompressionMethod) )
	{
		$self->{'desiredCompressionMethod'} = $newDesiredCompressionMethod;
		if ( $newDesiredCompressionMethod == COMPRESSION_STORED )
		{
			$self->{'desiredCompressionLevel'} = 0;
		}
		elsif ( $oldDesiredCompressionMethod == COMPRESSION_STORED )
		{
			$self->{'desiredCompressionLevel'} = COMPRESSION_LEVEL_DEFAULT;
		}
	}
	return $oldDesiredCompressionMethod;
}

sub desiredCompressionLevel    # Archive::Zip::Member
{
	my $self                       = shift;
	my $newDesiredCompressionLevel = shift;
	my $oldDesiredCompressionLevel = $self->{'desiredCompressionLevel'};
	if ( defined($newDesiredCompressionLevel) )
	{
		$self->{'desiredCompressionLevel'}  = $newDesiredCompressionLevel;
		$self->{'desiredCompressionMethod'} =
		  ( $newDesiredCompressionLevel 
		  ? COMPRESSION_DEFLATED
		  : COMPRESSION_STORED );
	}
	return $oldDesiredCompressionLevel;
}

sub fileName    # Archive::Zip::Member
{
	my $self    = shift;
	my $newName = shift;
	if ($newName)
	{
		$newName =~ s{[\\/]+}{/}g;    # deal with dos/windoze problems
		$self->{'fileName'} = $newName;
	}
	return $self->{'fileName'};
}

sub lastModFileDateTime    # Archive::Zip::Member
{
	my $modTime = shift->{'lastModFileDateTime'};
	$modTime =~ m/^(\d+)$/;    # untaint
	return $1;
}

sub lastModTime    # Archive::Zip::Member
{
	my $self = shift;
	return _dosToUnixTime( $self->lastModFileDateTime() );
}

sub setLastModFileDateTimeFromUnix    # Archive::Zip::Member
{
	my $self   = shift;
	my $time_t = shift;
	$self->{'lastModFileDateTime'} = _unixToDosTime($time_t);
}

# DOS date/time format
# 0-4 (5) Second divided by 2
# 5-10 (6) Minute (0-59)
# 11-15 (5) Hour (0-23 on a 24-hour clock)
# 16-20 (5) Day of the month (1-31)
# 21-24 (4) Month (1 = January, 2 = February, etc.)
# 25-31 (7) Year offset from 1980 (add 1980 to get actual year)

# Convert DOS date/time format to unix time_t format
# NOT AN OBJECT METHOD!
sub _dosToUnixTime    # Archive::Zip::Member
{
	my $dt = shift;
	return time() unless defined($dt);

	my $year = ( ( $dt >> 25 ) & 0x7f ) + 80;
	my $mon  = ( ( $dt >> 21 ) & 0x0f ) - 1;
	my $mday = ( ( $dt >> 16 ) & 0x1f );

	my $hour = ( ( $dt >> 11 ) & 0x1f );
	my $min  = ( ( $dt >> 5 ) & 0x3f );
	my $sec  = ( ( $dt << 1 ) & 0x3e );

	# catch errors
	my $time_t =
	  eval { Time::Local::timelocal( $sec, $min, $hour, $mday, $mon, $year ); };
	return time() if ($@);
	return $time_t;
}

sub internalFileAttributes    # Archive::Zip::Member
{
	shift->{'internalFileAttributes'};
}

sub externalFileAttributes    # Archive::Zip::Member
{
	shift->{'externalFileAttributes'};
}

# Convert UNIX permissions into proper value for zip file
# NOT A METHOD!
sub _mapPermissionsFromUnix    # Archive::Zip::Member
{
	my $perms = shift;
	return $perms << 16;

	# TODO: map MS-DOS perms too (RHSA?)
}

# Convert ZIP permissions into Unix ones
#
# This was taken from Info-ZIP group's portable UnZip
# zipfile-extraction program, version 5.50.
# http://www.info-zip.org/pub/infozip/ 
#
# See the mapattr() function in unix/unix.c
# See the attribute format constants in unzpriv.h
#
# XXX Note that there's one situation that isn't implemented
# yet that depends on the "extra field."
sub _mapPermissionsToUnix    # Archive::Zip::Member
{
	my $self = shift;

	my $format  = $self->{'fileAttributeFormat'};
	my $attribs = $self->{'externalFileAttributes'};

	my $mode = 0;

	if ( $format == FA_AMIGA )
	{
		$attribs = $attribs >> 17 & 7;                         # Amiga RWE bits
		$mode    = $attribs << 6 | $attribs << 3 | $attribs;
		return $mode;
	}

	if ( $format == FA_THEOS )
	{
		$attribs &= 0xF1FFFFFF;
		if ( ( $attribs & 0xF0000000 ) != 0x40000000 )
		{
			$attribs &= 0x01FFFFFF;    # not a dir, mask all ftype bits
		}
		else
		{
			$attribs &= 0x41FFFFFF;    # leave directory bit as set
		}
	}

	if ( $format == FA_UNIX
		|| $format == FA_VAX_VMS
		|| $format == FA_ACORN
		|| $format == FA_ATARI_ST
		|| $format == FA_BEOS
		|| $format == FA_QDOS
		|| $format == FA_TANDEM )
	{
		$mode = $attribs >> 16;
		return $mode if $mode != 0 or not $self->localExtraField;

		# warn("local extra field is: ", $self->localExtraField, "\n");

		# XXX This condition is not implemented
		# I'm just including the comments from the info-zip section for now.

		# Some (non-Info-ZIP) implementations of Zip for Unix and
		# VMS (and probably others ??) leave 0 in the upper 16-bit
		# part of the external_file_attributes field. Instead, they
		# store file permission attributes in some extra field.
		# As a work-around, we search for the presence of one of
		# these extra fields and fall back to the MSDOS compatible
		# part of external_file_attributes if one of the known
		# e.f. types has been detected.
		# Later, we might implement extraction of the permission
		# bits from the VMS extra field. But for now, the work-around
		# should be sufficient to provide "readable" extracted files.
		# (For ASI Unix e.f., an experimental remap from the e.f.
		# mode value IS already provided!)
	}

	# PKWARE's PKZip for Unix marks entries as FA_MSDOS, but stores the
	# Unix attributes in the upper 16 bits of the external attributes
	# field, just like Info-ZIP's Zip for Unix.  We try to use that
	# value, after a check for consistency with the MSDOS attribute
	# bits (see below).
	if ( $format == FA_MSDOS )
	{
		$mode = $attribs >> 16;
	}

	# FA_MSDOS, FA_OS2_HPFS, FA_WINDOWS_NTFS, FA_MACINTOSH, FA_TOPS20
	$attribs = !( $attribs & 1 ) << 1 | ( $attribs & 0x10 ) >> 4;

	# keep previous $mode setting when its "owner"
	# part appears to be consistent with DOS attribute flags!
	return $mode if ( $mode & 0700 ) == ( 0400 | $attribs << 6 );
	$mode = 0444 | $attribs << 6 | $attribs << 3 | $attribs;
	return $mode;
}

sub unixFileAttributes    # Archive::Zip::Member
{
	my $self     = shift;
	my $oldPerms = $self->_mapPermissionsToUnix();
	if (@_)
	{
		my $perms = shift;
		if ( $self->isDirectory() )
		{
			$perms &= ~FILE_ATTRIB;
			$perms |= DIRECTORY_ATTRIB;
		}
		else
		{
			$perms &= ~DIRECTORY_ATTRIB;
			$perms |= FILE_ATTRIB;
		}
		$self->{'externalFileAttributes'} = _mapPermissionsFromUnix($perms);
	}
	return $oldPerms;
}

sub localExtraField    # Archive::Zip::Member
{
	( $#_ > 0 ) 
	  ? ( $_[0]->{'localExtraField'} = $_[1] )
	  : $_[0]->{'localExtraField'};
}

sub cdExtraField    # Archive::Zip::Member
{
	( $#_ > 0 ) ? ( $_[0]->{'cdExtraField'} = $_[1] ) : $_[0]->{'cdExtraField'};
}

sub extraFields    # Archive::Zip::Member
{
	my $self = shift;
	return $self->localExtraField() . $self->cdExtraField();
}

sub fileComment    # Archive::Zip::Member
{
	( $#_ > 0 ) 
	  ? ( $_[0]->{'fileComment'} = pack( 'C0a*', $_[1] ) )
	  : $_[0]->{'fileComment'};
}

sub hasDataDescriptor    # Archive::Zip::Member
{
	my $self = shift;
	if (@_)
	{
		my $shouldHave = shift;
		if ($shouldHave)
		{
			$self->{'bitFlag'} |= GPBF_HAS_DATA_DESCRIPTOR_MASK;
		}
		else
		{
			$self->{'bitFlag'} &= ~GPBF_HAS_DATA_DESCRIPTOR_MASK;
		}
	}
	return $self->{'bitFlag'} & GPBF_HAS_DATA_DESCRIPTOR_MASK;
}

sub crc32    # Archive::Zip::Member
{
	shift->{'crc32'};
}

sub crc32String    # Archive::Zip::Member
{
	sprintf( "%08x", shift->{'crc32'} );
}

sub compressedSize    # Archive::Zip::Member
{
	shift->{'compressedSize'};
}

sub uncompressedSize    # Archive::Zip::Member
{
	shift->{'uncompressedSize'};
}

sub isEncrypted    # Archive::Zip::Member
{
	shift->bitFlag() & GPBF_ENCRYPTED_MASK;
}

sub isTextFile    # Archive::Zip::Member
{
	my $self = shift;
	my $bit  = $self->internalFileAttributes() & IFA_TEXT_FILE_MASK;
	if (@_)
	{
		my $flag = shift;
		$self->{'internalFileAttributes'} &= ~IFA_TEXT_FILE_MASK;
		$self->{'internalFileAttributes'} |=
		  ( $flag ? IFA_TEXT_FILE: IFA_BINARY_FILE );
	}
	return $bit == IFA_TEXT_FILE;
}

sub isBinaryFile    # Archive::Zip::Member
{
	my $self = shift;
	my $bit  = $self->internalFileAttributes() & IFA_TEXT_FILE_MASK;
	if (@_)
	{
		my $flag = shift;
		$self->{'internalFileAttributes'} &= ~IFA_TEXT_FILE_MASK;
		$self->{'internalFileAttributes'} |=
		  ( $flag ? IFA_BINARY_FILE: IFA_TEXT_FILE );
	}
	return $bit == IFA_BINARY_FILE;
}

sub extractToFileNamed    # Archive::Zip::Member
{
	my $self = shift;
	my $name = shift;    # local FS name
	return _error("encryption unsupported") if $self->isEncrypted();
	mkpath( dirname($name) );    # croaks on error
	my ( $status, $fh ) = _newFileHandle( $name, 'w' );
	return _ioError("Can't open file $name for write") unless $status;
	my $retval = $self->extractToFileHandle($fh);
	$fh->close();
	utime( $self->lastModTime(), $self->lastModTime(), $name );
	return $retval;
}

sub isDirectory    # Archive::Zip::Member
{
	return 0;
}

sub externalFileName    # Archive::Zip::Member
{
	return undef;
}

# The following are used when copying data
sub _writeOffset    # Archive::Zip::Member
{
	shift->{'writeOffset'};
}

sub _readOffset    # Archive::Zip::Member
{
	shift->{'readOffset'};
}

sub writeLocalHeaderRelativeOffset    # Archive::Zip::Member
{
	shift->{'writeLocalHeaderRelativeOffset'};
}

sub wasWritten { shift->{'wasWritten'} }

sub _dataEnded    # Archive::Zip::Member
{
	shift->{'dataEnded'};
}

sub _readDataRemaining    # Archive::Zip::Member
{
	shift->{'readDataRemaining'};
}

sub _inflater    # Archive::Zip::Member
{
	shift->{'inflater'};
}

sub _deflater    # Archive::Zip::Member
{
	shift->{'deflater'};
}

# Return the total size of my local header
sub _localHeaderSize    # Archive::Zip::Member
{
	my $self = shift;
	return SIGNATURE_LENGTH + LOCAL_FILE_HEADER_LENGTH +
	  length( $self->fileName() ) + length( $self->localExtraField() );
}

# Return the total size of my CD header
sub _centralDirectoryHeaderSize    # Archive::Zip::Member
{
	my $self = shift;
	return SIGNATURE_LENGTH + CENTRAL_DIRECTORY_FILE_HEADER_LENGTH +
	  length( $self->fileName() ) + length( $self->cdExtraField() ) +
	  length( $self->fileComment() );
}

# convert a unix time to DOS date/time
# NOT AN OBJECT METHOD!
sub _unixToDosTime    # Archive::Zip::Member
{
	my $time_t = shift;
	my ( $sec, $min, $hour, $mday, $mon, $year ) = localtime($time_t);
	my $dt = 0;
	$dt += ( $sec >> 1 );
	$dt += ( $min << 5 );
	$dt += ( $hour << 11 );
	$dt += ( $mday << 16 );
	$dt += ( ( $mon + 1 ) << 21 );
	$dt += ( ( $year - 80 ) << 25 );
	return $dt;
}

# Write my local header to a file handle.
# Stores the offset to the start of the header in my
# writeLocalHeaderRelativeOffset member.
# Returns AZ_OK on success.
sub _writeLocalFileHeader    # Archive::Zip::Member
{
	my $self = shift;
	my $fh   = shift;

	my $signatureData = pack( SIGNATURE_FORMAT, LOCAL_FILE_HEADER_SIGNATURE );
	$fh->print($signatureData)
	  or return _ioError("writing local header signature");

	my $header = pack(
		LOCAL_FILE_HEADER_FORMAT,
		$self->versionNeededToExtract(),
		$self->bitFlag(),
		$self->desiredCompressionMethod(),
		$self->lastModFileDateTime(),
		$self->crc32(),
		$self->compressedSize(),    # may need to be re-written later
		$self->uncompressedSize(),
		length( $self->fileName() ),
		length( $self->localExtraField() )
	);

	$fh->print($header) or return _ioError("writing local header");
	if ( $self->fileName() )
	{
		$fh->print( $self->fileName() )
		  or return _ioError("writing local header filename");
	}
	if ( $self->localExtraField() )
	{
		$fh->print( $self->localExtraField() )
		  or return _ioError("writing local extra field");
	}

	return AZ_OK;
}

sub _writeCentralDirectoryFileHeader    # Archive::Zip::Member
{
	my $self = shift;
	my $fh   = shift;

	my $sigData =
	  pack( SIGNATURE_FORMAT, CENTRAL_DIRECTORY_FILE_HEADER_SIGNATURE );
	$fh->print($sigData)
	  or return _ioError("writing central directory header signature");

	my $fileNameLength    = length( $self->fileName() );
	my $extraFieldLength  = length( $self->cdExtraField() );
	my $fileCommentLength = length( $self->fileComment() );

	my $header = pack(
		CENTRAL_DIRECTORY_FILE_HEADER_FORMAT,
		$self->versionMadeBy(),
		$self->fileAttributeFormat(),
		$self->versionNeededToExtract(),
		$self->bitFlag(),
		$self->desiredCompressionMethod(),
		$self->lastModFileDateTime(),
		$self->crc32(),            # these three fields should have been updated
		$self->_writeOffset(),     # by writing the data stream out
		$self->uncompressedSize(), #
		$fileNameLength,
		$extraFieldLength,
		$fileCommentLength,
		0,                         # {'diskNumberStart'},
		$self->internalFileAttributes(),
		$self->externalFileAttributes(),
		$self->writeLocalHeaderRelativeOffset()
	);

	$fh->print($header)
	  or return _ioError("writing central directory header");
	if ($fileNameLength)
	{
		$fh->print( $self->fileName() )
		  or return _ioError("writing central directory header signature");
	}
	if ($extraFieldLength)
	{
		$fh->print( $self->cdExtraField() )
		  or return _ioError("writing central directory extra field");
	}
	if ($fileCommentLength)
	{
		$fh->print( $self->fileComment() )
		  or return _ioError("writing central directory file comment");
	}

	return AZ_OK;
}

# This writes a data descriptor to the given file handle.
# Assumes that crc32, writeOffset, and uncompressedSize are
# set correctly (they should be after a write).
# Further, the local file header should have the
# GPBF_HAS_DATA_DESCRIPTOR_MASK bit set.
sub _writeDataDescriptor    # Archive::Zip::Member
{
	my $self   = shift;
	my $fh     = shift;
	my $header = pack(
		SIGNATURE_FORMAT . DATA_DESCRIPTOR_FORMAT,
		DATA_DESCRIPTOR_SIGNATURE,
		$self->crc32(),
		$self->_writeOffset(),    # compressed size
		$self->uncompressedSize()
	);

	$fh->print($header)
	  or return _ioError("writing data descriptor");
	return AZ_OK;
}

# Re-writes the local file header with new crc32 and compressedSize fields.
# To be called after writing the data stream.
# Assumes that filename and extraField sizes didn't change since last written.
sub _refreshLocalFileHeader    # Archive::Zip::Member
{
	my $self = shift;
	my $fh   = shift;

	my $here = $fh->tell();
	$fh->seek( $self->writeLocalHeaderRelativeOffset() + SIGNATURE_LENGTH,
		IO::Seekable::SEEK_SET )
	  or return _ioError("seeking to rewrite local header");

	my $header = pack(
		LOCAL_FILE_HEADER_FORMAT,
		$self->versionNeededToExtract(),
		$self->bitFlag(),
		$self->desiredCompressionMethod(),
		$self->lastModFileDateTime(),
		$self->crc32(),
		$self->_writeOffset(),    # compressed size
		$self->uncompressedSize(),
		length( $self->fileName() ),
		length( $self->localExtraField() )
	);

	$fh->print($header)
	  or return _ioError("re-writing local header");
	$fh->seek( $here, IO::Seekable::SEEK_SET )
	  or return _ioError("seeking after rewrite of local header");

	return AZ_OK;
}

sub readChunk    # Archive::Zip::Member
{
	my ( $self, $chunkSize ) = @_;

	if ( $self->readIsDone() )
	{
		$self->endRead();
		my $dummy = '';
		return ( \$dummy, AZ_STREAM_END );
	}

	$chunkSize = $Archive::Zip::ChunkSize if not defined($chunkSize);
	$chunkSize = $self->_readDataRemaining()
	  if $chunkSize > $self->_readDataRemaining();

	my $buffer = '';
	my $outputRef;
	my ( $bytesRead, $status ) = $self->_readRawChunk( \$buffer, $chunkSize );
	return ( \$buffer, $status ) unless $status == AZ_OK;

	$self->{'readDataRemaining'} -= $bytesRead;
	$self->{'readOffset'} += $bytesRead;

	if ( $self->compressionMethod() == COMPRESSION_STORED )
	{
		$self->{'crc32'} = $self->computeCRC32( $buffer, $self->{'crc32'} );
	}

	( $outputRef, $status ) = &{ $self->{'chunkHandler'} } ( $self, \$buffer );
	$self->{'writeOffset'} += length($$outputRef);

	$self->endRead()
	  if $self->readIsDone();

	return ( $outputRef, $status );
}

# Read the next raw chunk of my data. Subclasses MUST implement.
#	my ( $bytesRead, $status) = $self->_readRawChunk( \$buffer, $chunkSize );
sub _readRawChunk    # Archive::Zip::Member
{
	my $self = shift;
	return $self->_subclassResponsibility();
}

# A place holder to catch rewindData errors if someone ignores
# the error code.
sub _noChunk    # Archive::Zip::Member
{
	my $self = shift;
	return ( \undef, _error("trying to copy chunk when init failed") );
}

# Basically a no-op so that I can have a consistent interface.
# ( $outputRef, $status) = $self->_copyChunk( \$buffer );
sub _copyChunk    # Archive::Zip::Member
{
	my ( $self, $dataRef ) = @_;
	return ( $dataRef, AZ_OK );
}

# ( $outputRef, $status) = $self->_deflateChunk( \$buffer );
sub _deflateChunk    # Archive::Zip::Member
{
	my ( $self, $buffer ) = @_;
	my ( $out,  $status ) = $self->_deflater()->deflate($buffer);

	if ( $self->_readDataRemaining() == 0 )
	{
		my $extraOutput;
		( $extraOutput, $status ) = $self->_deflater()->flush();
		$out .= $extraOutput;
		$self->endRead();
		return ( \$out, AZ_STREAM_END );
	}
	elsif ( $status == Z_OK )
	{
		return ( \$out, AZ_OK );
	}
	else
	{
		$self->endRead();
		my $retval = _error( 'deflate error', $status );
		my $dummy = '';
		return ( \$dummy, $retval );
	}
}

# ( $outputRef, $status) = $self->_inflateChunk( \$buffer );
sub _inflateChunk    # Archive::Zip::Member
{
	my ( $self, $buffer ) = @_;
	my ( $out,  $status ) = $self->_inflater()->inflate($buffer);
	my $retval;
	$self->endRead() unless $status == Z_OK;
	if ( $status == Z_OK || $status == Z_STREAM_END )
	{
		$retval = ( $status == Z_STREAM_END ) ? AZ_STREAM_END: AZ_OK;
		return ( \$out, $retval );
	}
	else
	{
		$retval = _error( 'inflate error', $status );
		my $dummy = '';
		return ( \$dummy, $retval );
	}
}

sub rewindData    # Archive::Zip::Member
{
	my $self = shift;
	my $status;

	# set to trap init errors
	$self->{'chunkHandler'} = $self->can('_noChunk');

	# Work around WinZip bug with 0-length DEFLATED files
	$self->desiredCompressionMethod(COMPRESSION_STORED)
	  if $self->uncompressedSize() == 0;

	# assume that we're going to read the whole file, and compute the CRC anew.
	$self->{'crc32'} = 0
	  if ( $self->compressionMethod() == COMPRESSION_STORED );

	# These are the only combinations of methods we deal with right now.
	if ( $self->compressionMethod() == COMPRESSION_STORED
		and $self->desiredCompressionMethod() == COMPRESSION_DEFLATED )
	{
		( $self->{'deflater'}, $status ) = Compress::Zlib::deflateInit(
			'-Level'      => $self->desiredCompressionLevel(),
			'-WindowBits' => -MAX_WBITS(),                     # necessary magic
			'-Bufsize'    => $Archive::Zip::ChunkSize,
			@_
		);    # pass additional options
		return _error( 'deflateInit error:', $status )
		  unless $status == Z_OK;
		$self->{'chunkHandler'} = $self->can('_deflateChunk');
	}
	elsif ( $self->compressionMethod() == COMPRESSION_DEFLATED
		and $self->desiredCompressionMethod() == COMPRESSION_STORED )
	{
		( $self->{'inflater'}, $status ) = Compress::Zlib::inflateInit(
			'-WindowBits' => -MAX_WBITS(),               # necessary magic
			'-Bufsize'    => $Archive::Zip::ChunkSize,
			@_
		);    # pass additional options
		return _error( 'inflateInit error:', $status )
		  unless $status == Z_OK;
		$self->{'chunkHandler'} = $self->can('_inflateChunk');
	}
	elsif ( $self->compressionMethod() == $self->desiredCompressionMethod() )
	{
		$self->{'chunkHandler'} = $self->can('_copyChunk');
	}
	else
	{
		return _error(
			sprintf(
				"Unsupported compression combination: read %d, write %d",
				$self->compressionMethod(),
				$self->desiredCompressionMethod()
			)
		);
	}

	$self->{'readDataRemaining'} =
	  ( $self->compressionMethod() == COMPRESSION_STORED )
	  ? $self->uncompressedSize()
	  : $self->compressedSize();
	$self->{'dataEnded'}  = 0;
	$self->{'readOffset'} = 0;

	return AZ_OK;
}

sub endRead    # Archive::Zip::Member
{
	my $self = shift;
	delete $self->{'inflater'};
	delete $self->{'deflater'};
	$self->{'dataEnded'}         = 1;
	$self->{'readDataRemaining'} = 0;
	return AZ_OK;
}

sub readIsDone    # Archive::Zip::Member
{
	my $self = shift;
	return ( $self->_dataEnded() or !$self->_readDataRemaining() );
}

sub contents    # Archive::Zip::Member
{
	my $self        = shift;
	my $newContents = shift;

	if ( defined($newContents) )
	{

		# change our type and call the subclass contents method.
		$self->_become(STRINGMEMBERCLASS);
		return $self->contents( pack( 'C0a*', $newContents ) )
		  ;    # in case of Unicode
	}
	else
	{
		my $oldCompression =
		  $self->desiredCompressionMethod(COMPRESSION_STORED);
		my $status = $self->rewindData(@_);
		if ( $status != AZ_OK )
		{
			$self->endRead();
			return $status;
		}
		my $retval = '';
		while ( $status == AZ_OK )
		{
			my $ref;
			( $ref, $status ) = $self->readChunk( $self->_readDataRemaining() );

			# did we get it in one chunk?
			if ( length($$ref) == $self->uncompressedSize() )
			{
				$retval = $$ref;
			}
			else { $retval .= $$ref }
		}
		$self->desiredCompressionMethod($oldCompression);
		$self->endRead();
		$status = AZ_OK if $status == AZ_STREAM_END;
		$retval = undef unless $status == AZ_OK;
		return wantarray ? ( $retval, $status ) : $retval;
	}
}

sub extractToFileHandle    # Archive::Zip::Member
{
	my $self = shift;
	return _error("encryption unsupported") if $self->isEncrypted();
	my $fh = shift;
	_binmode($fh);
	my $oldCompression = $self->desiredCompressionMethod(COMPRESSION_STORED);
	my $status         = $self->rewindData(@_);
	$status = $self->_writeData($fh) if $status == AZ_OK;
	$self->desiredCompressionMethod($oldCompression);
	$self->endRead();
	return $status;
}

# write local header and data stream to file handle
sub _writeToFileHandle    # Archive::Zip::Member
{
	my $self         = shift;
	my $fh           = shift;
	my $fhIsSeekable = shift;
	my $offset       = shift;

	return _error("no member name given for $self")
	  unless $self->fileName();

	$self->{'writeLocalHeaderRelativeOffset'} = $offset;
	$self->{'wasWritten'}                     = 0;

	# Determine if I need to write a data descriptor
	# I need to do this if I can't refresh the header
	# and I don't know compressed size or crc32 fields.
	my $headerFieldsUnknown =
	  ( ( $self->uncompressedSize() > 0 )
	  and ( $self->compressionMethod() == COMPRESSION_STORED
		  or $self->desiredCompressionMethod() == COMPRESSION_DEFLATED ) );

	my $shouldWriteDataDescriptor =
	  ( $headerFieldsUnknown and not $fhIsSeekable );

	$self->hasDataDescriptor(1)
	  if ($shouldWriteDataDescriptor);

	$self->{'writeOffset'} = 0;

	my $status = $self->rewindData();
	( $status = $self->_writeLocalFileHeader($fh) )
	  if $status == AZ_OK;
	( $status = $self->_writeData($fh) )
	  if $status == AZ_OK;
	if ( $status == AZ_OK )
	{
		$self->{'wasWritten'} = 1;
		if ( $self->hasDataDescriptor() )
		{
			$status = $self->_writeDataDescriptor($fh);
		}
		elsif ($headerFieldsUnknown)
		{
			$status = $self->_refreshLocalFileHeader($fh);
		}
	}

	return $status;
}

# Copy my (possibly compressed) data to given file handle.
# Returns C<AZ_OK> on success
sub _writeData    # Archive::Zip::Member
{
	my $self    = shift;
	my $writeFh = shift;

	return AZ_OK if ( $self->uncompressedSize() == 0 );
	my $status;
	my $chunkSize = $Archive::Zip::ChunkSize;
	while ( $self->_readDataRemaining() > 0 )
	{
		my $outRef;
		( $outRef, $status ) = $self->readChunk($chunkSize);
		return $status if ( $status != AZ_OK and $status != AZ_STREAM_END );

		if ( length($$outRef) > 0 )
		{
			$writeFh->print($$outRef)
			  or return _ioError("write error during copy");
		}

		last if $status == AZ_STREAM_END;
	}
	$self->{'compressedSize'} = $self->_writeOffset();
	return AZ_OK;
}

# Return true if I depend on the named file
sub _usesFileNamed
{
	return 0;
}

# ----------------------------------------------------------------------
# class Archive::Zip::DirectoryMember
# ----------------------------------------------------------------------

package Archive::Zip::DirectoryMember;
use File::Path;

use vars qw( @ISA );
@ISA = qw ( Archive::Zip::Member );
BEGIN { use Archive::Zip qw( :ERROR_CODES :UTILITY_METHODS ) }

sub _newNamed    # Archive::Zip::DirectoryMember
{
	my $class    = shift;
	my $fileName = shift;    # FS name
	my $newName  = shift;    # Zip name
	$newName = _asZipDirName($fileName) unless $newName;
	my $self = $class->new(@_);
	$self->{'externalFileName'} = $fileName;
	$self->fileName($newName);
	if ( -e $fileName )
	{

		if ( -d _ )
		{
			my @stat = stat(_);
			$self->unixFileAttributes( $stat[2] );
			$self->setLastModFileDateTimeFromUnix( $stat[9] );
		}
		else    # hmm.. trying to add a non-directory?
		{
			_error( $fileName, ' exists but is not a directory' );
			return undef;
		}
	}
	else
	{
		$self->unixFileAttributes( $self->DEFAULT_DIRECTORY_PERMISSIONS );
		$self->setLastModFileDateTimeFromUnix( time() );
	}
	return $self;
}

sub externalFileName    # Archive::Zip::DirectoryMember
{
	shift->{'externalFileName'};
}

sub isDirectory    # Archive::Zip::DirectoryMember
{
	return 1;
}

sub extractToFileNamed    # Archive::Zip::DirectoryMember
{
	my $self    = shift;
	my $name    = shift;                                 # local FS name
	my $attribs = $self->unixFileAttributes() & 07777;
	mkpath( $name, 0, $attribs );                        # croaks on error
	utime( $self->lastModTime(), $self->lastModTime(), $name );
	return AZ_OK;
}

sub fileName    # Archive::Zip::DirectoryMember
{
	my $self    = shift;
	my $newName = shift;
	$newName =~ s{/?$}{/} if defined($newName);
	return $self->SUPER::fileName($newName);
}

# So people don't get too confused. This way it looks like the problem
# is in their code...
sub contents
{
	undef;
}

# ----------------------------------------------------------------------
# class Archive::Zip::FileMember
# Base class for classes that have file handles
# to external files
# ----------------------------------------------------------------------

package Archive::Zip::FileMember;
use vars qw( @ISA );
@ISA = qw ( Archive::Zip::Member );
BEGIN { use Archive::Zip qw( :UTILITY_METHODS ) }

sub externalFileName    # Archive::Zip::FileMember
{
	shift->{'externalFileName'};
}

# Return true if I depend on the named file
sub _usesFileNamed    # Archive::Zip::FileMember
{
	my $self     = shift;
	my $fileName = shift;
	my $xfn      = $self->externalFileName();
	return undef if ref($xfn);
	return $xfn eq $fileName;
}

sub fh    # Archive::Zip::FileMember
{
	my $self = shift;
	$self->_openFile()
	  if !defined( $self->{'fh'} ) || !$self->{'fh'}->opened();
	return $self->{'fh'};
}

# opens my file handle from my file name
sub _openFile    # Archive::Zip::FileMember
{
	my $self = shift;
	my ( $status, $fh ) = _newFileHandle( $self->externalFileName(), 'r' );
	if ( !$status )
	{
		_ioError( "Can't open", $self->externalFileName() );
		return undef;
	}
	$self->{'fh'} = $fh;
	_binmode($fh);
	return $fh;
}

# Make sure I close my file handle
sub endRead    # Archive::Zip::FileMember
{
	my $self = shift;
	undef $self->{'fh'};    # _closeFile();
	return $self->SUPER::endRead(@_);
}

sub _become    # Archive::Zip::FileMember
{
	my $self     = shift;
	my $newClass = shift;
	return $self if ref($self) eq $newClass;
	delete( $self->{'externalFileName'} );
	delete( $self->{'fh'} );
	return $self->SUPER::_become($newClass);
}

# ----------------------------------------------------------------------
# class Archive::Zip::NewFileMember
# Used when adding a pre-existing file to an archive
# ----------------------------------------------------------------------

package Archive::Zip::NewFileMember;
use vars qw( @ISA );
@ISA = qw ( Archive::Zip::FileMember );

BEGIN { use Archive::Zip qw( :CONSTANTS :ERROR_CODES :UTILITY_METHODS ) }

# Given a file name, set up for eventual writing.
sub _newFromFileNamed    # Archive::Zip::NewFileMember
{
	my $class    = shift;
	my $fileName = shift;    # local FS format
	my $newName  = shift;
	$newName = _asZipDirName($fileName) unless defined($newName);
	return undef unless ( stat($fileName) && -r _ && !-d _ );
	my $self = $class->new(@_);
	$self->fileName($newName);
	$self->{'externalFileName'}  = $fileName;
	$self->{'compressionMethod'} = COMPRESSION_STORED;
	my @stat = stat(_);
	$self->{'compressedSize'} = $self->{'uncompressedSize'} = $stat[7];
	$self->desiredCompressionMethod( ( $self->compressedSize() > 0 ) 
		? COMPRESSION_DEFLATED
		: COMPRESSION_STORED );
	$self->unixFileAttributes( $stat[2] );
	$self->setLastModFileDateTimeFromUnix( $stat[9] );
	$self->isTextFile( -T _ );
	return $self;
}

sub rewindData    # Archive::Zip::NewFileMember
{
	my $self = shift;

	my $status = $self->SUPER::rewindData(@_);
	return $status unless $status == AZ_OK;

	return AZ_IO_ERROR unless $self->fh();
	$self->fh()->clearerr();
	$self->fh()->seek( 0, IO::Seekable::SEEK_SET )
	  or return _ioError( "rewinding", $self->externalFileName() );
	return AZ_OK;
}

# Return bytes read. Note that first parameter is a ref to a buffer.
# my $data;
# my ( $bytesRead, $status) = $self->readRawChunk( \$data, $chunkSize );
sub _readRawChunk    # Archive::Zip::NewFileMember
{
	my ( $self, $dataRef, $chunkSize ) = @_;
	return ( 0, AZ_OK ) unless $chunkSize;
	my $bytesRead = $self->fh()->read( $$dataRef, $chunkSize )
	  or return ( 0, _ioError("reading data") );
	return ( $bytesRead, AZ_OK );
}

# If I already exist, extraction is a no-op.
sub extractToFileNamed    # Archive::Zip::NewFileMember
{
	my $self = shift;
	my $name = shift;    # local FS name
	if ( File::Spec->rel2abs($name) eq
		File::Spec->rel2abs( $self->externalFileName() ) and -r $name )
	{
		return AZ_OK;
	}
	else
	{
		return $self->SUPER::extractToFileNamed( $name, @_ );
	}
}

# ----------------------------------------------------------------------
# class Archive::Zip::ZipFileMember
# This represents a member in an existing zip file on disk.
# ----------------------------------------------------------------------

package Archive::Zip::ZipFileMember;
use vars qw( @ISA );
@ISA = qw ( Archive::Zip::FileMember );

BEGIN
{
	use Archive::Zip qw( :CONSTANTS :ERROR_CODES :PKZIP_CONSTANTS
	  :UTILITY_METHODS );
}

# Create a new Archive::Zip::ZipFileMember
# given a filename and optional open file handle
# 
sub _newFromZipFile    # Archive::Zip::ZipFileMember
{
	my $class              = shift;
	my $fh                 = shift;
	my $externalFileName   = shift;
	my $possibleEocdOffset = shift;    # normally 0

	my $self = $class->new(
		'crc32'                     => 0,
		'diskNumberStart'           => 0,
		'localHeaderRelativeOffset' => 0,
		'dataOffset' => 0,    # localHeaderRelativeOffset + header length
		@_
	);
	$self->{'externalFileName'}   = $externalFileName;
	$self->{'fh'}                 = $fh;
	$self->{'possibleEocdOffset'} = $possibleEocdOffset;
	return $self;
}

sub isDirectory    # Archive::Zip::ZipFileMember
{
	my $self = shift;
	return ( substr( $self->fileName(), -1, 1 ) eq '/'
		and $self->uncompressedSize() == 0 );
}

# Seek to the beginning of the local header, just past the signature.
# Verify that the local header signature is in fact correct.
# Update the localHeaderRelativeOffset if necessary by adding the possibleEocdOffset.
# Returns status.

sub _seekToLocalHeader    # Archive::Zip::ZipFileMember
{
	my $self          = shift;
	my $where         = shift;    # optional
	my $previousWhere = shift;    # optional

	$where = $self->localHeaderRelativeOffset() unless defined($where);

	# avoid loop on certain corrupt files (from Julian Field)
	return _formatError("corrupt zip file")
	  if defined($previousWhere) && $where == $previousWhere;

	my $status;
	my $signature;

	$status = $self->fh()->seek( $where, IO::Seekable::SEEK_SET );
	return _ioError("seeking to local header") unless $status;

	( $status, $signature ) =
	  _readSignature( $self->fh(), $self->externalFileName(),
		LOCAL_FILE_HEADER_SIGNATURE );
	return $status if $status == AZ_IO_ERROR;

	# retry with EOCD offset if any was given.
	if ( $status == AZ_FORMAT_ERROR && $self->{'possibleEocdOffset'} )
	{
		$status =
		  $self->_seekToLocalHeader(
			$self->localHeaderRelativeOffset() + $self->{'possibleEocdOffset'},
			$where );
		if ( $status == AZ_OK )
		{
			$self->{'localHeaderRelativeOffset'} +=
			  $self->{'possibleEocdOffset'};
			$self->{'possibleEocdOffset'} = 0;
		}
	}

	return $status;
}

# Because I'm going to delete the file handle, read the local file
# header if the file handle is seekable. If it isn't, I assume that
# I've already read the local header.
# Return ( $status, $self )

sub _become    # Archive::Zip::ZipFileMember
{
	my $self     = shift;
	my $newClass = shift;
	return $self if ref($self) eq $newClass;

	my $status = AZ_OK;

	if ( _isSeekable( $self->fh() ) )
	{
		my $here = $self->fh()->tell();
		$status = $self->_seekToLocalHeader();
		$status = $self->_readLocalFileHeader() if $status == AZ_OK;
		$self->fh()->seek( $here, IO::Seekable::SEEK_SET );
		return $status unless $status == AZ_OK;
	}

	delete( $self->{'eocdCrc32'} );
	delete( $self->{'diskNumberStart'} );
	delete( $self->{'localHeaderRelativeOffset'} );
	delete( $self->{'dataOffset'} );

	return $self->SUPER::_become($newClass);
}

sub diskNumberStart    # Archive::Zip::ZipFileMember
{
	shift->{'diskNumberStart'};
}

sub localHeaderRelativeOffset    # Archive::Zip::ZipFileMember
{
	shift->{'localHeaderRelativeOffset'};
}

sub dataOffset    # Archive::Zip::ZipFileMember
{
	shift->{'dataOffset'};
}

# Skip local file header, updating only extra field stuff.
# Assumes that fh is positioned before signature.
sub _skipLocalFileHeader    # Archive::Zip::ZipFileMember
{
	my $self = shift;
	my $header;
	my $bytesRead = $self->fh()->read( $header, LOCAL_FILE_HEADER_LENGTH );
	if ( $bytesRead != LOCAL_FILE_HEADER_LENGTH )
	{
		return _ioError("reading local file header");
	}
	my $fileNameLength;
	my $extraFieldLength;
	my $bitFlag;
	( undef,    # $self->{'versionNeededToExtract'},
	  $bitFlag,
	  undef,    # $self->{'compressionMethod'},
	  undef,    # $self->{'lastModFileDateTime'},
	  undef,    # $crc32,
	  undef,    # $compressedSize,
	  undef,    # $uncompressedSize,
	  $fileNameLength,
	  $extraFieldLength )
	  = unpack( LOCAL_FILE_HEADER_FORMAT, $header );

	if ($fileNameLength)
	{
		$self->fh()->seek( $fileNameLength, IO::Seekable::SEEK_CUR )
		  or return _ioError("skipping local file name");
	}

	if ($extraFieldLength)
	{
		$bytesRead =
		  $self->fh()->read( $self->{'localExtraField'}, $extraFieldLength );
		if ( $bytesRead != $extraFieldLength )
		{
			return _ioError("reading local extra field");
		}
	}

	$self->{'dataOffset'} = $self->fh()->tell();

	if ( $bitFlag & GPBF_HAS_DATA_DESCRIPTOR_MASK )
	{

		# Read the crc32, compressedSize, and uncompressedSize from the
		# extended data descriptor, which directly follows the compressed data.
		#
		# Skip over the compressed file data (assumes that EOCD compressedSize
		# was correct)
		$self->fh()->seek( $self->{'compressedSize'}, IO::Seekable::SEEK_CUR )
		  or return _ioError("seeking to extended local header");

		# these values should be set correctly from before.
		my $oldCrc32            = $self->{'eocdCrc32'};
		my $oldCompressedSize   = $self->{'compressedSize'};
		my $oldUncompressedSize = $self->{'uncompressedSize'};

		my $status = $self->_readDataDescriptor();
		return $status unless $status == AZ_OK;

		return _formatError(
			"CRC or size mismatch while skipping data descriptor")
		  if ( $oldCrc32 != $self->{'crc32'}
			|| $oldUncompressedSize != $self->{'uncompressedSize'} );
	}

	return AZ_OK;
}

# Read from a local file header into myself. Returns AZ_OK if successful.
# Assumes that fh is positioned after signature.
# Note that crc32, compressedSize, and uncompressedSize will be 0 if
# GPBF_HAS_DATA_DESCRIPTOR_MASK is set in the bitFlag.

sub _readLocalFileHeader    # Archive::Zip::ZipFileMember
{
	my $self = shift;
	my $header;
	my $bytesRead = $self->fh()->read( $header, LOCAL_FILE_HEADER_LENGTH );
	if ( $bytesRead != LOCAL_FILE_HEADER_LENGTH )
	{
		return _ioError("reading local file header");
	}
	my $fileNameLength;
	my $crc32;
	my $compressedSize;
	my $uncompressedSize;
	my $extraFieldLength;
	( $self->{'versionNeededToExtract'}, $self->{'bitFlag'},
	       $self->{'compressionMethod'}, $self->{'lastModFileDateTime'},
	       $crc32,                       $compressedSize,
	       $uncompressedSize,            $fileNameLength,
	  $extraFieldLength )
	  = unpack( LOCAL_FILE_HEADER_FORMAT, $header );

	if ($fileNameLength)
	{
		my $fileName;
		$bytesRead = $self->fh()->read( $fileName, $fileNameLength );
		if ( $bytesRead != $fileNameLength )
		{
			return _ioError("reading local file name");
		}
		$self->fileName($fileName);
	}

	if ($extraFieldLength)
	{
		$bytesRead =
		  $self->fh()->read( $self->{'localExtraField'}, $extraFieldLength );
		if ( $bytesRead != $extraFieldLength )
		{
			return _ioError("reading local extra field");
		}
	}

	$self->{'dataOffset'} = $self->fh()->tell();

	if ( $self->hasDataDescriptor() )
	{

		# Read the crc32, compressedSize, and uncompressedSize from the
		# extended data descriptor.
		# Skip over the compressed file data (assumes that EOCD compressedSize
		# was correct)
		$self->fh()->seek( $self->{'compressedSize'}, IO::Seekable::SEEK_CUR )
		  or return _ioError("seeking to extended local header");

		my $status = $self->_readDataDescriptor();
		return $status unless $status == AZ_OK;
	}
	else
	{
		return _formatError(
			"CRC or size mismatch after reading data descriptor")
		  if ( $self->{'crc32'} != $crc32
			|| $self->{'uncompressedSize'} != $uncompressedSize );
	}

	return AZ_OK;
}

# This will read the data descriptor, which is after the end of compressed file
# data in members that that have GPBF_HAS_DATA_DESCRIPTOR_MASK set in their
# bitFlag.
# The only reliable way to find these is to rely on the EOCD compressedSize.
# Assumes that file is positioned immediately after the compressed data.
# Returns status; sets crc32, compressedSize, and uncompressedSize.
sub _readDataDescriptor
{
	my $self = shift;
	my $signatureData;
	my $header;
	my $crc32;
	my $compressedSize;
	my $uncompressedSize;

	my $bytesRead = $self->fh()->read( $signatureData, SIGNATURE_LENGTH );
	return _ioError("reading header signature")
	  if $bytesRead != SIGNATURE_LENGTH;
	my $signature = unpack( SIGNATURE_FORMAT, $signatureData );

	# unfortunately, the signature appears to be optional.
	if ( $signature == DATA_DESCRIPTOR_SIGNATURE
		&& ( $signature != $self->{'crc32'} ) )
	{
		$bytesRead = $self->fh()->read( $header, DATA_DESCRIPTOR_LENGTH );
		return _ioError("reading data descriptor")
		  if $bytesRead != DATA_DESCRIPTOR_LENGTH;

		( $crc32, $compressedSize, $uncompressedSize ) =
		  unpack( DATA_DESCRIPTOR_FORMAT, $header );
	}
	else
	{
		$bytesRead =
		  $self->fh()->read( $header, DATA_DESCRIPTOR_LENGTH_NO_SIG );
		return _ioError("reading data descriptor")
		  if $bytesRead != DATA_DESCRIPTOR_LENGTH_NO_SIG;

		$crc32 = $signature;
		( $compressedSize, $uncompressedSize ) =
		  unpack( DATA_DESCRIPTOR_FORMAT_NO_SIG, $header );
	}

	$self->{'eocdCrc32'} = $self->{'crc32'}
	  unless defined( $self->{'eocdCrc32'} );
	$self->{'crc32'}            = $crc32;
	$self->{'compressedSize'}   = $compressedSize;
	$self->{'uncompressedSize'} = $uncompressedSize;

	return AZ_OK;
}

# Read a Central Directory header. Return AZ_OK on success.
# Assumes that fh is positioned right after the signature.

sub _readCentralDirectoryFileHeader    # Archive::Zip::ZipFileMember
{
	my $self      = shift;
	my $fh        = $self->fh();
	my $header    = '';
	my $bytesRead = $fh->read( $header, CENTRAL_DIRECTORY_FILE_HEADER_LENGTH );
	if ( $bytesRead != CENTRAL_DIRECTORY_FILE_HEADER_LENGTH )
	{
		return _ioError("reading central dir header");
	}
	my ( $fileNameLength, $extraFieldLength, $fileCommentLength );
	(
		$self->{'versionMadeBy'},          $self->{'fileAttributeFormat'},
		$self->{'versionNeededToExtract'}, $self->{'bitFlag'},
		$self->{'compressionMethod'},      $self->{'lastModFileDateTime'},
		$self->{'crc32'},                  $self->{'compressedSize'},
		$self->{'uncompressedSize'},       $fileNameLength,
		$extraFieldLength,                 $fileCommentLength,
		$self->{'diskNumberStart'},        $self->{'internalFileAttributes'},
		$self->{'externalFileAttributes'}, $self->{'localHeaderRelativeOffset'}
	)
	  = unpack( CENTRAL_DIRECTORY_FILE_HEADER_FORMAT, $header );

	$self->{'eocdCrc32'} = $self->{'crc32'};

	if ($fileNameLength)
	{
		$bytesRead = $fh->read( $self->{'fileName'}, $fileNameLength );
		if ( $bytesRead != $fileNameLength )
		{
			_ioError("reading central dir filename");
		}
	}
	if ($extraFieldLength)
	{
		$bytesRead = $fh->read( $self->{'cdExtraField'}, $extraFieldLength );
		if ( $bytesRead != $extraFieldLength )
		{
			return _ioError("reading central dir extra field");
		}
	}
	if ($fileCommentLength)
	{
		$bytesRead = $fh->read( $self->{'fileComment'}, $fileCommentLength );
		if ( $bytesRead != $fileCommentLength )
		{
			return _ioError("reading central dir file comment");
		}
	}

	# NK 10/21/04: added to avoid problems with manipulated headers
	if (    $self->{'uncompressedSize'} != $self->{'compressedSize'}
		and $self->{'compressionMethod'} == COMPRESSION_STORED )
	{
		$self->{'uncompressedSize'} = $self->{'compressedSize'};
	}

	$self->desiredCompressionMethod( $self->compressionMethod() );

	return AZ_OK;
}

sub rewindData    # Archive::Zip::ZipFileMember
{
	my $self = shift;

	my $status = $self->SUPER::rewindData(@_);
	return $status unless $status == AZ_OK;

	return AZ_IO_ERROR unless $self->fh();

	$self->fh()->clearerr();

	# Seek to local file header.
	# The only reason that I'm doing this this way is that the extraField
	# length seems to be different between the CD header and the LF header.
	$status = $self->_seekToLocalHeader();
	return $status unless $status == AZ_OK;

	# skip local file header
	$status = $self->_skipLocalFileHeader();
	return $status unless $status == AZ_OK;

	# Seek to beginning of file data
	$self->fh()->seek( $self->dataOffset(), IO::Seekable::SEEK_SET )
	  or return _ioError("seeking to beginning of file data");

	return AZ_OK;
}

# Return bytes read. Note that first parameter is a ref to a buffer.
# my $data;
# my ( $bytesRead, $status) = $self->readRawChunk( \$data, $chunkSize );
sub _readRawChunk    # Archive::Zip::ZipFileMember
{
	my ( $self, $dataRef, $chunkSize ) = @_;
	return ( 0, AZ_OK ) unless $chunkSize;
	my $bytesRead = $self->fh()->read( $$dataRef, $chunkSize )
	  or return ( 0, _ioError("reading data") );
	return ( $bytesRead, AZ_OK );
}

# ----------------------------------------------------------------------
# class Archive::Zip::StringMember ( concrete )
# A Zip member whose data lives in a string
# ----------------------------------------------------------------------

package Archive::Zip::StringMember;
use vars qw( @ISA );
@ISA = qw ( Archive::Zip::Member );

BEGIN { use Archive::Zip qw( :CONSTANTS :ERROR_CODES ) }

# Create a new string member. Default is COMPRESSION_STORED.
# Can take a ref to a string as well.
sub _newFromString    # Archive::Zip::StringMember
{
	my $class  = shift;
	my $string = shift;
	my $name   = shift;
	my $self   = $class->new(@_);
	$self->contents($string);
	$self->fileName($name) if defined($name);

	# Set the file date to now
	$self->setLastModFileDateTimeFromUnix( time() );
	$self->unixFileAttributes( $self->DEFAULT_FILE_PERMISSIONS );
	return $self;
}

sub _become    # Archive::Zip::StringMember
{
	my $self     = shift;
	my $newClass = shift;
	return $self if ref($self) eq $newClass;
	delete( $self->{'contents'} );
	return $self->SUPER::_become($newClass);
}

# Get or set my contents. Note that we do not call the superclass
# version of this, because it calls us.
sub contents    # Archive::Zip::StringMember
{
	my $self   = shift;
	my $string = shift;
	if ( defined($string) )
	{
		$self->{'contents'} =
		  pack( 'C0a*', ( ref($string) eq 'SCALAR' ) ? $$string : $string );
		$self->{'uncompressedSize'} = $self->{'compressedSize'} =
		  length( $self->{'contents'} );
		$self->{'compressionMethod'} = COMPRESSION_STORED;
	}
	return $self->{'contents'};
}

# Return bytes read. Note that first parameter is a ref to a buffer.
# my $data;
# my ( $bytesRead, $status) = $self->readRawChunk( \$data, $chunkSize );
sub _readRawChunk    # Archive::Zip::StringMember
{
	my ( $self, $dataRef, $chunkSize ) = @_;
	$$dataRef = substr( $self->contents(), $self->_readOffset(), $chunkSize );
	return ( length($$dataRef), AZ_OK );
}

1;
__END__


# vim: ts=4 sw=4 tw=80 wrap
