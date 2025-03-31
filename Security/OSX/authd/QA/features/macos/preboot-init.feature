@notautomatable @macos
Feature: Authorization daemon read data about users and preferences from Preboot partition for any further Authorization or Authentication in Recovery (AIR) operation

  @smoke @core @5m
  Scenario: Authorize during Install Assistant in RecoveryOS
    Given I boot to Recovery
    And I open the terminal
    # Best approach seems to be to have external drive with Install macOS <Release Name>.app
    # Then start the installation from terminal:
    #  $ /Volumes/<external>/Install\ macOS \Monterey.app/Contents/MacOS/InstallAssistant
    And I start the Install Assistant
    And I go through assistant to step where you choose partition
    And I select the partition
    # AIR - Authentication in Recovery
    And I'm prompted for credentials by AIR UI
    When I enter admin credentials
    Then the installation continues

  @smoke @core @5m
  Scenario: Change security settings in Startup Security Utility
    Given I boot to Recovery
    And I start Startup Security Utillity from upper panel
    And I change security settings
    When I provide correct admin credentials
    Then security settings are changed

  @smoke @core @5m
  Scenario: Authenticate in FVUnlock - Login
    Given I enable FileVault
    And I boot to FVUnlock
    When I provide correct user credentials
    Then I get logged in to the system

  @5m
  Scenario: Only relevant Preboot partitions are processed during initialization in FVUnlock
    Given I enable FileVault
    When I reboot to FVUnlock
    # There should be none to one 'Mount preboot volume <name>' log since relevant Preboot partition is usually mounted before authd does it
    # $ log show --info --debug --predicate "subsystem='com.apple.Authorization'" | grep 'Mount Preboot Volume'
    Then only relevant Preboot partitions are processed

  @smoke @core @5m
  Scenario: Authenticate in Recovery - Disable SIP
    Given I boot to Recovery
    And I open terminal
    # SIP is disabled by calling 'csrutil disable'
    And I disable SIP
    When I provide correct admin credentials
    # Verify SIP status by calling 'csrutil status'
    Then SIP is disabled

  @5m
  Scenario: Disabling SIP must work when two systems share the same container
    Given I have two MacOS installations on external drive
    And I boot to Recovery
    And I open terminal
    # SIP is disabled by calling 'csrutil disable'
    And I disable SIP
    And I choose volume from the external drive
    When I provide correct admin credentials
    # Verify SIP status by calling 'csrutil status'
    Then SIP is disabled

  @5m
  Scenario: Authentication in Recovery unmounts all Preboot volumes but keeps the one relevant to the Recovery session mounted
    # Relevant Preboot partition is the one in the same volume group as the System/Data partitions of the MacOS which I booted to the Recovery
    # Easiest way to achieve multiple Preboots is to have extended drive with MacOS installed on it plugged to the device
    Given I have more then one Preboot partition on the device
    And I boot to Recovery
    # Find relevant Preboot with 'diskutil apfs list' - then 'diskutil unmount force diskXsY'
    And I unmount relevant Preboot partition
    # Verify SIP status by calling 'csrutil status'
    When I disable SIP
    Then only relevant Preboot partition is mounted after the operation is done
