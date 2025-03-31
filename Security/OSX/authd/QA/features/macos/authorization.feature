@automatable @macos @smoke
Feature: Authorization in MacOS
  'Authorization' is part of 'Security' framework. It has a set of rights and rules then allow/decline access to the user.
  These rules and rights are stored in "Authorization Database (authdb)"
  It relies on the Security Agent or LocalAuthentication as an interface layer and a way how to interact with user.

  Background:
    Given there is at least one administrator user account
    And there is at least one standard user account

  @core @3m
  Scenario Outline: Authorization in System Preferences - Incorrect/empty credentials
    Given I open <syspref> in System Preferences
    And I click the lock
    When I enter incorrect/empty user credentials
    Then the sheet shakes and lock is still locked

    Examples:
      | syspref            |
      | Users & Groups     |
      | Security & Privacy |
      | Date & Time        |
      | Startup Disk       |

  @core @3m
  Scenario Outline: Authorization in System Preferences - Correct credentials
    Given I open <syspref> in System Preferences
    And I click the lock
    When I enter admin user credentials
    Then the lock is unlocked

    Examples:
      | syspref            |
      | Users & Groups     |
      | Security & Privacy |
      | Date & Time        |
      | Startup Disk       |

  @core @3m
  Scenario Outline: Authorization in System Preferences - Standard User
    Given I open <syspref> in System Preferences
    And I click the lock
    When I enter standard (non-admin) user credentials
    Then the sheet shakes and lock is still locked

    Examples:
      | syspref            |
      | Users & Groups     |
      | Security & Privacy |
      | Date & Time        |
      | Startup Disk       |

  @core @3m
  Scenario: Authorization using root account - Incorrect/empty credentials
    # root account can be enabled in Directory Utility -> Edit in top menu, Enable Root User - Directory Utility must be unlocked
    # Alternative is to call `dsenableroot` in Terminal
    Given root account is enabled
    And I open Users & Groups in System Preferences
    And I click the lock
    When I enter root as username, but incorrect/empty password
    Then the sheet shakes and lock is still locked

  @core @3m
  Scenario: Authorization using root account - Correct credentials
    # root account can be enabled in Directory Utility -> Edit in top menu, Enable Root User - Directory Utility must be unlocked
    # Alternative is to call `dsenableroot` in Terminal
    Given root account is enabled
    And I open Users & Groups in System Preferences
    And I click the lock
    When I enter root user credentials
    Then the lock is unlocked

  @3m
  Scenario: Change Certificate Trust settings in Keychain Access
    Given I launch Keychain Access
    And I change Trust Settings for any Certificate
    When I enter admin user credentials
    Then the Trust Settings for the certificate is changed

  @3m
  Scenario: Authorization in MacOS installer
    Given I start MacOS Assistant installer
    And I follow the steps from installer
    And I'm prompted for credentials
    When I enter admin user credentials
    Then the installation continues
