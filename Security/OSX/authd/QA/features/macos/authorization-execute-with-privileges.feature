@macos @smoke
Feature: AEWP - Authorization Execute With Privileges
  "AEWP" provides a way to run any executable with administrator or root privileges.
  'AuthorizationExecuteWithPrivileges' function is deprecated, but a lot of 3rd party installers are still depending on it's functionality.

  Background:
    # sudo ffctl SecurityAgent/NotarizationDetection=on 
    Given the notarization feature flag is enabled
    And there is at least one administrator user account
    And the administrator has biometry enrolled
    And the administrator is loged in on the machine

  @automatable @core @5m
  Scenario: AEWP - AppleScript - notarization warning
    Given I open Terminal
    # osascript -e 'do shell script "whoami" with administrator privileges'
    When I run AppleScript with administrator privileges
    Then I'm presented with administrator access warning
    And I can cancel the script execution by tapping Cancel button in the dialog

  @automatable @core @5m
  Scenario: AEWP - AppleScript
    Given I open Terminal
    # osascript -e 'do shell script "whoami" with administrator privileges'
    And I run AppleScript with administrator privileges
    And I'm presented with administrator access warning
    And I tap on Continue
    And I'm prompted with Authorization password dialogue
    When I enter correct credentials
    # You should see output of "whoami" command if the command from above was used
    Then I get correct output from the command

  @notautomatable @5m
  Scenario: AEWP - notarized third-party Apps biometric authorization
    # Run 'appendix' (AppleInternal) - download'Reason' installer
    # Reason does not have an installer, instead you copy it Application folder. It asks for AEWP when you start the app. The app is notarized
    Given I run a notarized app that asks for AEWP
    And I'm prompted for admin Authorization with biometry
    When I provide administrator's biometry
    Then installation finishes succesfully

  @notautomatable @5m
  Scenario: AEWP - notarized third-party Apps password authorization
    # Run 'appendix' (AppleInternal) - download'Reason' installer
    # Reason does not have an installer, instead you copy it Application folder. It asks for AEWP when you start the app. The app is notarized
    Given I run a notarized app that asks for AEWP
    And I'm prompted for admin Authorization with biometry
    And I tap on Use Password button
    And I'm prompted for admin Authorization with password
    When I enter correct credentials
    Then installation finishes succesfully

  @notautomatable @5m
  Scenario Outline: AEWP - non-notarized third-party Apps
    # Run 'appendix' (AppleInternal) - download 'Maya'
    # Maya asks for AEWP during it's installation. The installer is not notarized
    Given I run a non-notarized app that asks for AEWP
    And I'm presented with administrator access warning
    And I'm prompted for admin Authorization
    And I tap on Continue
    When I enter correct credentials
    Then installation finishes succesfully
