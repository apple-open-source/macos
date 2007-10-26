package t::TestYAML;

BEGIN { $ENV{PERL_DL_NONLAZY} = 0 }

use strict;
use Test;
use YAML::Syck;

sub import {
    shift;
    @_ or return;
    plan @_;

    *::ok = *ok;
    *::is = *ok;
    *::Dump = *YAML::Syck::Dump;
    *::Load = *YAML::Syck::Load;
}

1;

#use Test::YAML 0.51 -Base;
#
#$Test::YAML::YAML = 'YAML::Syck';
#Test::YAML->load_yaml_pm;
