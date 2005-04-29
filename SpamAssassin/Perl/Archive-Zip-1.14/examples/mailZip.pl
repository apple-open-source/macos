#!/usr/bin/perl -w
# Requires the following to be installed:
#  File::Path
#  File::Spec
#  IO::Scalar, ...         from the IO-stringy distribution
#  MIME::Base64
#  MIME::QuotedPrint
#  Net::SMTP
#  Mail::Internet, ...     from the MailTools distribution.
#  MIME::Tools

use strict;
use Archive::Zip qw(:CONSTANTS :ERROR_CODES);
use IO::Scalar;
use MIME::Entity;    # part of MIME::Tools package

my $zipContents = '';
my $SH          = IO::Scalar->new( \$zipContents );

my $zip = Archive::Zip->new();
my $member;

# add a string as a member:
my $stringMember = '<html><head></head><body><h1>Testing</h1></body></html>';
$member = $zip->addString($stringMember, 'whatever.html');
# $member->desiredCompressionMethod(COMPRESSION_STORED);

# write it to the scalar
my $status = $zip->writeToFileHandle($SH);
$SH->close;

print STDERR "zip is ". length($zipContents). " bytes long\n";

### Create an entity:
my $top = MIME::Entity->build(
    Type    => 'multipart/mixed',
    From    => 'ned@bike-nomad.com',
    To      => 'billnevin@tricom.net',
    Subject => "Your zip",
);

# attach the message
$top->attach(
    Encoding => '7bit',
    Data     => "here is the zip you ordered\n"
);

# attach the zip
$top->attach(
    Data     => \$zipContents,
    Type     => "application/x-zip",
    Encoding => "base64",
	Disposition => 'attachment',
	Filename => 'your.zip'
);

# attach this code
$top->attach(
	Encoding => '8bit',
	Type => 'text/plain',
	Path => $0,
	# Data => 'whatever',
	Disposition => 'inline'
);

# and print it out to stdout
$top->print( \*STDOUT );
