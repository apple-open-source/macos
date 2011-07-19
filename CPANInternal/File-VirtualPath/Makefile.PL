use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
	NAME	=> 'File::VirtualPath',
	VERSION_FROM => 'lib/File/VirtualPath.pm', # finds $VERSION
	PREREQ_PM => {
	},
);
