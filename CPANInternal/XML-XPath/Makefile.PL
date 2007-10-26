use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

require 5.005;

WriteMakefile(
    'NAME'	=> 'XML::XPath',
    'VERSION_FROM' => 'XPath.pm', # finds $VERSION
    'AUTHOR' => 'Matt Sergeant, AxKit.com Ltd',
    'ABSTRACT_FROM' => 'XPath.pm',
    'PREREQ_PM' => {
		    'XML::Parser' => '2.23',
		    },
    'EXE_FILES' => [ 'examples/xpath' ],
);
