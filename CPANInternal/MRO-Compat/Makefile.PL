use inc::Module::Install 0.75;

name          'MRO-Compat';
all_from      'lib/MRO/Compat.pm';

requires      'perl'       => 5.006_000;
test_requires 'Test::More' => '0.47';

if($] < 5.009_005) {
    feature  'XS Speedups', 'Class::C3::XS' => '0.08';
    requires 'Class::C3' => '0.20';
}

# Rebuild README for maintainers
system("pod2text lib/MRO/Compat.pm >README") if -e 'MANIFEST.SKIP';

auto_provides;
WriteAll;
