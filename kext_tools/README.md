# Testing kcinstall:

This project does not have automated CI coverage.
All tests should be run locally before nominating changes to main.

## Test Overview:

### test_kcinstall_kcache_e2e**

    This test performs a complete end-to-end test using kcinstall.
    It swaps out the existing kernel collection with a different variant of the same train and build.

### test_kcinstall_kcache_personalize_*

    These tests test personalization for different device models.


## Manual Testing Instructions For kcinstall:

  1.) Open "test_kcinstall.sh" and locate the "TestCases" array. Un-comment the
      "test_kcinstall_kcache_e2e" test case.

  2.) Connect your iPhone, iPad or AppleWatch device to your host system.

  3.) In "test_kcinstall.sh", set the LOCATIONID variable to the location ID of
      the device you are testing. "usbterm -list" can be used to find this value.

  4.) Run "./test_kcinstall.sh" to begin executing the tests.
