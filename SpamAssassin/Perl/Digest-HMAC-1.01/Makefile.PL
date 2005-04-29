require 5.004;
use ExtUtils::MakeMaker;

WriteMakefile(
    'NAME'	   => 'Digest::HMAC',
    'VERSION_FROM' => 'lib/Digest/HMAC.pm',
    'PREREQ_PM'    => { 'Digest::MD5' => 2.00,
                        'Digest::SHA1' => 1.00,
                      },
    'dist'         => { COMPRESS => 'gzip -9f', SUFFIX => 'gz', },
);
