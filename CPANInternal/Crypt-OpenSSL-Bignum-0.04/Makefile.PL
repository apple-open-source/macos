use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    'NAME'              => 'Crypt::OpenSSL::Bignum',
    'VERSION_FROM'      => 'Bignum.pm',
    'PREREQ_PM'         => {},
    'ABSTRACT_FROM'     => 'Bignum.pm', # retrieve abstract from module
    'AUTHOR'            => 'Ian Robertson <iroberts@cpan.org>',
    'LIBS'              => ['-lcrypto'],   # e.g., '-lm'
    'DEFINE'	=> '-DPERL5 -DOPENSSL_NO_KRB5',
    # perl-5.8/gcc-3.2 needs -DPERL5, and redhat9 likes -DOPENSSL_NO_KRB5
    'INC'               => '',
);
