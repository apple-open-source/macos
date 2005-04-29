use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    'NAME'         => 'Convert::TNEF',
    'VERSION_FROM' => 'TNEF.pm', # finds $VERSION
    'PREREQ_PM'    => { 'MIME::Body' => 4.109 },
);
