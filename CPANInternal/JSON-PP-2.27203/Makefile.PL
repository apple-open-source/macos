require 5.00503;
use strict;
use ExtUtils::MakeMaker;

use lib qw( ./lib );

my %prereq = $] < 5.006 ? ( 'JSON::PP::Compat5005' => 0 )
           : $] < 5.008 ? ( 'JSON::PP::Compat5006' => 0 )
           : ()
           ;


WriteMakefile(
    'NAME'          => 'JSON::PP',
    'VERSION_FROM'  => 'lib/JSON/PP.pm', # finds $VERSION
    'PREREQ_PM'     => {
              'Test::More'  => 0,
              %prereq,
    },
    'EXE_FILES' => [ 'bin/json_pp' ],
    ($] >= 5.005 ?    ## Add these new keywords supported since 5.005
      (ABSTRACT_FROM => 'lib/JSON/PP.pm', # retrieve abstract from module
       AUTHOR     => 'Makamaka Hannyaharamitu, E<lt>makamaka[at]cpan.orgE<gt>') : ()),
    ( $ExtUtils::MakeMaker::VERSION >= 6.3002 ? ('LICENSE' => '', ) : () ),

    ( $ExtUtils::MakeMaker::VERSION >= 6.46 ? (
        'META_MERGE' => {
            resources => {
                repository  =>  'http://github.com/makamaka/JSON-PP',
            },
        } ) : ()
    ),
);


if ($] < 5.006) { # I saw to http://d.hatena.ne.jp/asakusabashi/20051231/p1 
    open(IN, "Makefile");
    open(OUT,">Makefile.tmp") || die;
    while(<IN>) {
        s/PERL_DL_NONLAZY=1//g;
        print OUT;
    }
    close(OUT);
    close(IN);
    rename("Makefile.tmp" => "Makefile");
}

