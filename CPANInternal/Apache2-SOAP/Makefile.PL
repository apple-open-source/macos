#!/usr/bin/perl -w
use strict;
use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

my $prereqs = {'SOAP::Lite' => 0,
               'mod_perl2' => 0,
              };
my %opts = (
            NAME              => 'Apache2::SOAP',
            VERSION_FROM      => 'lib/Apache2/SOAP.pm', # finds $VERSION
            dist   => {
                        SUFFIX   => 'gz',
                        COMPRESS => 'gzip -9f',
                    },
            PL_FILES => {},
            PREREQ_PM => $prereqs,
           );

my $eu_version = $ExtUtils::MakeMaker::VERSION;
if ($eu_version >= 5.43) {
  $opts{ABSTRACT} = q{mod_perl-2 SOAP server};
  $opts{AUTHOR} = 'Randy Kobes <r.kobes@uwinnipeg.ca>';
}
if ($eu_version > 6.11 ) {
  $opts{NO_META} = 1;
}

eval {require ModPerl::MM; };
if ($@) {
  warn qq{Please install ModPerl::MM first};
  exit 0;
}

require Apache::TestMM;
import Apache::TestMM qw(test clean);

Apache::TestMM::filter_args();

Apache::TestMM::generate_script('t/TEST'); 

ModPerl::MM::WriteMakefile(%opts); 
