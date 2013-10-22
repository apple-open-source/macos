use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    PREREQ_PM           => {
        Test::Simple => 0.45,
        },
    'NAME'	=> 'Heap',
    'VERSION_FROM' => 'lib/Heap.pm', # finds $VERSION
);
