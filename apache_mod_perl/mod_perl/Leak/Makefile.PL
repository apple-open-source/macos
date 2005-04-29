use ExtUtils::MakeMaker;

WriteMakefile(
    NAME	=> "Apache::Leak",
    VERSION_FROM => "Leak.pm",
    LDDLFLAGS  => "$ENV{RC_CFLAGS} $Config{lddlflags}",
);

