# Drop-in replacement to ExtUtils::MakeMaker
use 5.8.3;
use inc::Module::Install;

# Define metadata
name 'JSON-Any';
all_from 'lib/JSON/Any.pm';

requires 'Carp' => '0';

sub has_json () {
    @order = qw(XS JSON DWIW Syck);
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
  -default   => 0,
  'JSON::XS' => '0';

feature 'JSON::PC',
  -default   => 0,
  'JSON::PC' => '0';

feature 'JSON::DWIW',
  -default     => 0,
  'JSON::DWIW' => '0';

feature 'JSON::Syck',
  -default     => 0,
  'JSON::Syck' => '0';

# Specific dependencies
build_requires 'Test::More' => '0';
no_index 'directory'        => 'ex';

if ( $ENV{BUILD_STANDARD_TESTS} ) {
    use_test_base;
    use_standard_tests;
}

auto_install;
WriteAll;
