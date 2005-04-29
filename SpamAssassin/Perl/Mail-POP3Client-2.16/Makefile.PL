use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile
  (
   'NAME'	=> 'Mail::POP3Client',
   'DISTNAME'   => "Mail-POP3Client",
   'VERSION_FROM' => 'POP3Client.pm', # finds $VERSION
   'dist' => {
	      COMPRESS => "gzip -9f",
	      SUFFIX => "gz",
	     }
  );
