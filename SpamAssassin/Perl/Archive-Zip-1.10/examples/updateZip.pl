# Shows how to update a Zip in place using a temp file.
# $Revision: 1.1 $
#
use Archive::Zip qw(:ERROR_CODES);
use File::Copy();

my $zipName = shift || die 'must provide a zip name';
my @fileNames = @ARGV;
die 'must provide file names' unless scalar(@fileNames);

# Read the zip
my $zip = Archive::Zip->new();
die "can't read $zipName\n" unless $zip->read($zipName) == AZ_OK;

# Update the zip
foreach my $file (@fileNames)
{
	$zip->removeMember($file);
	if ( -r $file )
	{
		if ( -f $file )
		{
			$zip->addFile($file) or die "Can't add $file to zip!\n";
		}
		elsif ( -d $file )
		{
			$zip->addDirectory($file) or die "Can't add $file to zip!\n";
		}
		else
		{
			warn "Don't know how to add $file\n";
		}
	}
	else
	{
		warn "Can't read $file\n";
	}
}

# Now the zip is updated. Write it back via a temp file.

exit( $zip->overwrite() );
