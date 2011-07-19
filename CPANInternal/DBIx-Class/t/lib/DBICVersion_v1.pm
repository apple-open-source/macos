package DBICVersion::Table;

use base 'DBIx::Class::Core';
use strict;
use warnings;

__PACKAGE__->table('TestVersion');

__PACKAGE__->add_columns
    ( 'Version' => {
        'data_type' => 'INTEGER',
        'is_auto_increment' => 1,
        'default_value' => undef,
        'is_foreign_key' => 0,
        'is_nullable' => 0,
        'size' => ''
        },
      'VersionName' => {
        'data_type' => 'VARCHAR',
        'is_auto_increment' => 0,
        'default_value' => undef,
        'is_foreign_key' => 0,
        'is_nullable' => 0,
        'size' => '10'
        },
      );

__PACKAGE__->set_primary_key('Version');

package DBICVersion::Schema;
use base 'DBIx::Class::Schema';
use strict;
use warnings;

our $VERSION = '1.0';

__PACKAGE__->register_class('Table', 'DBICVersion::Table');
__PACKAGE__->load_components('+DBIx::Class::Schema::Versioned');

sub upgrade_directory
{
    return 't/var/';
}

sub ordered_schema_versions {
  return('1.0','2.0','3.0');
}

1;
