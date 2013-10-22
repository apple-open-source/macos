#!perl

use strict;
use warnings;

use Test::More;

BEGIN {
    use DBIx::Class::Schema::Loader::Optional::Dependencies ();
    if (DBIx::Class::Schema::Loader::Optional::Dependencies->req_ok_for('test_pod')) {
        Test::Pod->import;
    }
    else {
        plan skip_all => 'Tests needs ' . DBIx::Class::Schema::Loader::Optional::Dependencies->req_missing_for('test_pod')
    }
}

all_pod_files_ok();

# vim:tw=0 sw=4 et sts=4:
