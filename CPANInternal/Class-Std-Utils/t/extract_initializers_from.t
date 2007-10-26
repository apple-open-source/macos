use Test::More 'no_plan';

my %hash = (
    top_1 => 'def val top 1',
    top_2 => 'def val top 2',
    top_3 => 'def val top 3',

    Classname_1 => {
        top_2 => 'class 1 val top 2',
    },

    Classname_2 => {
        top_1 => 'class 2 val top 1',
        top_2 => 'class 2 val top 2',
        top_3 => 'class 2 val top 3',
    },

    Bad => 'class',
);

my %orig_hash = %hash;

my %expect_1 = (
    top_1 => 'def val top 1',
    top_2 => 'class 1 val top 2',
    top_3 => 'def val top 3',

    Classname_1 => {
        top_2 => 'class 1 val top 2',
    },

    Classname_2 => {
        top_1 => 'class 2 val top 1',
        top_2 => 'class 2 val top 2',
        top_3 => 'class 2 val top 3',
    },

    Bad => 'class',
);

my %expect_2 = (
    top_1 => 'class 2 val top 1',
    top_2 => 'class 2 val top 2',
    top_3 => 'class 2 val top 3',

    Classname_1 => {
        top_2 => 'class 1 val top 2',
    },

    Classname_2 => {
        top_1 => 'class 2 val top 1',
        top_2 => 'class 2 val top 2',
        top_3 => 'class 2 val top 3',
    },

    Bad => 'class',
);

package Classname_1;
use Class::Std::Utils;

my %args1 = extract_initializers_from(\%hash);

::is_deeply \%hash, \%orig_hash        => 'Original args unchanged in class 1';
::is_deeply \%args1, \%expect_1        => 'Extracted as expected in class 1';


package Classname_2;
use Class::Std::Utils;

my %args2 = extract_initializers_from(\%hash);

::is_deeply \%hash, \%orig_hash        => 'Original args unchanged in class 2';
::is_deeply \%args2, \%expect_2        => 'Extracted as expected in class 2';

package Classname_3;
use Class::Std::Utils;

my %args3 = extract_initializers_from(\%hash);

::is_deeply \%hash, \%orig_hash        => 'Original args unchanged in class 2';
::is_deeply \%args3, \%hash            => 'Extracted as expected in class 3';


package Bad;
use Class::Std::Utils;

::ok !eval { extract_initializers_from(\%hash); 1 } 
                                       => 'Exception for bad specification';

::like $@, qr/^Bad initializer must be a nested hash/
                                       => 'Correct exception thrown';

