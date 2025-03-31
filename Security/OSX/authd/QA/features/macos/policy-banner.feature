@notautomatable @macos @smoke @core
Feature: Policy Banner shows info and terms of usage of the station before users/guests log in

  @5m
  Scenario: Policy Banner is shown after reboot before user is allowed to login
    # PolicyBanner must be stored in /Library/Security - can be .txt, .rtf or .rtfd file type
    Given 'Policy Banner' file is in place
    When I reboot the device
    Then I'm presented with the 'Policy Banner' before I'm allowed to login to the station

  @5m
  Scenario: Policy Banner is shown after loging out from the station
    # PolicyBanner must be stored in /Library/Security - can be .txt, .rtf or .rtfd file type
    Given 'Policy Banner' file is in place
    When I log out
    Then I'm presented with the 'Policy Banner' before I'm allowed to login to the station

  @5m
  Scenario Outline: Policy Banner is NOT shown after being deleted
    # PolicyBanner must be stored in /Library/Security - can be .txt, .rtf or .rtfd file type
    Given 'Policy Banner' file is in place
    And I delete 'Policy Banner' file
    When I do <action>
    Then I'm NOT presented with the 'Policy Banner' before I'm allowed to login to the station

    Examples:
      | action  |
      | reboot  |
      | log out |

  @5m
  Scenario: Policy Banner is shown only in FVUnlock when FileVault is enabled
    # PolicyBanner must be stored in /Library/Security - can be .txt, .rtf or .rtfd file type
    Given 'Policy Banner' file is in place
    # To get Policy Banner to Preboot you have to manualy sync first:
    #   $ diskutil apfs updatePreboot /
    And I sync 'Policy Banner' to 'Preboot'
    And 'FileVault' is turned on
    When I reboot the device
    # Policy Banner shouldn't appear again after logging in FVUnlock
    Then I'm presented with the 'Policy Banner' before I'm allowed to login in FVUnlock
