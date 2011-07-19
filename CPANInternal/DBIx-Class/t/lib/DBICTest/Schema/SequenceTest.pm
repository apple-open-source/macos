package # hide from PAUSE 
    DBICTest::Schema::SequenceTest;

use base qw/DBICTest::BaseResult/;

__PACKAGE__->table('sequence_test');
__PACKAGE__->source_info({
    "source_info_key_A" => "source_info_value_A",
    "source_info_key_B" => "source_info_value_B",
    "source_info_key_C" => "source_info_value_C",
    "source_info_key_D" => "source_info_value_D",
});
__PACKAGE__->add_columns(
  'pkid1' => {
    data_type => 'integer',
    auto_nextval => 1,
    sequence => 'pkid1_seq',
  },
  'pkid2' => {
    data_type => 'integer',
    auto_nextval => 1,
    sequence => 'pkid2_seq',
  },
  'nonpkid' => {
    data_type => 'integer',
    auto_nextval => 1,
    sequence => 'nonpkid_seq',
  },
  'name' => {
    data_type => 'varchar',
    size      => 100,
    is_nullable => 1,
  },
);
__PACKAGE__->set_primary_key('pkid1', 'pkid2');

1;
