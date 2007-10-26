use ExtUtils::MakeMaker;

WriteMakefile(
    'NAME'	      => 'Class::Singleton',
    'VERSION_FROM'    => 'Singleton.pm', # finds $VERSION
    'dist'            => { 'COMPRESS' => 'gzip', 'SUFFIX' => 'gz' },
);
