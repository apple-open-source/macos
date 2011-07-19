use 5.006;
use strict;
use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile1(
    'NAME'		=> 'Test::Warn',
    'VERSION_FROM'	=> 'Warn.pm', # finds $VERSION
    'PREREQ_PM'		=> {
                            #'Array::Compare'        => 0,
                            #'Test::Exception'       => 0,
                            'Test::Builder'         => 0.13,
                            'Test::Builder::Tester' => 1.02,
                            'Sub::Uplevel'          => 0.12,
                            'Tree::DAG_Node'        => 0,
                            },
    'BUILD_REQUIRES'    => {
                            'File::Spec'            => 0,
                            'Test::More'            => 0,
                           },
    'LICENSE'		=> 'perl',
    'MIN_PERL_VERSION'  => 5.006,
    ABSTRACT_FROM => 'Warn.pm', # retrieve abstract from module
    AUTHOR        => 'Alexandr Ciornii <alexchorny'.'@gmail.com>',
    META_MERGE => {
      resources => {
        repository => 'http://github.com/chorny/test-warn/tree',
      },
      keywords => ['testing','warnings'],
    },
);

sub WriteMakefile1 {
        my %params=@_;
        my $eumm_version=$ExtUtils::MakeMaker::VERSION;
        $eumm_version=eval $eumm_version;
        die "EXTRA_META is deprecated" if exists $params{EXTRA_META};
        die "License not specified" if not exists $params{LICENSE};
        if ($params{BUILD_REQUIRES}) { #and $eumm_version < 6.5503
            #Should be modified in future when EUMM will
            #correctly support BUILD_REQUIRES.
            #EUMM 6.5502 has problems with BUILD_REQUIRES
            $params{PREREQ_PM}={ %{$params{PREREQ_PM} || {}} , %{$params{BUILD_REQUIRES}} };
            delete $params{BUILD_REQUIRES};
        }
        delete $params{CONFIGURE_REQUIRES} if $eumm_version < 6.52;
        delete $params{MIN_PERL_VERSION} if $eumm_version < 6.48;
        delete $params{META_MERGE} if $eumm_version < 6.46;
        delete $params{META_ADD} if $eumm_version < 6.46;
        delete $params{LICENSE} if $eumm_version < 6.31;
        delete $params{AUTHOR} if $] < 5.005;
        delete $params{ABSTRACT_FROM} if $] < 5.005;
        WriteMakefile(%params);
}
