use inc::Module::Install 0.75;

name           'Class-C3-XS';
all_from       'lib/Class/C3/XS.pm';
perl_version   5.006_000;
test_requires  'Test::More' => '0.47';

# Rebuild README for maintainers
if(-e 'MANIFEST.SKIP') {
    system("pod2text lib/Class/C3/XS.pm >README");
}

auto_provides;
WriteAll;
