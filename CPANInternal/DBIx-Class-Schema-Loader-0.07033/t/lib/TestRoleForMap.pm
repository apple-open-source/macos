package TestRoleForMap;

use Moose::Role;

requires qw/id dat meta/; # in loader_test2

sub test_role_for_map_method { 'test_role_for_map_method works' }

1;
