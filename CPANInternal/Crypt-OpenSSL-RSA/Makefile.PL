use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    'NAME'	=> 'Crypt::OpenSSL::RSA',
    'DISTNAME' => 'Crypt-OpenSSL-RSA',
    'VERSION_FROM' => 'RSA.pm', # finds $VERSION
    'OBJECT' => 'RSA.o',
    'LIBS'	=> ['-lssl -lcrypto'],   # e.g., '-lm'
    'PREREQ_PM' => { 'Crypt::OpenSSL::Random' => 0 },
    'DEFINE'	=> '-DPERL5',   # perl-5.8/gcc-3.2 needs this
    'INC'	=> '',     # e.g., '-I/usr/include/other'
);
