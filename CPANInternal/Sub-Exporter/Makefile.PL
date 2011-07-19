use inc::Module::Install;

all_from      ('lib/Sub/Exporter.pm');

requires('Sub::Install'  => 0.92 ); # exporter, needed to actually export
requires('Data::OptList' => 0.100); # used to process options
requires('Params::Util'  => 0.14 ); # _CODELIKE

extra_tests;

repository('http://github.com/rjbs/sub-exporter');
auto_manifest;

WriteAll();
