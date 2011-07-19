use inc::Module::Install 0.85;

name      'Class-C3-Componentised';
all_from  'lib/Class/C3/Componentised.pm';
author    'Ash Berlin <ash@cpan.org>';


requires  'MRO::Compat';
requires  'Class::Inspector';
requires  'Carp';
requires  'Test::Exception';

# we don't actually need Class::C3. MRO::Compat loads it on 5.8. On 5.10 it
# isn't needed. However, some existing code relies on us loading Class::C3. We
# don't want to break it just yet. Therefore we depend directly on Class::C3 as
# well.

requires  'Class::C3' => '0.20';

build_requires 'FindBin';

WriteAll;
