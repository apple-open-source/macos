@macOS @FVUnlock @Recovery
Feature: Authorization daemon read data about users and preferences from Preboot partition for any further Authorization or Authentication in Recovery (AIR) operation

  @FVUnlock @AS
  Scenario: Only relevant Preboot partitions are processed during initialization in FVUnlock 
    When I boot to FVUnlock
    # There should be none to one 'Mount preboot volume <name>' log since relevant Preboot partition is usually mounted earlier
    Then only relevant Preboot partitions are processed

  @Recovery @AS @T2
  Scenario: Authenticate in Recovery - Disable SIP
    Given I boot to Recovery
    And I open terminal
    # SIP is disabled by calling 'csrutil disable'
    And I disable SIP
    When I provide correct admin credentials
    # SIP status can be checked by calling 'csrutil status'
    Then SIP is disabled

  @Recovery @AS @T2
  Scenario: Change security settings in Startup Security Utility
    Given I boot to Recovery
    And I start Startup Security Utillity from upper panel
    And I change security settings 
    When I provide correct admin credentials
    Then security settings are changed

  @Recovery @AS @external_bootable
  Scenario: Disabling SIP must work when two systems share the same container
    Given I have two MacOS installations on external drive
    And I boot to Recovery
    And I open terminal
    # SIP is disabled by calling 'csrutil disable'
    And I disable SIP
    And I choose volume from the external drive
    When I provide correct admin credentials
    # SIP status can be checked by calling 'csrutil status'   
    Then SIP is disabled

  @FVUnlock @AS
  Scenario: Authenticate in FVUnlock - Login
    Given I boot to FVUnlock
    When I provide correct user credentials
    Then I get logged in to the system
  
 
