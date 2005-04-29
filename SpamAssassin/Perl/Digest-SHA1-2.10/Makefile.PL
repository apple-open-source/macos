require 5.004;
use ExtUtils::MakeMaker;

WriteMakefile(
    'NAME'	   => 'Digest::SHA1',
    'VERSION_FROM' => 'SHA1.pm',
    'PREREQ_PM'    => {
	                'Digest::base' => '1.00',
                      },
    'dist'         => { COMPRESS => 'gzip -9f', SUFFIX => 'gz', },
);
