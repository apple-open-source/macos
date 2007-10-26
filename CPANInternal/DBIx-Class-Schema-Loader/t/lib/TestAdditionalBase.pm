package TestAdditionalBase;

sub test_additional_base { return "test_additional_base"; }
sub test_additional_base_override { return "test_additional_base_override"; }
sub test_additional_base_additional { return TestAdditional->test_additional; }

1;
