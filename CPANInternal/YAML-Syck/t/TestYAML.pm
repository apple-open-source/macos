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
    *::roundtrip = *roundtrip;
    *::Dump = *YAML::Syck::Dump;
    *::Load = *YAML::Syck::Load;
}

sub roundtrip {
    @_ = (YAML::Syck::Load(YAML::Syck::Dump($_[0])), $_[0]);
    goto &main::is;
}

1;

#use Test::YAML 0.51 -Base;
#
#$Test::YAML::YAML = 'YAML::Syck';
#Test::YAML->load_yaml_pm;
