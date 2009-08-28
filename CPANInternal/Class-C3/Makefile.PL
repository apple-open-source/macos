use inc::Module::Install;

name           'Class-C3';
all_from       'lib/Class/C3.pm';

# Class::C3 under 5.9.5+ has no deps
if($] < 5.009_005) {
    build_requires 'Test::More'    => '0.47';

    feature 'XS Speedups', 'Class::C3::XS' => '0.07';

    # Would like to disable these if they answer yes above too ...
    requires 'Algorithm::C3' => '0.06';
    requires 'Scalar::Util'  => '1.10';
}

auto_install;
WriteAll;
