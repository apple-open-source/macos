# Drop-in replacement to ExtUtils::MakeMaker
use 5.008;
use inc::Module::Install;

# Define metadata
name 'JSON-Any';
all_from 'lib/JSON/Any.pm';

requires 'Carp' => '0';

sub has_json () {
    our @order = qw(XS JSON DWIW);
    foreach my $testmod (@order) {
        $testmod = "JSON::$testmod" unless $testmod eq "JSON";
        eval "require $testmod";
        return 1 unless $@;
    }
    return 0;
}

unless (has_json) {
    requires 'JSON' => '2.02';
}
else {
    feature 'JSON',
      -default => 0,
      'JSON'   => '2.02';
}

feature 'JSON::XS',
  -default   => 1,
  'JSON::XS' => '2.3';

feature 'JSON::DWIW',
  -default     => 0,
  'JSON::DWIW' => '0';

feature 'JSON::Syck',
  -default     => 0,
  'JSON::Syck' => '0';

# Specific dependencies
build_requires 'Test::More' => '0.42';
no_index 'directory'        => 'ex';

if ($Module::Install::AUTHOR) {
    system("pod2text lib/JSON/Any.pm > README")
        and die;
}

auto_set_repository;
auto_manifest;
auto_install;
WriteAll;
