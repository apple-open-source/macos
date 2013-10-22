use warnings;
use strict;

use 5.008001;
use inc::Module::Install 1.00;

use Getopt::Long();

my $getopt = Getopt::Long::Parser->new(
  config => [qw/gnu_getopt bundling_override no_ignore_case pass_through/]
);
my $args = {
  skip_author_deps => undef,
};
$getopt->getoptions($args, 'skip_author_deps');
if (@ARGV) {
  warn "\nIgnoring unrecognized option(s): @ARGV\n\n";
}

use FindBin;
use lib "$FindBin::Bin/lib";

perl_version '5.008001';
name           'DBIx-Class-Schema-Loader';
all_from       'lib/DBIx/Class/Schema/Loader.pm';

test_requires 'Test::Exception';
test_requires 'Test::More'    => '0.94';
test_requires 'Test::Warn'    => '0.21';
test_requires 'DBI'           => '1.56';
test_requires 'DBD::SQLite'   => '1.29';
test_requires 'File::Copy';
test_requires 'File::Temp'    => '0.16';
test_requires 'File::Path'    => '2.07';
test_requires 'IPC::Open3'    => 0;
test_requires 'DBIx::Class::IntrospectableM2M' => 0;

requires 'File::Spec'                  => 0;
requires 'Scalar::Util'                => 0;
requires 'Task::Weaken'                => 0;
requires 'Data::Dump'                  => '1.06';
requires 'Lingua::EN::Inflect::Number' => '1.1';
requires 'Lingua::EN::Tagger'          => 0.20;
requires 'Lingua::EN::Inflect::Phrase' => '0.02';
requires 'Digest::MD5'                 => '2.36';
requires 'Class::Accessor::Grouped'    => '0.10002';
requires 'MRO::Compat'                 => '0.09';
requires 'Sub::Name'                   => 0;
requires 'Class::C3::Componentised'    => '1.0008';
requires 'Carp::Clan'                  => 0;
requires 'Class::Inspector'            => '1.27';
requires 'DBIx::Class'                 => '0.08127';
requires 'Class::Unload'               => 0;
requires 'List::MoreUtils'             => 0;
requires 'namespace::clean'            => '0.20';
requires 'Scope::Guard'                => 0;
requires 'Exporter'                    => '5.63';
requires 'Try::Tiny'                   => 0;
requires 'String::ToIdentifier::EN'    => '0.05';
requires 'String::CamelCase'           => '0.02';
requires 'Hash::Merge'                 => 0;

print <<"EOF";
******************* DBIx::Class::Schema::Loader WARNING ***********************
The default attributes for belongs_to relationships for foreign keys with no
rules has been changed for most databases, and is soon changing for the rest,
as ON DELETE/UPDATE and DEFERRABLE clauses for foreign keys are now being
introspected.

THIS MAY AFFECT YOUR DDL DIFFS WHEN DEPLOYING

YOUR GENERATED CODE WILL ALMOST CERTAINLY CHANGE

Read more about the changes in "relationship_attrs" in:

perldoc DBIx::Class::Schema::Loader::Base

https://metacpan.org/module/DBIx::Class::Schema::Loader::Base#relationship_attrs

See also the "Changes" file for the last few revisions.
*******************************************************************************
EOF

if ($Module::Install::AUTHOR && ! $args->{skip_author_deps}) {
    eval { require Module::Install::ReadmeFromPod }
      or die "\nYou need Module::Install::ReadmeFromPod installed to run this Makefile.PL in author mode:\n\n$@\n";

    warn "\n*** AUTHOR MODE: some optional dependencies converted to hard requires.\n\n";

    require DBIx::Class::Schema::Loader::Optional::Dependencies;

    test_requires map %$_, values %{ DBIx::Class::Schema::Loader::Optional::Dependencies->req_group_list };

    DBIx::Class::Schema::Loader::Optional::Dependencies->_gen_pod;

    readme_from( 'lib/DBIx/Class/Schema/Loader.pm' );
    realclean_files( qw[README MANIFEST lib/DBIx/Class/Schema/Loader/Optional/Dependencies.pod] );
}

tests_recursive;

install_script 'script/dbicdump';

resources 'IRC'         => 'irc://irc.perl.org/#dbix-class';
resources 'license'     => 'http://dev.perl.org/licenses/';
resources 'repository'  => 'git://git.shadowcat.co.uk/dbsrgits/DBIx-Class-Schema-Loader.git';
resources 'MailingList' => 'http://lists.scsys.co.uk/cgi-bin/mailman/listinfo/dbix-class';

auto_install;
WriteAll;

# vim:et sts=4 sw=4 tw=0:
